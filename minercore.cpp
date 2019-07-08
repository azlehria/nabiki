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

#include "minercore.h"
#include "miner_state.h"
#include "log.h"
#include "platforms.h"
#include "commo.h"
#include "isolver.h"
#include "cpusolver.h"
#include "cudasolver.h"
#include "clsolver.h"
#include "telemetry.h"
#include "ui.h"

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
  static std::vector<std::shared_ptr<ISolver>> m_solvers;

  static uint_fast16_t m_solvers_cuda{ 0u };
  static uint_fast16_t m_solvers_cpu{ 0u };
  static uint_fast16_t m_solvers_cl{ 0u };
  static steady_clock::time_point m_launch_time;

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

    ss_out << "."sv;

    Log::pushLog( ss_out.str() );
  }

  static auto createMiners() -> void
  {
    MinerState::waitUntilReady();

    for( auto const&[ device, intensity ] : MinerState::getCudaDevices() )
    {
      m_solvers.push_back( std::make_shared<CUDASolver>( device, intensity ) );
      ++m_solvers_cuda;
    }

    for( m_solvers_cpu = 0; m_solvers_cpu < MinerState::getCpuThreads(); ++m_solvers_cpu )
    {
      m_solvers.push_back( std::make_shared<CPUSolver>() );
    }

    Opencl cl{};
    if( !cl.Flush ) { return; }

    cl_int error{ 0 };
    std::vector<cl_platform_id> platforms;
    {
      cl_uint plat_count{ 0 };
      error = cl.GetPlatformIDs( 0u, nullptr, &plat_count );
      platforms.resize( plat_count );
      error = cl.GetPlatformIDs( plat_count, platforms.data(), nullptr );
    }
    if( error != CL_SUCCESS )
    {
      Log::pushLog( "OpenCL error: " + std::to_string( error ) );
    }
    for( auto const&[ pName, pDevices ] : MinerState::getClDevices() )
    {
      for( auto const& plat : platforms )
      {
        std::string plat_name;
        {
          size_t name_size{ 0 };
          cl.GetPlatformInfo( plat, CL_PLATFORM_NAME, 0u, nullptr, &name_size );
          char* name{ new char[name_size] };
          cl.GetPlatformInfo( plat, CL_PLATFORM_NAME, name_size, name, nullptr );
          plat_name = name;
          delete[] name;
        }
        if( (pName == "intel"s && plat_name.find( "Intel"s ) == std::string::npos)
          || (pName == "nvidia"s && plat_name.find( "NVIDIA"s ) == std::string::npos)
          || (pName == "amd"s && plat_name.find( "AMD"s ) == std::string::npos) ) { continue; }

        cl_uint device_count{ 0 };
        cl.GetDeviceIDs( plat, CL_DEVICE_TYPE_ALL, 0u, nullptr, &device_count );
        auto devices{ new cl_device_id[device_count] };
        cl.GetDeviceIDs( plat, CL_DEVICE_TYPE_ALL, device_count, devices, nullptr );

        for( auto const&[ device, intensity ] : pDevices )
        {
          m_solvers.push_back( std::make_shared<CLSolver>( devices[size_t(device)], intensity ) );
          ++m_solvers_cl;
        }
        delete[] devices;
      }
    }
  }
}

namespace MinerCore
{
  auto updateTarget() -> void
  {
    for( auto const& solver : m_solvers )
    {
      solver->updateTarget();
    }
  }

  auto updateMessage() -> void
  {
    for( auto const& solver : m_solvers )
    {
      solver->updateMessage();
    }
  }

  auto run() -> int32_t
  {
    m_launch_time = steady_clock::now();

    InitBaseState();

    UI::Init();

    MinerState::Init();

    Commo::Init();

    createMiners();

    std::thread uiThread{ UI::Run };

    for( auto const& solver : m_solvers )
    {
      solver->startFinding();
    }

    printStartMessage();

    Telemetry::Init();

    if( uiThread.joinable() )
      uiThread.join();

    CleanupBaseState();

    return 0;
  }

  auto stop() -> void
  {
    UI::Stop();

    for( auto const& solver : m_solvers )
    {
      solver->stopFinding();
    }

    m_solvers_cuda = m_solvers_cpu = 0u;

    Telemetry::Cleanup();

    Commo::Cleanup();
  }

  auto getHashrates() -> std::vector<double> const
  {
    std::vector<double> temp;
    for( auto const& solver : m_solvers )
    {
      temp.emplace_back( solver->getHashrate() );
    }
    return temp;
  }

  auto getDevice( size_t devIndex ) -> ISolver*
  {
    return m_solvers[devIndex].get();
  }

  auto getDeviceReferences() -> std::vector<std::shared_ptr<ISolver>> const
  {
    std::vector<std::shared_ptr<ISolver>> ret;
    for( auto solver : m_solvers )
    {
      ret.emplace_back( solver );
    }
    return ret;
  }

  auto getActiveDeviceCount() -> uint_fast16_t const
  {
    return m_solvers_cuda + m_solvers_cl + m_solvers_cpu;
  }

  auto getUptime() -> int64_t const
  {
    return duration_cast<seconds>( steady_clock::now() - m_launch_time ).count();
  }
}
