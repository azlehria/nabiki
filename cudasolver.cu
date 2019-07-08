/*
 * Copyright 2018 Azlehria
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined __INTELLISENSE__
 /* reduce vstudio warnings (__byteperm, blockIdx...) */
#  define __CUDA_ARCH__ 1
#  include <device_launch_parameters.h>
#  undef __CUDA_ARCH__
#  define __CUDACC__ 1
#  include <device_atomic_functions.hpp>
#  undef __CUDACC__
#  include <cstring>
#endif //__INTELLISENSE__

using uint64_t = unsigned long long;

struct __align__(8) uchar8
{
  unsigned char s0, s1, s2, s3, s4, s5, s6, s7;
};

__constant__ uint64_t mid[25]{ 0 };
__constant__ uint64_t target{ 0 };
__constant__ uint64_t threads{ 0 };

__device__ uint64_t solution_count{ 0 };
__device__ uint64_t solutions[256]{ 0 };

__device__
auto ROTL64( uint64_t& output, uint64_t const x, uint64_t const y ) -> void
{
  output = (x << y) ^ (x >> (64u - y));
}

//__device__
//auto bswap_32( uint32_t& input ) -> void
//{
//  uchar4 temp{ reinterpret_cast<uchar4&>(input) };
//  input = temp.x << 24 | temp.y << 16 | temp.z << 8 | temp.w;
//}

__device__
auto bswap_64( uint64_t input ) -> uint64_t
{
  //uint2& t_ref{ reinterpret_cast<uint2&>(input) };
  //uchar4 &tempx{ reinterpret_cast<uchar4&>(t_ref.x) },
  //       tempy{ reinterpret_cast<uchar4&>(t_ref.y) };
  //t_ref.x = tempy.x << 24 | tempy.y << 16 | tempy.z << 8 | tempy.w;
  //t_ref.y = tempx.x << 24 | tempx.y << 16 | tempx.z << 8 | tempx.w;
  //return input;
  uchar8& t_ref{ reinterpret_cast<uchar8&>(input) };
  return uint64_t( t_ref.s0 << 24 | t_ref.s1 << 16 | t_ref.s2 << 8 | t_ref.s3 ) << 32 | t_ref.s4 << 24 | t_ref.s5 << 16 | t_ref.s6 << 8 | t_ref.s7;
}

__device__
auto xor5( uint64_t& output, uint64_t (&s)[25], uint64_t offset ) -> void
{
#if __CUDA_ARCH__
  asm( "{"
       "  xor.b64 %0, %1, %2;"
       "  xor.b64 %0, %0, %3;"
       "  xor.b64 %0, %0, %4;"
       "  xor.b64 %0, %0, %5;"
       "}"
       : "=l"(output)
       : "l"(s[0u + offset]), "l"(s[5u + offset]), "l"(s[10u + offset]), "l"(s[15u + offset]), "l"(s[20u + offset]) );
#else
  output = s[0u + offset] ^ s[5u + offset] ^ s[10u + offset] ^ s[15u + offset] ^ s[20u + offset];
#endif
}

__device__
auto xor3( uint64_t& a, uint64_t const b, uint64_t const c ) -> void
{
  a = a ^ b ^ c;
}

__device__
auto theta_parity( uint64_t (&t)[5], uint64_t (&s)[25] ) -> void
{
  xor5( t[1], s, 0u );
  xor5( t[2], s, 1u );
  xor5( t[3], s, 2u );
  xor5( t[4], s, 3u );
  xor5( t[0], s, 4u );
}

__device__
auto theta_xor( uint64_t (&s)[25], uint64_t offset, uint64_t const t, uint64_t const v ) -> void
{
  uint64_t u;
  ROTL64( u, v, 1u );
  xor3( s[ 0u + offset], u, t );
  xor3( s[ 5u + offset], u, t );
  xor3( s[10u + offset], u, t );
  xor3( s[15u + offset], u, t );
  xor3( s[20u + offset], u, t );
}

__device__
auto theta( uint64_t (&s)[25], uint64_t (&t)[5] ) -> void
{
  theta_parity( t, s );

  theta_xor( s, 0u, t[0], t[2] );
  theta_xor( s, 1u, t[1], t[3] );
  theta_xor( s, 2u, t[2], t[4] );
  theta_xor( s, 3u, t[3], t[0] );
  theta_xor( s, 4u, t[4], t[1] );
}

__device__
auto chi_group( uint64_t (&s)[25], uint64_t offset, uint64_t (&t)[5] ) -> void
{
  s[0u + offset] = t[0] ^ ~t[1] & t[2];
  s[1u + offset] = t[1] ^ ~t[2] & t[3];
  s[2u + offset] = t[2] ^ ~t[3] & t[4];
  s[3u + offset] = t[3] ^ ~t[4] & t[0];
  s[4u + offset] = t[4] ^ ~t[0] & t[1];
}

__device__
auto chi( uint64_t (&s)[25], uint64_t offset, uint64_t (&t)[5] ) -> void
{
  t[0] = s[0u + offset];
  t[1] = s[1u + offset];
  t[2] = s[2u + offset];
  t[3] = s[3u + offset];
  t[4] = s[4u + offset];
  chi_group( s, offset, t );
}

__device__
auto keccak_round( uint64_t (&s)[25], uint64_t (&t)[5], uint64_t const rc ) -> void
{
  theta( s, t );

  t[0] = s[1];
  ROTL64( s[ 1], s[ 6], 44u );
  ROTL64( s[ 6], s[ 9], 20u );
  ROTL64( s[ 9], s[22], 61u );
  ROTL64( s[22], s[14], 39u );
  ROTL64( s[14], s[20], 18u );
  ROTL64( s[20], s[ 2], 62u );
  ROTL64( s[ 2], s[12], 43u );
  ROTL64( s[12], s[13], 25u );
  ROTL64( s[13], s[19],  8u );
  ROTL64( s[19], s[23], 56u );
  ROTL64( s[23], s[15], 41u );
  ROTL64( s[15], s[ 4], 27u );
  ROTL64( s[ 4], s[24], 14u );
  ROTL64( s[24], s[21],  2u );
  ROTL64( s[21], s[ 8], 55u );
  ROTL64( s[ 8], s[16], 45u );
  ROTL64( s[16], s[ 5], 36u );
  ROTL64( s[ 5], s[ 3], 28u );
  ROTL64( s[ 3], s[18], 21u );
  ROTL64( s[18], s[17], 15u );
  ROTL64( s[17], s[11], 10u );
  ROTL64( s[11], s[ 7],  6u );
  ROTL64( s[ 7], s[10],  3u );
  ROTL64( s[10], t[ 0],  1u );

  chi( s,  0u, t );
  chi( s,  5u, t );
  chi( s, 10u, t );
  chi( s, 15u, t );
  chi( s, 20u, t );

  s[0] ^= rc;
}

__device__
auto keccak_first( uint64_t (&s)[25], uint64_t (&t)[5] ) -> void
{
  memcpy( s, mid, sizeof( s ) );
  uint64_t D{ 0u };
  uint64_t const nounce{ threads + (blockDim.x * blockIdx.x + threadIdx.x) };
  ROTL64( D, nounce, 44u ); // 44
  s[2] ^= D;
  ROTL64( D, nounce, 14u ); // 14
  s[4] ^= D;
  ROTL64( D, nounce, 20u ); // 20
  s[6] ^= D;
  ROTL64( D, nounce, 62u ); // 62
  s[9] ^= D;
  ROTL64( D, nounce, 7u ); // 7
  s[11] ^= D;
  ROTL64( D, nounce, 8u ); // 8
  s[13] ^= D;
  ROTL64( D, nounce, 27u ); // 27
  s[15] ^= D;
  ROTL64( D, nounce, 16u ); // 16
  s[18] ^= D;
  ROTL64( D, nounce, 63u ); //63
  s[20] ^= D;
  ROTL64( D, nounce, 55u ); // 55
  s[21] ^= D;
  ROTL64( D, nounce, 39u ); // 39
  s[22] ^= D;

  chi( s,  0u, t );
  chi( s,  5u, t );
  chi( s, 10u, t );
  chi( s, 15u, t );
  chi( s, 20u, t );

  s[0] ^= 0x0000000000000001ull;
}

extern "C"
__global__
void cuda_mine()
{
  uint64_t state[25], C[5];

  keccak_first( state, C );

  keccak_round( state, C, 0x0000000000008082uLL );
  keccak_round( state, C, 0x800000000000808auLL );
  keccak_round( state, C, 0x8000000080008000uLL );
  keccak_round( state, C, 0x000000000000808buLL );
  keccak_round( state, C, 0x0000000080000001uLL );
  keccak_round( state, C, 0x8000000080008081uLL );
  keccak_round( state, C, 0x8000000000008009uLL );
  keccak_round( state, C, 0x000000000000008auLL );
  keccak_round( state, C, 0x0000000000000088uLL );
  keccak_round( state, C, 0x0000000080008009uLL );
  keccak_round( state, C, 0x000000008000000auLL );
  keccak_round( state, C, 0x000000008000808buLL );
  keccak_round( state, C, 0x800000000000008buLL );
  keccak_round( state, C, 0x8000000000008089uLL );
  keccak_round( state, C, 0x8000000000008003uLL );
  keccak_round( state, C, 0x8000000000008002uLL );
  keccak_round( state, C, 0x8000000000000080uLL );
  keccak_round( state, C, 0x000000000000800auLL );
  keccak_round( state, C, 0x800000008000000auLL );
  keccak_round( state, C, 0x8000000080008081uLL );
  keccak_round( state, C, 0x8000000000008080uLL );
  keccak_round( state, C, 0x0000000080000001uLL );

  theta_parity( C, state );

  ROTL64( state[1], C[2], 1u );
  xor3( state[ 0], state[1], C[0] );
  ROTL64( state[1], C[3], 1u );
  xor3( state[ 6], state[1], C[1] );
  ROTL64( state[1], C[4], 1u );
  xor3( state[12], state[1], C[2] );

  ROTL64( state[2], state[6], 44u );
  ROTL64( state[3], state[12], 43u );

  state[1] = state[0] ^ ~state[2] & state[3];
  state[1] ^= 0x8000000080008008uLL;

  //if( reinterpret_cast<uint2&>(state[0]).x ) return;
  //bswap_64( state[0] );
  if( bswap_64( state[1] ) > target ) return;

  uint64_t cIdx{ atomicAdd( &solution_count, 1u ) };
  if( cIdx >= 256u ) return;

  solutions[cIdx] = threads + (blockDim.x * blockIdx.x + threadIdx.x);
}

// --------------------------------------------------------------------

//using vec64 = union
//{
//private:
//  uint64_t raw_;
//public:
//  struct
//  { uint32_t x, y; };
//  operator uint64_t()
//  { return raw_; }
//  operator uint2()
//  { return { x, y }; }
//  uint64_t operator=( uint64_t rt )
//  { return raw_ = rt; }
//  uint2 operator=( uint2 rt )
//  { return { x = rt.x, y = rt.y }; }
//};
