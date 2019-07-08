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

#include "log.h"
#include "miner_state.h"
#include "cudasolver.h"
#include "cudasolver.fatbin.h"

#include <cstdint>
#include <cmath>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <functional>

using namespace std::chrono;
using namespace std::string_literals;

CUDASolver::CUDASolver( int32_t const& device, double const& intensity ) :
  m_stop( false ),
  m_new_target( true ),
  m_new_message( true ),
  m_device_initialized( false ),
  m_hash_count( 0u ),
  m_working_time( 0u ),
  h_solution_count( 0u ),
  h_solutions(),
  m_intensity( intensity <= 41.99 ? intensity : 41.99 ),
  m_threads( intensity == 1 ? 1 : static_cast<uint64_t>(std::pow( 2, m_intensity )) ),
  m_grid( 1u ),
  m_block( 1024u ),
  d_mid( 0u ),
  d_target( 0u ),
  d_threads( 0u ),
  d_solution_count( 0u ),
  d_solutions( 0u ),
  cu()
{
  if( m_intensity > 5 )
  {
    m_threads -= (m_threads % m_block);
  }

  m_grid = uint32_t( (m_threads + m_block - 1) / m_block );

  cuSafeCall( cu.DeviceGet( &m_device, device ) );
  cuSafeCall( cu.CtxCreate( &m_context, CU_CTX_BLOCKING_SYNC, m_device ) );

  // these are minor optimizations at best
  // it doesn't really matter if they fail or not
  cu.CtxSetLimit( CU_LIMIT_PRINTF_FIFO_SIZE, 0u );
  cu.CtxSetLimit( CU_LIMIT_MALLOC_HEAP_SIZE, 0u );
  cu.CtxSetLimit( CU_LIMIT_DEV_RUNTIME_SYNC_DEPTH, 1u );
  cu.CtxSetLimit( CU_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT, 2u );
  cu.CtxSetLimit( CU_LIMIT_STACK_SIZE, 0u );

  cuSafeCall( cu.StreamCreate( &m_stream, CU_STREAM_NON_BLOCKING ) );

  cuSafeCall( cu.ModuleLoadFatBinary( &m_module, cudaKeccakFatbin ) );
  cuSafeCall( cu.ModuleGetFunction( &m_kernel, m_module, "cuda_mine" ) );

  cuSafeCall( cu.ModuleGetGlobal( &d_mid, nullptr, m_module, "mid" ) );
  cuSafeCall( cu.ModuleGetGlobal( &d_target, nullptr, m_module, "target" ) );
  cuSafeCall( cu.ModuleGetGlobal( &d_threads, nullptr, m_module, "threads" ) );
  cuSafeCall( cu.ModuleGetGlobal( &d_solution_count, nullptr, m_module, "solution_count" ) );
  cuSafeCall( cu.ModuleGetGlobal( &d_solutions, nullptr, m_module, "solutions" ) );

  m_telemetry_handle = Nabiki::MakeDeviceTelemetryObject( m_device );
}

CUDASolver::~CUDASolver()
{
  if( m_run_thread.joinable() )
    m_run_thread.join();

  cuSafeCall( cu.ModuleUnload( m_module ) );
  cuSafeCall( cu.StreamDestroy( m_stream ) );
  cuSafeCall( cu.CtxDestroy( m_context ) );
}

auto CUDASolver::findSolution() -> void
{
  uint64_t t_target{ 0u };
  state_t t_mid{ 0u };

  cuSafeCall( cu.CtxSetCurrent( m_context ) );

  cudaResetSolution();

  m_device_initialized = true;

  do
  {
    if( m_new_target )
    {
      t_target = MinerState::getTargetNum();
      cuSafeCall( cu.MemcpyHtoDAsync( d_target, &t_target, sizeof( t_target ), m_stream ) );

      m_new_target = false;
    }
    if( m_new_message )
    {
      t_mid = MinerState::getMidstate();
      cuSafeCall( cu.MemcpyHtoDAsync( d_mid, &t_mid, sizeof( t_mid ), m_stream ) );

      m_new_message = false;
    }

    h_threads = MinerState::getIncSearchSpace( m_threads );
    cuSafeCall( cu.MemcpyHtoDAsync( d_threads, &h_threads, sizeof( h_threads ), m_stream ) );

    cuSafeCall( cu.LaunchKernel( m_kernel, m_grid, 1u, 1u, m_block, 1u, 1u, 0u, m_stream, nullptr, nullptr ) );

    updateHashrate();

    cuSafeCall( cu.StreamSynchronize( m_stream ) );
    //cuSafeCall( cu.CtxSynchronize() );
    cuSafeCall( cu.MemcpyDtoHAsync( &h_solution_count, d_solution_count, sizeof( h_solution_count ), m_stream ) );

    if( !h_solution_count )
    {
      continue;
    }

    cuSafeCall( cu.MemcpyDtoHAsync( &h_solutions, d_solutions, h_solution_count * sizeof( *h_solutions ), m_stream ) );
    MinerState::pushSolution( std::vector<uint64_t>{ h_solutions, h_solutions + h_solution_count } );
    cudaResetSolution();
  }
  while( !m_stop );

  cuSafeCall( cu.CtxSynchronize() );

  m_device_initialized = false;

  guard lock{ m_hashrate_mutex };
  m_working_time = m_hash_count = 0;
}

auto CUDASolver::startFinding() -> void
{
  if( m_run_thread.joinable() )
    m_run_thread.join();

  m_stop = false;
  m_round_start = steady_clock::now();
  m_run_thread = std::thread( &CUDASolver::findSolution, this );
}

auto CUDASolver::cudaResetSolution() -> void
{
  h_solution_count = 0u;
  cuSafeCall( cu.MemcpyHtoDAsync( d_solution_count, &h_solution_count, sizeof( h_solution_count ), m_stream ) );
}

auto CUDASolver::updateHashrate() -> void
{
  using namespace std::chrono;

  // goofy, yes; but it results in timing the _entire loop_
  m_round_end = steady_clock::now();
  {
    guard lock{ m_hashrate_mutex };
    m_working_time += static_cast<uint64_t>((m_round_end - m_round_start).count());
    m_hash_count += m_threads;
  }
  m_round_start = steady_clock::now();
}
