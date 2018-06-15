#include "commo.h"
#include "miner_state.h"
#include "types.h"
#include "hybridminer.h"
#include "BigInt/BigIntegerLibrary.hh"
#include "json.hpp"
#include "sph_keccak.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <curl/curl.h>

using namespace std::literals::string_literals;
using namespace std::chrono;
using namespace nlohmann;

typedef std::lock_guard<std::mutex> guard;

namespace
{
  typedef std::unique_ptr<CURL, decltype( &curl_easy_cleanup )> CURLhandle;
  typedef std::unique_ptr<struct curl_slist, decltype( &curl_slist_free_all )> CURLslist;

  static uint_fast64_t solutionCount{ 0ull };
  static uint_fast64_t devfeeCount{ 0ull };
  static std::atomic<uint_fast64_t> failureCount{ 0ull };
  static std::vector<std::string> connectionErrors;
  static std::mutex connectionMutex;
  static std::atomic<uint_fast64_t> totalCount{ 0ull };
  static std::atomic<double> m_ping{ 0 };
  static std::atomic<bool> m_stop{ false };
  static sph_keccak256_context ctx;

  static std::thread m_thread;

  static json m_get_address{ { "jsonrpc", "2.0" }, { "method", "getPoolEthAddress" }, { "id", "addr" } };
  static json m_get_challenge{ { "jsonrpc", "2.0" }, { "method", "getChallengeNumber" }, { "id", "chal" } };
  static json m_get_diff{ { "jsonrpc", "2.0" }, { "method", "getMinimumShareDifficulty" }, { "params", {} }, { "id", "diff" } };
  static json m_get_target{ { "jsonrpc", "2.0" }, { "method", "getMinimumShareTarget" }, { "params", {} }, { "id", "tar" } };
  static json m_solution_base{ { "jsonrpc", "2.0" }, { "method", "submitShare" }, { "params", {} }, { "id", {} } };

  static auto keccak256( std::string const& message ) -> std::string const
  {
    message_t data;
    MinerState::hexToBytes( message, data );
    sph_keccak256( &ctx, data.data(), data.size() );
    hash_t out;
    sph_keccak256_close( &ctx, out.data() );
    return MinerState::bytesToString( out );
  }

  static auto logConnectionError( std::string const& error ) -> void
  {
    ++failureCount;
    MinerState::pushLog( error );
    {
      guard lock( connectionMutex );
      connectionErrors.push_back( error );
    }
  }

  static auto writebackHandler( char* __restrict body, size_t size, size_t nmemb, void* __restrict out ) -> size_t const
  {
    *static_cast<json*>( out ) = json::parse( std::string( body, size * nmemb ) );
    return size * nmemb;
  }

  static auto doMethod( json j ) -> json const
  {
    json response;
    std::string body = j.dump();

    CURLhandle handle{ curl_easy_init(), curl_easy_cleanup };
    char errstr[CURL_ERROR_SIZE]{ 0 };

    CURLslist header{ curl_slist_append( NULL, "Content-Type: application/json" ), curl_slist_free_all };
    curl_easy_setopt( handle.get(), CURLOPT_HTTPHEADER, header.get() );

    curl_easy_setopt( handle.get(), CURLOPT_ERRORBUFFER, errstr );

    curl_easy_setopt( handle.get(), CURLOPT_URL, MinerState::getPoolUrl().c_str() );
    curl_easy_setopt( handle.get(), CURLOPT_POSTFIELDS, body.c_str() );
    curl_easy_setopt( handle.get(), CURLOPT_POSTFIELDSIZE, body.length() );
    curl_easy_setopt( handle.get(), CURLOPT_WRITEFUNCTION, writebackHandler );
    curl_easy_setopt( handle.get(), CURLOPT_WRITEDATA, &response );

    CURLcode errcode{ curl_easy_perform( handle.get() ) };

    if( errcode != CURLE_OK )
    {
      logConnectionError( strlen( errstr ) ? errstr : curl_easy_strerror( errcode ) );

      switch( errcode )
      {
        case CURLE_FAILED_INIT:
          MinerState::pushLog( "Fatal error: libcURL failed to initialize."s );
          std::exit( EXIT_FAILURE );
          break;
        case CURLE_UNSUPPORTED_PROTOCOL:
        case CURLE_URL_MALFORMAT:
          MinerState::pushLog( "Fatal error: pool URL malformed." );
          std::exit( EXIT_FAILURE );
          break;
        case CURLE_OUT_OF_MEMORY:
          MinerState::pushLog( "Fatal error: libcURL could not allocate memory."s );
          std::exit( EXIT_FAILURE );
          break;
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_HTTP_POST_ERROR:
        case CURLE_TOO_MANY_REDIRECTS:
        case CURLE_GOT_NOTHING:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
          response = errcode;
          break;
        default:
          ; // do nothing
      }
    }

    curl_easy_getinfo( handle.get(), CURLINFO_CONNECT_TIME, &m_ping );

    return response;
  }

  static auto updateState( bool const& full ) -> void
  {
    json request;

    request.emplace_back( m_get_challenge );

    if( !MinerState::getCustomDiff() )
    {
      request.emplace_back( m_get_diff );
    }

    if( full )
    {
      request.emplace_back( m_get_address );
    }

    json response( doMethod( request ) );

    for( auto& ret : response )
    {
      if( ret["id"].is_string() &&
          ret["id"].get<std::string>() == "addr"s &&
          ret["result"].is_string() &&
          ret["result"].get<std::string>() != MinerState::getPoolAddress() )
      {
        MinerState::setPoolAddress( ret["result"].get<std::string>() );
        HybridMiner::updateMessage();
      }
      if( ret["id"].is_string() &&
          ret["id"].get<std::string>() == "diff"s &&
          ret["result"].is_number()
          && ret["result"].get<uint64_t>() > 0 )
      {
        MinerState::setDiff( ret["result"].get<uint64_t>() );
        HybridMiner::updateTarget();
      }
      if( ret["id"].is_string() &&
          ret["id"].get<std::string>() == "chal"s &&
          ret["result"].is_string() &&
          ret["result"].get<std::string>() != MinerState::getChallenge() )
      {
        MinerState::setChallenge( ret["result"].get<std::string>() );
        HybridMiner::updateMessage();
      }
    }
  }

  static auto submitSolutions() -> void
  {
    std::string sol{ MinerState::getSolution() };
    if( sol.empty() ) return;

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

    while( sol.length() > 0 )
    {
      std::string digest{ keccak256( prefix + sol ) };
      BigUnsigned digestBU{ BigUnsignedInABase{ digest, 16 } };

      // I know, this is so incredibly ugly
      if( digestBU > MinerState::getTarget() )
      {
        digest = keccak256( oldPrefix + sol );
        digestBU = BigUnsignedInABase{ digest, 16 };
        bool submitAnyway{ false };

        if( digestBU <= MinerState::getTarget() )
        {
          if( MinerState::getSubmitStale() )
          {
            submitAnyway = true;
          }
          else
          {
            MinerState::pushLog( "Stale solution; not submitting."s );
          }
        }
        else
        {
          MinerState::pushLog( "CPU verification failed."s );
        }

        if( !submitAnyway )
        {
          sol = MinerState::getSolution();
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
      sol = MinerState::getSolution();
    }

    totalCount.fetch_add( idCount + 1 );

    json response{ doMethod( submission ) };

    while( response.is_number_integer() )
    {
      MinerState::pushLog( "Retrying in 2 seconds . . ."s );
      std::this_thread::sleep_for( 2s );
      response = doMethod( submission );
    }

    for( auto& ret : response )
    {
      if( ret.find( "result" ) != ret.end() && ret["result"].is_boolean() && ret["result"].get<bool>() )
      {
        if( solutionCount % 40 == 0 && solutionCount / 40 > devfeeCount )
        {
          ++devfeeCount;
          MinerState::pushLog( "Submitted developer share #"s + std::to_string( devfeeCount ) + "."s );
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
    updateState( true );

    auto check_time{ steady_clock::now() + 4s };
    while( !m_stop )
    {
      if( steady_clock::now() >= check_time )
      {
        updateState( false );
        check_time = steady_clock::now() + 4s;
      }

      submitSolutions();

      std::this_thread::sleep_for( 1ms );
    }
  }
}

namespace Commo
{
  auto Init() -> void
  {
    curl_global_init( CURL_GLOBAL_ALL );

    m_get_diff["params"][0] = MinerState::getAddress();
    m_get_target["params"][0] = MinerState::getAddress();

    sph_keccak256_init( &ctx );

    m_thread = std::thread( &netWorker );
  }

  auto Cleanup() -> void
  {
    m_stop = true;
    if( m_thread.joinable() )
      m_thread.join();

    curl_global_cleanup();
  }

  auto GetPing() -> uint64_t const
  {
    return uint64_t(m_ping * 1000);
  }

  auto GetTotalShares() -> uint64_t const
  {
    return totalCount;
  }

  auto GetConnectionErrorCount() -> uint64_t const
  {
    return failureCount;
  }

  auto GetConnectionErrorLog() -> std::vector<std::string> const
  {
    guard lock( connectionMutex );
    return connectionErrors;
  }
}
