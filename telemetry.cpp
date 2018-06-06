#include "telemetry.h"
#include "types.h"
#include "miner_state.h"
#include "hybridminer.h"
#include "commo.h"

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

#include "json.hpp"
#include "CivetWeb/civetweb.h"

namespace
{
  static const nlohmann::json foo = { 0 };
  static struct mg_context* m_ctx;
  static bool m_started{ false };

  static int32_t api_handler_json( struct mg_connection* __restrict conn, void* )
  {
    nlohmann::json body;
    double hashrate{ 0 };
    uint_fast16_t device_id{ 0 };
    for( const auto& device : HybridMiner::getDeviceStates() )
    {
      body["health"].emplace_back( nlohmann::json{ { "name", device.name },
                                                   { "clock", device.core },
                                                   { "mem_clock", device.memory },
                                                   { "power", device.power },
                                                   { "temp", device.temperature },
                                                   { "fan", device.fan } } );

      body["hashrate"]["threads"][device_id].emplace_back( uint64_t( device.hashrate ) );

      hashrate += device.hashrate;
      ++device_id;
    }

    body["version"] = std::string( MINER_VERSION.substr( 8u ) );
    body["kind"] = "nvidia"s;
    body["ua"] = std::string( MINER_VERSION );
    body["algo"] = "keccak256"s;
    body["hashrate"]["total"].emplace_back( uint64_t( hashrate ) );
    body["connection"] = nlohmann::json{ { "pool", MinerState::getPoolUrl() },
                                         { "uptime", HybridMiner::getUptime() },
                                         { "ping", Commo::GetPing() },
                                         { "failures", Commo::GetConnectionErrorCount() },
                                         { "error_log", Commo::GetConnectionErrorLog() } };

    body["results"]["diff_current"] = MinerState::getDiff();
    body["results"]["shares_good"] = MinerState::getSolCount();
    body["results"]["shares_total"] = Commo::GetTotalShares();
    //body["results"]["avg_time"] = 0;
    body["results"]["hashes_total"] = MinerState::getIncSearchSpace( 0u );

    std::stringstream ss_out;

    ss_out << "HTTP/1.1 200 OK\r\n"
           << "Content-Length: " << body.dump().length() << "\r\n"
           << "Access-Control-Allow-Origin: *\r\n"
           << "Content-Type: application/json\r\n"
           << "Connection: close\r\n\r\n"
           << body.dump();
    mg_printf( conn, "%s", ss_out.str().c_str() );

    return 200;
  }
}

namespace Telemetry
{
  auto init() -> void
  {
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

  auto cleanup() -> void
  {
    if( !m_started ) return;

    mg_stop( m_ctx );
    mg_exit_library();
  }
}
