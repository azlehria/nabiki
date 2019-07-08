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

#include "telemetry.h"
#include "types.h"
#include "miner_state.h"
#include "minercore.h"
#include "commo.h"

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

#include <json.hpp>
#include "CivetWeb/civetweb.h"

namespace
{
  using json = nlohmann::json;
  using namespace std::string_literals;

  static mg_context* m_ctx;
  static bool m_started{ false };

  static auto api_handler_json( mg_connection* __restrict conn, [[maybe_unused]] void* cbdata ) noexcept -> int32_t
  try
  {
    json body;
    double hashrate{ 0 };
    uint_fast16_t device_id{ 0 };
    for( auto const& device : MinerCore::getDeviceReferences() )
    {
      body["health"].emplace_back( json{ { "name"s, device->getName() },
                                         { "clock"s, device->getClockCore() },
                                         { "mem_clock"s, device->getClockMem() },
                                         { "power"s, device->getPowerWatts() },
                                         { "temp"s, device->getTemperature() },
                                         { "fan"s, device->getFanSpeed() } } );

      body["hashrate"]["threads"][device_id].emplace_back( uint64_t( device->getHashrate() ) );

      hashrate += device->getHashrate();
      ++device_id;
    }

    body["worker_id"] = MinerState::getWorkerName();
    body["version"] = std::string( MinerCore::MINER_VERSION.substr( 8u ) );
    body["kind"] = "nvidia"s;
    body["ua"] = std::string( MinerCore::MINER_VERSION );
    body["algo"] = "keccak256"s;
    body["hashrate"]["total"].emplace_back( uint64_t( hashrate ) );
    body["connection"] = json{ { "pool"s, MinerState::getPoolUrl() },
                               { "uptime"s, MinerCore::getUptime() },
                               { "ping"s, Commo::GetPing() },
                               { "failures"s, Commo::GetConnectionErrorCount() },
                               { "error_log"s, Commo::GetConnectionErrorLog() } };

    body["results"]["diff_current"] = MinerState::getDiff();
    body["results"]["shares_good"] = MinerState::getSolCount();
    body["results"]["shares_total"] = Commo::GetTotalShares();
    //body["results"]["avg_time"] = 0;
    body["results"]["hashes_total"] = MinerState::getIncSearchSpace( 0u );

    std::stringstream ss_out;

    ss_out << "HTTP/1.1 200 OK\r\n"s
           << "Content-Length: "s << body.dump().length() << "\r\n"s
           << "Access-Control-Allow-Origin: *\r\n"s
           << "Content-Type: application/json\r\n"s
           << "Connection: close\r\n\r\n"s
           << body.dump();
    mg_printf( conn, "%s", ss_out.str().c_str() );

    return 200;
  }
  catch( ... ){ return 500; }
}

namespace Telemetry
{
  auto Init() -> void
  {
    if( m_started ) return;

    std::string ports{ MinerState::getTelemetryPorts() };
    if( ports.length() == 0 || ports == "0" ) return;

    mg_init_library( 0u );
    char const* cw_opts[]{ "listening_ports",     ports.c_str(),
                           // currently breaks . . . everything
                           //"access_control_list", MinerState::getTelemetryAcl().c_str(),
                           "request_timeout_ms",  "5000",
                           "num_threads",         "2",
                           0u };

    m_ctx = mg_start( NULL, 0u, cw_opts );
    mg_set_request_handler( m_ctx, "/", api_handler_json, NULL );

    m_started = true;
  }

  auto Cleanup() -> void
  {
    if( !m_started ) return;

    mg_stop( m_ctx );
    mg_exit_library();
  }
}
