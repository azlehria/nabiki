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

#include "commo.h"
#include "log.h"
#include "utils.h"
#include "miner_state.h"
#include "types.h"
#include "minercore.h"
#include "ui.h"
#include "BigInt/BigIntegerLibrary.hh"
#include <json.hpp>
#include "sph_keccak.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <stdexcept>
#include <curl/curl.h>

using namespace std::string_literals;
using namespace std::chrono;
using json = nlohmann::json;
using namespace Nabiki::Utils;

namespace
{
  using CURLhandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
  using CURLslist = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
  static auto writebackHandler( char* __restrict body, size_t size, size_t nmemb, void* __restrict out ) noexcept -> size_t const;

  static uint_fast64_t solutionCount{ 0ull };
  static uint_fast64_t devfeeCount{ 0ull };
  static std::atomic<uint_fast64_t> failureCount{ 0ull };
  static std::vector<std::string> connectionErrors;
  static std::mutex connectionMutex;
  static std::atomic<uint_fast64_t> totalCount{ 0ull };
  static std::atomic<double> m_ping{ 0. };
  static std::atomic<bool> m_stop{ false };
  static sph_keccak256_context ctx;
  static bool m_started{ false };
  static message_t keccak_data;
  static hash_t keccak_result;

  static std::array<char, CURL_ERROR_SIZE> m_errstr{ 0 };
  static CURLslist m_headers{ curl_slist_append( NULL, "Content-Type: application/json" ), &curl_slist_free_all };
  static CURLhandle m_handle{ [] {
    curl_global_init( CURL_GLOBAL_DEFAULT );
    CURL* tHandle{ curl_easy_init() };

    curl_easy_setopt( tHandle, CURLOPT_NOSIGNAL, 1 );

    curl_easy_setopt( tHandle, CURLOPT_HTTPHEADER, m_headers.get() );

    curl_easy_setopt( tHandle, CURLOPT_ERRORBUFFER, m_errstr.data() );

    curl_easy_setopt( tHandle, CURLOPT_WRITEFUNCTION, writebackHandler );
    curl_easy_setopt( tHandle, CURLOPT_CONNECTTIMEOUT, 20 );
    curl_easy_setopt( tHandle, CURLOPT_TIMEOUT, 20 );
    return tHandle;
  }(), &curl_easy_cleanup };

  static std::thread m_thread;

  static json const m_get_address{ { "jsonrpc"s, "2.0"s }, { "method"s, "getPoolEthAddress"s }, { "id"s, "addr"s } };
  static json const m_get_challenge{ { "jsonrpc"s, "2.0"s }, { "method"s, "getChallengeNumber"s }, { "id"s, "chal"s } };
  static json m_get_diff{ { "jsonrpc"s, "2.0"s }, { "method"s, "getMinimumShareDifficulty"s }, { "params"s, {} }, { "id"s, "diff"s } };
  static json m_get_target{ { "jsonrpc"s, "2.0"s }, { "method"s, "getMinimumShareTarget"s }, { "params"s, {} }, { "id"s, "tar"s } };
  static json const m_solution_base{ { "jsonrpc"s, "2.0"s }, { "method"s, "submitShare"s }, { "params"s, {} }, { "id"s, {} } };

  static auto keccak256( std::string_view const message ) -> std::string
  {
    hexToBytes( message, keccak_data );
    sph_keccak256( &ctx, keccak_data.data(), keccak_data.size() );
    sph_keccak256_close( &ctx, keccak_result.data() );
    return bytesToString( keccak_result );
  }

  static auto logConnectionError( std::string error ) -> void
  {
    failureCount.fetch_add( 1, std::memory_order_release );
    Log::pushLog( error );
    {
      guard lock( connectionMutex );
      connectionErrors.emplace_back( error );
    }
  }

  static auto writebackHandler( char* __restrict body, size_t size, size_t nmemb, void* __restrict out ) noexcept -> size_t const
  {
    try
    {
      static_cast<std::string*>(out)->append( body, size * nmemb );
    }
    catch( ... )
    {
      return 0;
    }
    return size * nmemb;
  }

  static auto doMethod( json& request ) -> CURLcode
  {
    std::string response{};
    std::string body{ request.dump() };

    curl_easy_setopt( m_handle.get(), CURLOPT_POSTFIELDS, body.c_str() );
    curl_easy_setopt( m_handle.get(), CURLOPT_POSTFIELDSIZE, body.length() );
    curl_easy_setopt( m_handle.get(), CURLOPT_WRITEDATA, &response );

    CURLcode errcode{ curl_easy_perform( m_handle.get() ) };

    if( errcode == CURLE_OK )
    {
      // Ubuntu doesn't have 7.61 on a LTS release yet
      curl_easy_getinfo( m_handle.get(), CURLINFO_TOTAL_TIME, &m_ping );
      request = json::parse( response.cbegin(), response.cend(), nullptr, false );
      return errcode;
    }
    logConnectionError( m_errstr.size() ? m_errstr.data() : curl_easy_strerror( errcode ) );

    switch( errcode )
    {
      case CURLE_FAILED_INIT:
        Log::pushLog( "Fatal error: libcURL failed to initialize."s );
        std::abort();
        break;
      case CURLE_UNSUPPORTED_PROTOCOL: [[fallthrough]] ;
      case CURLE_URL_MALFORMAT:
        Log::pushLog( "Fatal error: pool URL malformed." );
        std::abort();
        break;
      case CURLE_OUT_OF_MEMORY:
        Log::pushLog( "Fatal error: libcURL could not allocate memory."s );
        std::abort();
        break;
      case CURLE_COULDNT_RESOLVE_HOST: [[fallthrough]] ;
      case CURLE_COULDNT_CONNECT: [[fallthrough]] ;
      case CURLE_HTTP_POST_ERROR: [[fallthrough]] ;
      case CURLE_TOO_MANY_REDIRECTS: [[fallthrough]] ;
      case CURLE_GOT_NOTHING: [[fallthrough]] ;
      case CURLE_SEND_ERROR: [[fallthrough]] ;
      case CURLE_RECV_ERROR:
        break;
      default:
        ; // do nothing
    }

    return errcode;
  }

  static auto updateState( bool const full = false ) -> void
  {
    json request;

    request.push_back( m_get_challenge );

    if( !MinerState::getCustomDiff() )
    {
      request.push_back( m_get_diff );
    }

    if( full )
    {
      request.push_back( m_get_address );
    }

    // need better (ANY) error handling here or in doMethod
    if( doMethod( request ) )
    {
      return;
    }

    std::string foo{ request.dump() };
    for( auto const& ret : request )
    {
      // these tests need to be false for all valid responses, so . . .
      if( ret.find( "id" ) == ret.end() || !ret["id"].is_string() ||
          ret.find( "result" ) == ret.end() )
      {
        continue;
      }
      if( ret["id"].get<std::string>() == "addr"s &&
          ret["result"].is_string() &&
          ret["result"].get<std::string>() != MinerState::getPoolAddress() )
      {
        MinerState::setPoolAddress( ret["result"].get<std::string>() );
        MinerCore::updateMessage();
      }
      if( ret["id"].get<std::string>() == "diff"s &&
          ret["result"].is_number_unsigned() &&
          ret["result"].get<uint64_t>() > 0 )
      {
        MinerState::setDiff( ret["result"].get<uint64_t>() );
        MinerCore::updateTarget();
      }
      if( ret["id"].get<std::string>() == "chal"s &&
          ret["result"].is_string() &&
          ret["result"].get<std::string>() != MinerState::getChallenge() )
      {
        MinerState::setChallenge( ret["result"].get<std::string>() );
        MinerCore::updateMessage();
      }
    }
  }

  static auto submitSolutions() -> void
  {
    std::string prefix{ MinerState::getPrefix() };
    std::string oldPrefix{ MinerState::getOldPrefix() };

    json submission;
    json solParams{ 0,
                    MinerState::getAddress(),
                    0,
                    0,
                    "0x"s + MinerState::getChallenge(),
                    MinerState::getCustomDiff() };
    uint_fast16_t idCount{ 0u };

    BigUnsigned digestBU, target{ MinerState::getTarget() };
    for( auto const& sol : MinerState::getAllSolutions() )
    {
      std::string digest{ keccak256( prefix + sol ) };
      digestBU = BigUnsignedInABase{ digest, 16 };

      // I know, this is so incredibly ugly
      if( digestBU > MinerState::getTarget() )
      {
        digest = keccak256( oldPrefix + sol );
        digestBU = BigUnsignedInABase{ digest, 16 };
        bool submitAnyway{ false };

        if( digestBU <= target )
        {
          if( MinerState::getSubmitStale() )
          {
            submitAnyway = true;
          }
          else
          {
            Log::pushLog( "Stale solution; not submitting."s );
          }
        }
        else
        {
          Log::pushLog( "CPU verification failed."s );
        }

        if( !submitAnyway )
        {
          continue;
        }
      }

      solParams[0] = "0x"s + sol;
      solParams[2] = "0x"s + digest;
      // subtract 1 from the calculated diff because pool software rejects GTE instead of GT
      solParams[3] = BigUnsignedInABase( (MinerState::getMaximumTarget() / digestBU) - 1, 10u );

      submission.push_back( m_solution_base );
      submission.back()["params"] = solParams;
      submission.back()["id"] = idCount++;

      MinerState::resetCounter();
    }

    if( submission.size() == 0 ) { return; }

    totalCount.fetch_add( idCount, std::memory_order_release );

    do
    {
      if( m_stop.load( std::memory_order_acquire ) ) { return; }

      if( !doMethod( submission ) ) { break; }

      Log::pushLog( "Retrying in 2 seconds . . ."s );
      // change this to an event
      std::this_thread::sleep_for( 2s );
    }
    while( true );

    json::const_iterator iter;
    for( auto const& ret : submission )
    {
      iter = ret.find( "result" );
      if( iter != ret.end() && iter->is_boolean() && iter->get<bool>() )
      {
        if( solutionCount % 40 == 0 && solutionCount / 40 > devfeeCount )
        {
          ++devfeeCount;
          UI::UpdateDevSolutions( devfeeCount );
          Log::pushLog( "Submitted developer share #"s + std::to_string( devfeeCount ) + "."s );
        }
        else
        {
          ++solutionCount;
          MinerState::incSolCount();
        }
      }
    }
  }

  static auto netWorker() -> void
  {
    curl_easy_setopt( m_handle.get(), CURLOPT_URL, MinerState::getPoolUrl().c_str() );
    updateState( true );

    auto check_time{ steady_clock::now() + 4s };
    do
    {
      if( steady_clock::now() >= check_time )
      {
        updateState();
        check_time = steady_clock::now() + 4s;
      }

      submitSolutions();

      std::this_thread::sleep_for( 1ms );
    }
    while( !m_stop.load( std::memory_order_acquire ) );
  }
}

namespace Commo
{
  auto Init() -> void
  {
    if( m_started ) return;

    m_get_diff["params"][0] = m_get_target["params"][0] = MinerState::getAddress();

    sph_keccak256_init( &ctx );

    m_thread = std::thread( &netWorker );

    m_started = true;
  }

  auto Cleanup() -> void
  {
    if( !m_started ) return;

    m_stop.store( true, std::memory_order_release );
    if( m_thread.joinable() )
      m_thread.join();

    curl_global_cleanup();
  }

  auto GetPing() -> uint64_t
  {
    return uint64_t( m_ping.load( std::memory_order_acquire ) * 1000 );
  }

  auto GetTotalShares() -> uint64_t
  {
    return totalCount.load( std::memory_order_acquire );
  }

  auto GetConnectionErrorCount() -> uint64_t
  {
    return failureCount.load( std::memory_order_acquire );
  }

  auto GetConnectionErrorLog() -> std::vector<std::string>
  {
    guard lock( connectionMutex );
    return connectionErrors;
  }
}
