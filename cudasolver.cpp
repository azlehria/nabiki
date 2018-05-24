#include <cmath>
#include <cuda_runtime_api.h>
#include "cudasolver.h"

// don't put this in the header . . .
#include "miner_state.h"

using namespace std::chrono;

CUDASolver::CUDASolver( int32_t const device, double const intensity ) noexcept :
m_start( steady_clock::now() ),
m_stop( false ),
m_stopped( false ),
m_new_target( true ),
m_new_message( true ),
m_hash_count( 0u ),
m_hash_count_samples( 0u ),
m_hash_average( 0 ),
m_intensity( intensity <= 41.99 ? intensity : 41.99 ),
m_threads( static_cast<uint64_t>(std::pow( 2, intensity <= 41.99 ? intensity : 41.99 )) ),
m_gpu_initialized( false ),
m_device( device ),
m_grid( 1u ),
m_block( 1u )
{
  nvmlInit();
  char busId[13];
  cudaDeviceGetPCIBusId( busId, 13, device );
  nvmlDeviceGetHandleByPciBusId( busId, &m_nvml_handle );

  char t_name[256u];
  nvmlDeviceGetName( m_nvml_handle, t_name, 256u );
  m_name = std::string( t_name );

  m_run_thread = std::thread( &CUDASolver::findSolution, this );
}

CUDASolver::~CUDASolver()
{
  stopFinding();
  while( !m_stopped || !m_run_thread.joinable() )
  {
    std::this_thread::sleep_for( 1ms );
  }
  cudaCleanup();
  nvmlShutdown();
  m_run_thread.join();
}

auto CUDASolver::getHashrate() const -> double const
{
  return m_hash_average / 1000;
}

auto CUDASolver::getTemperature() const -> uint32_t const
{
  uint32_t t_temp;
  nvmlDeviceGetTemperature( m_nvml_handle, NVML_TEMPERATURE_GPU, &t_temp );
  return t_temp;
}

auto CUDASolver::getDeviceState() const -> device_info_t const
{
  uint32_t t_core;
  nvmlDeviceGetClockInfo( m_nvml_handle, NVML_CLOCK_GRAPHICS, &t_core );
  uint32_t t_memory;
  nvmlDeviceGetClockInfo( m_nvml_handle, NVML_CLOCK_MEM, &t_memory );
  uint32_t t_power;
  nvmlDeviceGetPowerUsage( m_nvml_handle, &t_power );
  uint32_t t_temp;
  nvmlDeviceGetTemperature( m_nvml_handle, NVML_TEMPERATURE_GPU, &t_temp );
  uint32_t t_fan;
  nvmlDeviceGetFanSpeed( m_nvml_handle, &t_fan );

  return { m_name, t_core, t_memory, t_power / 1000, t_temp, t_fan, m_hash_average };
}

auto CUDASolver::updateTarget() -> void
{
  m_new_target = true;
}

auto CUDASolver::updateMessage() -> void
{
  m_new_message = true;
}

auto CUDASolver::stopFinding() -> void
{
  m_stop = true;
}

auto CUDASolver::getNextSearchSpace() -> uint64_t const
{
  m_hash_count += m_threads;
  double t{ static_cast<double>(duration_cast<milliseconds>(steady_clock::now() - m_start).count()) / 1000 };

  if( m_hash_count_samples < 100 )
  {
    ++m_hash_count_samples;
  }

  double temp_average{ m_hash_average };
  temp_average += ((m_hash_count / t) / 1000 - temp_average) / m_hash_count_samples;
  if( std::isnan( temp_average ) || std::isinf( temp_average ) )
  {
    temp_average = m_hash_average;
  }
  m_hash_average = temp_average;
  return MinerState::getIncSearchSpace( m_threads );
}

auto CUDASolver::getTarget() const -> uint64_t const
{
  return MinerState::getTargetNum();
}

auto CUDASolver::getMidstate() const -> state_t const
{
  return MinerState::getMidstate();
}

auto CUDASolver::pushSolutions() const -> void
{
  for( uint_fast32_t i{ 0 }; i < *h_solution_count; ++i )
    MinerState::pushSolution( h_solutions[i] );
}
