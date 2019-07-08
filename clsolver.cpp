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

#include "clsolver.h"
#include "clsolver.cl"
#include "log.h"
#include "miner_state.h"
#include "devicetelemetry.h"

#include <cmath>
#include <iostream>

using namespace std::literals::string_literals;
using namespace std::chrono;

CLSolver::CLSolver( cl_device_id const& device, double const& intensity ) noexcept :
  m_telemetry_handle( Nabiki::MakeDeviceTelemetryObject( device ) ),
  m_stop( false ),
  m_new_target( true ),
  m_new_message( true ),
  m_device_initialized( false ),
  h_solution_count( 0 ),
  h_solutions{},
  temp_time( 0 ),
  temp_average( 0 ),
  m_hash_count( 0 ),
  m_first_round_passed( false ),
  m_hash_average( 0 ),
  m_intensity( intensity <= 41.99 ? intensity : 41.99 ),
  m_threads( intensity == 1 ? 1 : static_cast<uint64_t>(std::pow( 2, m_intensity )) ),
  m_round_start( 0ns ),
  m_round_end( 0ns ),
  m_device( device ),
  m_kernel( nullptr ),
  h_threads( 0 ),
  m_global_work_size( 1u ),
  m_local_work_size( 1u ),
  cl()
{
  cl_int error;

  error = cl.GetDeviceInfo( m_device, CL_DEVICE_PLATFORM, sizeof( cl_platform_id ), &m_platform, nullptr );
  cl_context_properties t_ctx_props[]{ CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(m_platform), CL_PROPERTIES_LIST_END_EXT };
  m_context = cl.CreateContext( t_ctx_props, 1u, &m_device, nullptr, nullptr, &error );
  m_queue = cl.CreateCommandQueue( m_context, m_device, 0u, &error );

  d_solution_count = cl.CreateBuffer( m_context, static_cast<cl_mem_flags>(CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR), sizeof( h_solution_count ), nullptr, &error );
  d_solutions = cl.CreateBuffer( m_context, static_cast<cl_mem_flags>(CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR), sizeof( h_solutions ), nullptr, &error );
  d_mid = cl.CreateBuffer( m_context, static_cast<cl_mem_flags>(CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR), sizeof( state_t ), nullptr, &error );

  const char* src = clKeccakSource.c_str();
  m_program = cl.CreateProgramWithSource( m_context, 1u, &src, nullptr, &error );

  std::string options{ "-cl-std=CL"s };
  {
    size_t version_size{ 0 };
    std::string version;
    cl.GetDeviceInfo( m_device, CL_DEVICE_VERSION, 0u, nullptr, &version_size );
    version.resize( version_size );
    cl.GetDeviceInfo( m_device, CL_DEVICE_VERSION, version_size, version.data(), nullptr );
    options += version.substr( 7, 3 );
  }

  std::string plat_name;
  {
    size_t name_size{ 0 };
    cl.GetPlatformInfo( m_platform, CL_PLATFORM_NAME, 0u, nullptr, &name_size );
    plat_name.resize( name_size );
    cl.GetPlatformInfo( m_platform, CL_PLATFORM_NAME, name_size, plat_name.data(), nullptr );
  }
  if( plat_name.find( "NVIDIA"s ) != std::string::npos )
  {
    cl_uint major_version{ 0 }, minor_version{ 0 };

    error = cl.GetDeviceInfo( m_device, CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV, sizeof( cl_uint ), &major_version, nullptr );
    error = cl.GetDeviceInfo( m_device, CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV, sizeof( cl_uint ), &minor_version, nullptr );

    options += " -DCUDA_VERSION="s + std::to_string( major_version * 10 + minor_version );
  }

  error = cl.BuildProgram( m_program, 1u, &m_device, options.c_str(), nullptr, nullptr );
  if( error != CL_SUCCESS )
  {
    size_t log_size{ 0 };
    std::string log;
    cl.GetProgramBuildInfo( m_program, m_device, CL_PROGRAM_BUILD_LOG, 0u, nullptr, &log_size );
    log.resize( log_size );
    cl.GetProgramBuildInfo( m_program, m_device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr );
    Log::pushLog( "Error building OpenCL kernel: "s + log + "\n"s );
    std::abort();
  }

  m_kernel = cl.CreateKernel( m_program, "cl_mine", &error );

  error = cl.SetKernelArg( m_kernel, 0u, sizeof d_mid, &d_mid );
  error = cl.SetKernelArg( m_kernel, 2u, sizeof d_solutions, &d_solutions );
  error = cl.SetKernelArg( m_kernel, 3u, sizeof d_solution_count, &d_solution_count );

  error = cl.GetKernelWorkGroupInfo( m_kernel, m_device, CL_KERNEL_WORK_GROUP_SIZE, sizeof( m_local_work_size ), &m_local_work_size, nullptr );
  if( error != CL_SUCCESS )
  {
    Log::pushLog( "OpenCL error getting work size: "s + std::to_string( error ) );
  }
  if( intensity > 10 )
  {
    m_threads = (m_threads / m_local_work_size) * m_local_work_size;
  }

  m_global_work_size = m_threads;
  //Log::pushLog( std::to_string( *m_local_work_size ) + ":"s + std::to_string( m_threads ) );

  h_solution_count = 0u;
  error = cl.EnqueueWriteBuffer( m_queue, d_solution_count, CL_FALSE, 0u, sizeof( h_solution_count ), &h_solution_count, 0u, nullptr, nullptr );

  m_device_initialized = true;
}

CLSolver::~CLSolver()
{
  if( m_run_thread.joinable() )
    m_run_thread.join();
}

auto CLSolver::findSolution() -> void
{
  cl_int error;
  uint64_t target{ 0 };

  m_start = steady_clock::now();

  do
  {
    if( m_new_message )
    {
      error = cl.EnqueueWriteBuffer( m_queue, d_mid, CL_FALSE, 0u, sizeof( state_t ), MinerState::getMidstate().data(), 0u, nullptr, nullptr );
      m_new_message = false;
    }
    if( m_new_target )
    {
      target = MinerState::getTargetNum();
      error = cl.SetKernelArg( m_kernel, 1u, sizeof( target ), &target );
      m_new_target = false;
    }

    h_threads = MinerState::getIncSearchSpace( m_threads );
    error = cl.SetKernelArg( m_kernel, 4, sizeof( h_threads ), &h_threads );

    cl.Flush( m_queue );
    error = cl.EnqueueNDRangeKernel( m_queue, m_kernel, 1u, nullptr, &m_global_work_size, &m_local_work_size, 0u, nullptr, nullptr );

    updateHashrate();

    error = cl.EnqueueReadBuffer( m_queue, d_solution_count, CL_TRUE, 0u, sizeof( h_solution_count ), &h_solution_count, 0u, nullptr, nullptr );

    if( error == CL_SUCCESS && h_solution_count > 0u )
    {
      if( cl.EnqueueReadBuffer( m_queue, d_solutions, CL_TRUE, 0u, sizeof( h_solutions[0] ) * h_solution_count, &h_solutions, 0u, nullptr, nullptr ) )
      {
        continue;
      }

      MinerState::pushSolution( std::vector<uint64_t>{ h_solutions, h_solutions + h_solution_count } );
      h_solution_count = 0u;
      error = cl.EnqueueWriteBuffer( m_queue, d_solution_count, CL_FALSE, 0u, sizeof( h_solution_count ), &h_solution_count, 0u, nullptr, nullptr );
    }
  }
  while( !m_stop );

  m_device_initialized = false;
}

auto CLSolver::startFinding() -> void
{
  m_run_thread = std::thread( &CLSolver::findSolution, this );
}
