/*
Author: Mikers
date march 4, 2018 for 0xbitcoin dev

based off of https://github.com/Dunhili/SHA3-gpu-brute-force-cracker/blob/master/sha3.cu

 * Author: Brian Bowden
 * Date: 5/12/14
 *
 * This is the parallel version of SHA-3.
 */

#include <iostream>
#include <cstring>
#include "cudasolver.h"

#ifdef __INTELLISENSE__
 /* reduce vstudio warnings (__byteperm, blockIdx...) */
#  include <device_functions.h>
#  include <device_launch_parameters.h>
#  include <cuda_runtime.h>
#  include <cuda.h>
#endif //__INTELLISENSE__

#define cudaSafeCall(err) __cudaSafeCall(err, __FILE__, __LINE__, m_device)

__host__ inline
auto __cudaSafeCall( cudaError_t err, char const* file, int32_t const line, int32_t device_id ) -> void
{
#ifndef CUDA_NDEBUG
  if (cudaSuccess != err) {
    std::cerr << "CUDA device " << device_id
              << " encountered an asynchronous error in file '" << file
              << "' in line " << line
              << " : " << cudaGetErrorString( err ) << ".\n";
    cudaError_t syncErr = cudaGetLastError();
    if( syncErr )
    {
      std::cerr << "Synchronous error " << syncErr << ":"
                << cudaGetErrorString( syncErr ) << " was also encountered.\n";
    }
    exit( EXIT_FAILURE );
  }
#endif
}

#if __CUDA_ARCH__ < 350
__device__ __constant__ const uint64_t RC[24] = {
  0x0000000000008082, 0x800000000000808a, 0x8000000080008000,
  0x000000000000808b, 0x0000000080000001, 0x8000000080008081,
  0x8000000000008009, 0x000000000000008a, 0x0000000000000088,
  0x0000000080008009, 0x000000008000000a, 0x000000008000808b,
  0x800000000000008b, 0x8000000000008089, 0x8000000000008003,
  0x8000000000008002, 0x8000000000000080, 0x000000000000800a,
  0x800000008000000a, 0x8000000080008081, 0x8000000000008080,
  0x0000000080000001
};
#endif
__constant__ uint64_t d_mid[25];
__constant__ uint64_t d_target;

__device__ __forceinline__
auto ROTL64( uint64_t& output, uint64_t const x, uint32_t const y ) -> void
{
  output = (x << y) ^ (x >> (64 - y));
}

__device__ __forceinline__
auto bswap_64( uint64_t const input ) -> uint64_t const
{
  uint64_t output;
  asm( "{"
       "  prmt.b32 %0, %3, 0, 0x0123;"
       "  prmt.b32 %1, %2, 0, 0x0123;"
       "}" : "=r"(reinterpret_cast<uint2&>(output).x), "=r"(reinterpret_cast<uint2&>(output).y)
           : "r"(reinterpret_cast<uint2 const&>(input).x), "r"(reinterpret_cast<uint2 const&>(input).y) );
  return output;
}

__device__ __forceinline__
auto xor5( uint64_t& output, uint64_t const* const s ) -> void
{
  asm( "{"
       "  xor.b64 %0, %1, %2;"
       "  xor.b64 %0, %0, %3;"
       "  xor.b64 %0, %0, %4;"
       "  xor.b64 %0, %0, %5;"
       "}" : "=l"( output ) : "l"( s[0] ), "l"( s[5] ), "l"( s[10] ), "l"( s[15] ), "l"( s[20] ) );
}

__device__ __forceinline__
auto xor3( uint64_t& a, uint64_t const b, uint64_t const c ) -> void
{
#if __CUDA_ARCH__ >= 500
  asm( "{"
       "  lop3.b32 %0, %0, %2, %4, 0x96;"
       "  lop3.b32 %1, %1, %3, %5, 0x96;"
       "}" : "+r"(reinterpret_cast<uint2&>(a).x), "+r"(reinterpret_cast<uint2&>(a).y)
           : "r"(reinterpret_cast<uint2 const&>(b).x), "r"(reinterpret_cast<uint2 const&>(b).y),
             "r"(reinterpret_cast<uint2 const&>(c).x), "r"(reinterpret_cast<uint2 const&>(c).y) );
#else
  asm( "{"
       "  xor.b64 %0, %0, %1;"
       "  xor.b64 %0, %0, %2;"
       "}" : "+l"(a) : "l"(b), "l"(c) );
#endif
}

__device__ __forceinline__
auto theta_parity( uint64_t* __restrict__ t, uint64_t const* const __restrict__ s ) -> void
{
  xor5( t[1], &s[0] );
  xor5( t[2], &s[1] );
  xor5( t[3], &s[2] );
  xor5( t[4], &s[3] );
  xor5( t[0], &s[4] );
}

__device__ __forceinline__
auto theta_xor( uint64_t* __restrict__ s, uint64_t const t, uint64_t& u, uint64_t const v ) -> void
{
  ROTL64( u, v, 1 );
  xor3( s[ 0], u, t );
  xor3( s[ 5], u, t );
  xor3( s[10], u, t );
  xor3( s[15], u, t );
  xor3( s[20], u, t );
}

__device__ __forceinline__
auto theta( uint64_t* __restrict__ s, uint64_t* __restrict__ t, uint64_t* __restrict__ u ) -> void
{
  theta_parity( t, s );

  theta_xor( &s[0], t[0], u[0], t[2] );
  theta_xor( &s[1], t[1], u[1], t[3] );
  theta_xor( &s[2], t[2], u[2], t[4] );
  theta_xor( &s[3], t[3], u[3], t[0] );
  theta_xor( &s[4], t[4], u[4], t[1] );
}

__device__ __forceinline__
auto chi_single( uint64_t& output, uint64_t const a, uint64_t const b, uint64_t const c ) -> void
{
#if __CUDA_ARCH__ >= 500
  asm( "{"
       "  lop3.b32 %0, %2, %4, %6, 0xD2;"
       "  lop3.b32 %1, %3, %5, %7, 0xD2;"
       "}" : "+r"(reinterpret_cast<uint2&>(output).x), "+r"(reinterpret_cast<uint2&>(output).y)
           : "r"(reinterpret_cast<uint2 const&>(a).x), "r"(reinterpret_cast<uint2 const&>(a).y),
             "r"(reinterpret_cast<uint2 const&>(b).x), "r"(reinterpret_cast<uint2 const&>(b).y),
             "r"(reinterpret_cast<uint2 const&>(c).x), "r"(reinterpret_cast<uint2 const&>(c).y) );
#else
  output = a ^ ((~b) & c);
#endif
}

__device__ __forceinline__
auto chi_group( uint64_t* __restrict__ s, uint64_t const* const __restrict__ t ) -> void
{
  chi_single( s[0], t[0], t[1], t[2] );
  chi_single( s[1], t[1], t[2], t[3] );
  chi_single( s[2], t[2], t[3], t[4] );
  chi_single( s[3], t[3], t[4], t[0] );
  chi_single( s[4], t[4], t[0], t[1] );
}

__device__ __forceinline__
auto chi( uint64_t* __restrict__ s, uint64_t* __restrict__ t ) -> void
{
  t[0] = s[0];
  t[1] = s[1];
  t[2] = s[2];
  t[3] = s[3];
  t[4] = s[4];
  chi_group( s, t );
}

__device__ __forceinline__
auto keccak_round( uint64_t* __restrict__ s, uint64_t* __restrict__ t, uint64_t* __restrict__ u, uint64_t const rc ) -> void
{
  theta( s, t, u );
  
  t[0] = s[1];
  ROTL64( s[ 1], s[ 6], 44 );
  ROTL64( s[ 6], s[ 9], 20 );
  ROTL64( s[ 9], s[22], 61 );
  ROTL64( s[22], s[14], 39 );
  ROTL64( s[14], s[20], 18 );
  ROTL64( s[20], s[ 2], 62 );
  ROTL64( s[ 2], s[12], 43 );
  ROTL64( s[12], s[13], 25 );
  ROTL64( s[13], s[19],  8 );
  ROTL64( s[19], s[23], 56 );
  ROTL64( s[23], s[15], 41 );
  ROTL64( s[15], s[ 4], 27 );
  ROTL64( s[ 4], s[24], 14 );
  ROTL64( s[24], s[21],  2 );
  ROTL64( s[21], s[ 8], 55 );
  ROTL64( s[ 8], s[16], 45 );
  ROTL64( s[16], s[ 5], 36 );
  ROTL64( s[ 5], s[ 3], 28 );
  ROTL64( s[ 3], s[18], 21 );
  ROTL64( s[18], s[17], 15 );
  ROTL64( s[17], s[11], 10 );
  ROTL64( s[11], s[ 7],  6 );
  ROTL64( s[ 7], s[10],  3 );
  ROTL64( s[10], t[ 0],  1 );

  chi( &s[ 0], t );
  chi( &s[ 5], t );
  chi( &s[10], t );
  chi( &s[15], t );
  chi( &s[20], t );

  s[0] ^= rc;
}

__device__ __forceinline__
auto keccak_first( uint64_t* __restrict__ s, uint64_t* __restrict__ t, uint64_t const nounce ) -> void
{
  uint64_t n[11]{ 0 };
  ROTL64( n[ 0], nounce, 7 );
  ROTL64( n[ 1], n[0],  1 );
  ROTL64( n[ 2], n[1],  6 );
  ROTL64( n[ 3], n[2],  2 );
  ROTL64( n[ 4], n[3],  4 );
  ROTL64( n[ 5], n[4],  7 );
  ROTL64( n[ 6], n[5], 12 );
  ROTL64( n[ 7], n[6],  5 );
  ROTL64( n[ 8], n[7], 11 );
  ROTL64( n[ 9], n[8],  7 );
  ROTL64( n[10], n[9],  1 );

  t[0] = d_mid[0];
  t[1] = d_mid[1];
  t[2] = d_mid[2] ^ n[7];
  t[3] = d_mid[3];
  t[4] = d_mid[4] ^ n[2];
  chi_group( &s[0], t );
  s[0] ^= 0x0000000000000001ull;

  t[0] = d_mid[5];
  t[1] = d_mid[6] ^ n[4];
  t[2] = d_mid[7];
  t[3] = d_mid[8];
  t[4] = d_mid[9] ^ n[9];
  chi_group( &s[5], t );

  t[0] = d_mid[10];
  t[1] = d_mid[11] ^ n[0];
  t[2] = d_mid[12];
  t[3] = d_mid[13] ^ n[1];
  t[4] = d_mid[14];
  chi_group( &s[10], t );

  t[0] = d_mid[15] ^ n[5];
  t[1] = d_mid[16];
  t[2] = d_mid[17];
  t[3] = d_mid[18] ^ n[3];
  t[4] = d_mid[19];
  chi_group( &s[15], t );

  t[0] = d_mid[20] ^ n[10];
  t[1] = d_mid[21] ^ n[8];
  t[2] = d_mid[22] ^ n[6];
  t[3] = d_mid[23];
  t[4] = d_mid[24];
  chi_group( &s[20], t );
}
__global__
void cuda_mine( uint64_t* __restrict__ solution, uint32_t* __restrict__ solution_count, uint64_t const threads )
{
  uint64_t const nounce{ threads + (blockDim.x * blockIdx.x + threadIdx.x) };

  uint64_t state[25], C[5], D[5];

  keccak_first( state, C, nounce );

#if __CUDA_ARCH__ >= 350
  keccak_round( state, C, D, 0x0000000000008082ull );
  keccak_round( state, C, D, 0x800000000000808aull );
  keccak_round( state, C, D, 0x8000000080008000ull );
  keccak_round( state, C, D, 0x000000000000808bull );
  keccak_round( state, C, D, 0x0000000080000001ull );
  keccak_round( state, C, D, 0x8000000080008081ull );
  keccak_round( state, C, D, 0x8000000000008009ull );
  keccak_round( state, C, D, 0x000000000000008aull );
  keccak_round( state, C, D, 0x0000000000000088ull );
  keccak_round( state, C, D, 0x0000000080008009ull );
  keccak_round( state, C, D, 0x000000008000000aull );
  keccak_round( state, C, D, 0x000000008000808bull );
  keccak_round( state, C, D, 0x800000000000008bull );
  keccak_round( state, C, D, 0x8000000000008089ull );
  keccak_round( state, C, D, 0x8000000000008003ull );
  keccak_round( state, C, D, 0x8000000000008002ull );
  keccak_round( state, C, D, 0x8000000000000080ull );
  keccak_round( state, C, D, 0x000000000000800aull );
  keccak_round( state, C, D, 0x800000008000000aull );
  keccak_round( state, C, D, 0x8000000080008081ull );
  keccak_round( state, C, D, 0x8000000000008080ull );
  keccak_round( state, C, D, 0x0000000080000001ull );
#else
  for( int32_t i = 0; i < 22; ++i )
  {
    keccak_round( state, C, D, RC[i] );
  }
#endif // __CUDA_ARCH__ >= 350

  theta_parity( C, state );

  ROTL64( D[0], C[2], 1 );
  ROTL64( D[1], C[3], 1 );
  ROTL64( D[2], C[4], 1 );

  xor3( state[ 0], D[0], C[0] );
  xor3( state[ 6], D[1], C[1] );
  xor3( state[12], D[2], C[2] );
  ROTL64( state[ 6], state[ 6], 44 );
  ROTL64( state[12], state[12], 43 );

  chi_single( state[0], state[0], state[6], state[12] );
  state[0] ^= 0x8000000080008008ull;

  if( bswap_64( state[0] ) <= d_target )
  {
    uint64_t cIdx{ atomicAdd( solution_count, 1 ) };
    if( cIdx >= 256 ) return;

    solution[cIdx] = nounce;
  }
}

// --------------------------------------------------------------------

auto CUDASolver::cudaInit() -> void
{
  cudaSetDevice( m_device );

  cudaDeviceProp device_prop;
  cudaSafeCall( cudaGetDeviceProperties( &device_prop, m_device ) );

  int32_t compute_version{ device_prop.major * 100 + device_prop.minor * 10 };

  if( compute_version <= 500 )
  {
    m_intensity = m_intensity <= 40.55 ? m_intensity : 40.55;
    m_threads = static_cast<uint64_t>( std::pow( 2, m_intensity <= 40.55 ? m_intensity : 40.55 ) );
  }

  m_block.x = compute_version > 500 ? TPB50 : TPB35;
  m_grid.x = uint32_t((m_threads + m_block.x - 1) / m_block.x);

  if( !m_gpu_initialized )
  {
    // CPU usage goes _insane_ without this.
    cudaSafeCall( cudaDeviceReset() );
    // so we don't actually _use_ L1 or local memory . . .
    cudaSafeCall( cudaSetDeviceFlags( cudaDeviceScheduleBlockingSync ) );

    cudaSafeCall( cudaMalloc( reinterpret_cast<void**>(&d_solution_count), 4 ) );
    cudaSafeCall( cudaMallocHost( reinterpret_cast<void**>(&h_solution_count), 4 ) );
    cudaSafeCall( cudaMalloc( reinterpret_cast<void**>(&d_solutions), 256*8 ) );
    cudaSafeCall( cudaMallocHost( reinterpret_cast<void**>(&h_solutions), 256*8 ) );

    cudaResetSolution();

    m_gpu_initialized = true;
  }
}

auto CUDASolver::cudaCleanup() -> void
{
  cudaSafeCall( cudaSetDevice( m_device ) );

  cudaSafeCall( cudaThreadSynchronize() );

  cudaSafeCall( cudaFree( d_solution_count ) );
  cudaSafeCall( cudaFreeHost( h_solution_count ) );
  cudaSafeCall( cudaFree( d_solutions ) );
  cudaSafeCall( cudaFreeHost( h_solutions ) );

  cudaSafeCall( cudaDeviceReset() );

  m_gpu_initialized = false;
}

auto CUDASolver::cudaResetSolution() -> void
{
  cudaSetDevice( m_device );

  std::memset( h_solution_count, 0u, 4 );
  cudaSafeCall( cudaMemset( d_solution_count, 0u, 4 ) );
}

auto CUDASolver::pushTarget() -> void
{
  cudaSetDevice( m_device );

  uint64_t target{ getTarget() };
  cudaSafeCall( cudaMemcpyToSymbol( d_target, &target, 8, 0, cudaMemcpyHostToDevice) );

  m_new_target = false;
}

auto CUDASolver::pushMessage() -> void
{
  cudaSetDevice( m_device );

  cudaSafeCall( cudaMemcpyToSymbol( d_mid, getMidstate().data(), 200, 0, cudaMemcpyHostToDevice) );

  m_new_message = false;
}

auto CUDASolver::findSolution() -> void
{
  cudaInit();

  cudaSetDevice( m_device );

  do
  {
    if( m_new_target ) { pushTarget(); }
    if( m_new_message ) { pushMessage(); }

    cuda_mine <<< m_grid, m_block >>> ( d_solutions, d_solution_count, getNextSearchSpace() );
    cudaError_t syncErr = cudaGetLastError();
    cudaError_t asyncErr = cudaDeviceSynchronize();
    if( syncErr | asyncErr != cudaSuccess )
    {
      if( syncErr )
      {
        std::cerr << "Kernel launch encountered synchronous error "
                  << syncErr
                  << ": \x1b[38;5;196m"
                  << cudaGetErrorString( syncErr )
                  << ".\x1b[0m\n"
                  << "Check your hardware configuration.\n";
      }
      if( asyncErr )
      {
        std::cerr << "Kernel launch encountered asynchronous error "
                  << asyncErr
                  << ": \x1b[38;5;196m"
                  << cudaGetErrorString( asyncErr )
                  << ".\x1b[0m\n"
                  << "Check your hardware configuration.\n";
      }
      exit( EXIT_FAILURE );
    }

    cudaSafeCall( cudaMemcpy( h_solution_count, d_solution_count, 4, cudaMemcpyDeviceToHost ) );

    if( *h_solution_count )
    {
      cudaSafeCall( cudaMemcpy( h_solutions, d_solutions, (*h_solution_count)*8, cudaMemcpyDeviceToHost ) );
      pushSolutions();
      cudaResetSolution();
    }
  } while( !m_stop );

  m_stopped = true;
}
