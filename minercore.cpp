#include "miner_state.h"
#include "platforms.h"
#include "hybridminer.h"
#include "commo.h"
#include "isolver.h"
#include "cpusolver.h"
#include "cudasolver.h"
#include "clsolver.h"
#include "telemetry.h"
#include "text.h"

#include <cstdlib>
#include <sstream>
#include <memory>
#include <chrono>
#include <thread>
#include <string>
#include <string_view>

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;
using namespace std::chrono;

namespace
{
  static std::vector<std::unique_ptr<ISolver>> m_solvers;

  static uint_fast16_t m_solvers_cuda{ 0u };
  static uint_fast16_t m_solvers_cpu{ 0u };
  static uint_fast16_t m_solvers_cl{ 0u };
  static steady_clock::time_point m_launch_time;
  static std::atomic<bool> m_stop{ false };

  static auto printStartMessage() -> void
  {
    std::stringstream ss_out;
    ss_out <<  "Mining on "sv;
    if( m_solvers_cuda > 0u )
    {
      ss_out << m_solvers_cuda
             << " GPU"sv << (m_solvers_cuda > 1 ? "s"sv : ""sv) << " using CUDA"sv;
    }
    if( m_solvers_cuda > 0u && m_solvers_cl > 0u )
    {
      ss_out << " and"sv << (m_solvers_cpu > 0u ? ", "sv : " "sv);
    }
    if( m_solvers_cl > 0u )
    {
      ss_out << m_solvers_cl
             << " device"sv << ( m_solvers_cl > 1 ? "s"sv : ""sv ) << " using OpenCL"sv;
    }
    if( m_solvers_cl > 0u && m_solvers_cpu > 0u )
    {
      ss_out << (m_solvers_cuda > 0u ? ", "sv : " "sv) << "and "sv;
    }
    if( m_solvers_cpu > 0u )
    {
      ss_out << m_solvers_cpu << " CPU core"sv << (m_solvers_cpu > 1 ? "s"sv : ""sv);
    }

    ss_out << ".\n"sv;

    MinerState::pushLog( ss_out.str() );
  }

  static auto startMining() -> void
  {
    while( !MinerState::isReady() )
    {
      std::this_thread::sleep_for( 1ms );
    }

    for( auto const&[ device, intensity ] : MinerState::getCudaDevices() )
    {
      m_solvers.push_back( std::make_unique<CUDASolver>( device, intensity ) );
      ++m_solvers_cuda;
    }

    for( m_solvers_cpu = 0; m_solvers_cpu < MinerState::getCpuThreads(); ++m_solvers_cpu )
    {
      m_solvers.push_back( std::make_unique<CPUSolver>() );
    }

    //std::vector<cl::Platform> platforms;
    //cl_int error = cl::Platform::get( &platforms );
    //for( auto const&[ pName, pDevices ] : MinerState::getClDevices() )
    //{
    //  std::vector<cl::Device> devices;
    //  error = platforms[1].getDevices( CL_DEVICE_TYPE_ALL, &devices );

    //  m_solvers.push_back( std::make_unique<CLSolver>( devices[0], 17 ) );
    //  ++m_solvers_cl;
    //}
  }
}

namespace HybridMiner
{
  auto updateTarget() -> void
  {
    for( auto&& solver : m_solvers )
    {
      solver->updateTarget();
    }
  }

  auto updateMessage() -> void
  {
    for( auto&& solver : m_solvers )
    {
      solver->updateMessage();
    }
  }

  // This is the "main" thread of execution
  auto run() -> void
  {
    m_launch_time = steady_clock::now();

    SetBasicState();

    MinerState::Init();

    Commo::Init();

    startMining();

    printStartMessage();

    Telemetry::Init();

    UI::Text::Init();

    do {
      std::this_thread::sleep_for( 1ms );
    } while( !m_stop.load( std::memory_order_acquire ) );

    UI::Text::Cleanup();

    std::cerr << MinerState::getPrintableTimeStamp() << "Process exiting... stopping miner\n"sv;

    if( !UseOldUI() )
    {
      std::cerr << "\x1b[s\x1b[?25h\x1b[r\x1b[u"sv;
    }

    for( auto& solver : m_solvers )
    {
      solver->stopFinding();
    }

    if( m_solvers_cuda )
      nvmlShutdown();

    m_solvers_cuda = m_solvers_cpu = 0u;

    Telemetry::Cleanup();

    Commo::Cleanup();
  }

  auto stop() -> void
  {
    m_stop.store( true, std::memory_order_release );
  }

  auto getHashrates() -> std::vector<double> const
  {
    std::vector<double> temp;
    for( auto&& solver : m_solvers )
    {
      temp.emplace_back( solver->getHashrate() );
    }
    return temp;
  }

  auto getTemperatures() -> std::vector<uint32_t> const
  {
    std::vector<uint32_t> temp;
    for( auto&& solver : m_solvers )
    {
      temp.emplace_back( solver->getTemperature() );
    }
    return temp;
  }

  auto getDeviceStates() -> std::vector<device_info_t> const
  {
    std::vector<device_info_t> ret;
    for( auto&& solver : m_solvers )
    {
      device_info_t temp{ solver->getDeviceState() };
      if( !temp.name.empty() )
      {
        ret.emplace_back( temp );
      }
    }
    return ret;
  }

  auto getActiveDeviceCount() -> uint_fast16_t const
  {
    return m_solvers_cuda + m_solvers_cl + m_solvers_cpu;
  }

  auto getUptime() -> uint64_t const
  {
    return duration_cast<seconds>( steady_clock::now() - m_launch_time ).count();
  }
}
