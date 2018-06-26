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
#include <stdexcept>
#include <curl/curl.h>

using namespace std::literals::string_literals;
using namespace std::chrono;
using namespace nlohmann;

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
  static std::atomic<int64_t> m_ping{ 0 };
  static std::atomic<bool> m_stop{ false };
  static sph_keccak256_context ctx;

  //static CURL* m_handle;
  //static curl_slist* m_headers;
  static CURLhandle m_handle{ nullptr, &curl_easy_cleanup };
  static CURLslist m_headers{ nullptr, &curl_slist_free_all };
  static std::array<char, CURL_ERROR_SIZE> m_errstr{ 0 };

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
    failureCount.fetch_add( 1, std::memory_order_release );
    MinerState::pushLog( error );
    {
      guard lock( connectionMutex );
      connectionErrors.push_back( error );
    }
  }

  static auto writebackHandler( char* __restrict body, size_t size, size_t nmemb, void* __restrict out ) -> size_t const
  {
    try
    {
      static_cast<std::string*>( out )->append( body, size * nmemb );
    }
    catch( ... )
    {
      //MinerState::pushLog( ":"s + std::string( body, size * nmemb ) + ":"s + std::to_string( size ) + ":"s + std::to_string( nmemb ) );
      return 0;
    }
    return size * nmemb;
  }

  static auto doMethod( json j ) -> json const
  {
    std::string response{};
    std::string body = j.dump();

    curl_easy_setopt( m_handle.get(), CURLOPT_POSTFIELDS, body.c_str() );
    curl_easy_setopt( m_handle.get(), CURLOPT_POSTFIELDSIZE, body.length() );
    curl_easy_setopt( m_handle.get(), CURLOPT_WRITEDATA, &response );

    CURLcode errcode{ curl_easy_perform( m_handle.get() ) };

    if( errcode != CURLE_OK )
    {
      logConnectionError( m_errstr.size() ? m_errstr.data() : curl_easy_strerror( errcode ) );

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
          throw std::runtime_error{ curl_easy_strerror( errcode ) };
          break;
        default:
          ; // do nothing
      }
    }

    curl_easy_getinfo( m_handle.get(), CURLINFO_PRETRANSFER_TIME_T, &m_ping );

    return json::parse( response.cbegin(), response.cend(), false );
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

    try
    {
      json response( doMethod( request ) );

      for( auto& ret : response )
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
          HybridMiner::updateMessage();
        }
        if( ret["id"].get<std::string>() == "diff"s &&
            ret["result"].is_number_unsigned() &&
            ret["result"].get<uint64_t>() > 0 )
        {
          MinerState::setDiff( ret["result"].get<uint64_t>() );
          HybridMiner::updateTarget();
        }
        if( ret["id"].get<std::string>() == "chal"s &&
            ret["result"].is_string() &&
            ret["result"].get<std::string>() != MinerState::getChallenge() )
        {
          MinerState::setChallenge( ret["result"].get<std::string>() );
          HybridMiner::updateMessage();
        }
      }
    }
    catch( std::runtime_error rterr )
    {
      return;
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

    totalCount.fetch_add( idCount, std::memory_order_release );

    json response{ doMethod( submission ) };

    while( true )
    {
      if( m_stop.load( std::memory_order_acquire ) ) return;

      try
      {
        response = doMethod( submission );
        break;
      }
      catch( std::runtime_error rterr )
      {
        MinerState::pushLog( "Retrying in 2 seconds . . ."s );
        std::this_thread::sleep_for( 2s );
      }
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
    while( !m_stop.load( std::memory_order_acquire ) )
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
    m_handle.reset( curl_easy_init() );

    m_headers.reset( curl_slist_append( NULL, "Content-Type: application/json" ) );
    curl_easy_setopt( m_handle.get(), CURLOPT_HTTPHEADER, m_headers.get() );

    curl_easy_setopt( m_handle.get(), CURLOPT_ERRORBUFFER, m_errstr.data() );

    curl_easy_setopt( m_handle.get(), CURLOPT_URL, MinerState::getPoolUrl().c_str() );
    curl_easy_setopt( m_handle.get(), CURLOPT_WRITEFUNCTION, writebackHandler );

    m_get_diff["params"][0] = MinerState::getAddress();
    m_get_target["params"][0] = MinerState::getAddress();

    sph_keccak256_init( &ctx );

    m_thread = std::thread( &netWorker );
  }

  auto Cleanup() -> void
  {
    m_stop.store( true, std::memory_order_release );
    if( m_thread.joinable() )
      m_thread.join();

    //curl_slist_free_all( m_headers );
    //curl_easy_cleanup( m_handle );
    curl_global_cleanup();
  }

  auto GetPing() -> uint64_t const
  {
    return uint64_t( m_ping.load( std::memory_order_acquire ) / 1000 );
  }

  auto GetTotalShares() -> uint64_t const
  {
    return totalCount.load( std::memory_order_acquire );
  }

  auto GetConnectionErrorCount() -> uint64_t const
  {
    return failureCount.load( std::memory_order_acquire );
  }

  auto GetConnectionErrorLog() -> std::vector<std::string> const
  {
    guard lock( connectionMutex );
    return connectionErrors;
  }
}
