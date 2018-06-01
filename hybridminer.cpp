#include "miner_state.h"
#include "platforms.h"
#include "hybridminer.h"
#include "commo.h"
#include "isolver.h"
#include "cpusolver.h"
#include "cudasolver.h"
#include "clsolver.h"
#include "telemetry.h"

#include <cstdlib>
#include <sstream>
#include <memory>
#include <chrono>
#include <thread>
#include <string>
#include <string_view>
#include <nvml.h>

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;
using namespace std::chrono;

namespace
{
  static std::vector<std::unique_ptr<ISolver>> m_solvers;

  static uint_fast16_t m_solvers_cuda{ 0u };
  static uint_fast16_t m_solvers_cpu{ 0u };
  static uint_fast16_t m_solvers_cl{ 0u };
  static std::atomic<bool> m_stop{ false };

  static auto printUiBase() -> void
  {
    if( !UseOldUI() )
    {
      uint_fast16_t logTop{ 5u + (MinerState::isDebug() ? m_solvers_cuda : 0u) };

      std::cout << "\x1b[?25l\x1b[2J\x1b(0"sv
                << "\x1b[1;1flqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqk"sv
                << "\x1b[4;1fmqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqj"sv
                << "\x1b[2;1fx\x1b[2;35fx\x1b[2;62fx\x1b[2;80fx"sv
                << "\x1b[3;1fx\x1b[3;35fx\x1b[3;62fx\x1b[3;80fx"sv
                << "\x1b(B\x1b[2;2fChallenge:"sv
                << "\x1b[3;2fDifficulty:"sv
                << "\x1b[2;37fHashes this round"sv
                << "\x1b[2;63fRound time:"sv
                << "\x1b[3;63fAccount:"sv
                << "\x1b[2;31fMH/s"sv
                << "\x1b[3;31fSols"sv
                << "\x1b[s\x1b[3;29f\x1b[38;5;221m0\x1b[0m\x1b[u"sv
                << "\x1b[1;64f"sv << MINER_VERSION.substr( 7 )
                << "\x1b]2;"sv << MINER_VERSION << "\x07"sv
                << "\x1b[" << logTop << "r\x1b[" << logTop << ";1f"sv;
    }

    std::stringstream ss_out;
    ss_out << MinerState::getPrintableTimeStamp() << "Mining on "sv;
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

    std::cout << ss_out.str();
  }

  static auto startMining() -> void
  {
    while( !MinerState::isReady() )
    {
      std::this_thread::sleep_for( 1ms );
    }

    for( auto& device : MinerState::getCudaDevices() )
    {
      nvmlInit();
      m_solvers.push_back( std::make_unique<CUDASolver>( device.first,
                                                         device.second ) );
      ++m_solvers_cuda;
    }

    for( m_solvers_cpu = 0; m_solvers_cpu < MinerState::getCpuThreads(); ++m_solvers_cpu )
    {
      m_solvers.push_back( std::make_unique<CPUSolver>() );
    }

    std::vector<cl::Platform> platforms;
    cl_int error = cl::Platform::get( &platforms );
    for( auto& cfg_plats : MinerState::getClDevices() )
    {
      std::vector<cl::Device> devices;
      error = platforms[1].getDevices( CL_DEVICE_TYPE_ALL, &devices );

      m_solvers.push_back( std::make_unique<CLSolver>( devices[0], 17 ) );
      ++m_solvers_cl;
    }
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
    SetBasicState();

    MinerState::initState();

    Commo::Init();

    startMining();

    printUiBase();

    Telemetry::init();

    std::thread printer{ [] {
    do
    {
      auto timerNext = steady_clock::now() + 100ms;

      MinerState::printStatus();

      std::this_thread::sleep_until( timerNext );
    } while( !m_stop ); } };

    printer.join();

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

    Telemetry::cleanup();

    Commo::Cleanup();
  }

  auto stop() -> void
  {
    m_stop = true;
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
}
