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

#include "miner_state.h"
#include "log.h"
#include "utils.h"
#include "platforms.h"
#include "minercore.h"
#include "ui.h"
#include "DynamicLibs/dlopencl.h"
#include "DynamicLibs/dlcuda.h"
#include <json.hpp>

#include <ctime>
#include <cctype>
#include <cassert>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <string_view>
#include <algorithm>
#include <stdexcept>
#include <condition_variable>
#if defined _MSC_VER // GCC is not cooperative
#  include <execution>
#endif

using namespace std::literals;
using namespace std::chrono;
using namespace Nabiki::Utils;
using json = nlohmann::json;

namespace
{
  static double constexpr DEFAULT_INTENSITY{ 23.0 };
  static std::array<std::pair<std::string_view, std::string_view>, 3> constexpr
    opencl_platforms{ { { "amd"sv, "AMD"sv }, { "nvidia"sv, "NVIDIA"sv }, { "intel"sv, "Intel"sv } } };

  static message_t m_message{};
  static state_t m_midstate{};
  static std::atomic<bool> m_midstate_ready{ false };
  static std::mutex m_midstate_mutex;
  static hash_t m_challenge_old{};
  static std::mutex m_message_mutex;
  static std::atomic<bool> m_challenge_ready{ false };
  static std::atomic<bool> m_pool_address_ready{ false };
  static std::atomic<uint64_t> m_sol_count{ 0ull };
  static std::atomic<uint64_t> m_target_num{ 0ull };
  static BigUnsigned m_target{ 0u };
  static BigUnsigned m_maximum_target{ 1u };
  static std::mutex m_target_mutex;
  static bool m_custom_diff{ false };
  static std::atomic<uint64_t> m_diff{ 1ull };
  static std::atomic<bool> m_diff_ready{ false };
  static std::string m_address{};
  static std::mutex m_address_mutex;
  static std::string m_pool_url{};
  static std::mutex m_pool_url_mutex;
  static std::mutex m_solutions_mutex;
  static std::vector<std::string> m_solutions_queue{};
  static hash_t m_solution{};
  static std::atomic<uint64_t> m_hash_count{ 0ull };
  static std::condition_variable m_is_ready;
  static std::mutex m_is_ready_mutex;

  static std::atomic<uint64_t> m_hash_count_printable{ 0ull };
  static std::queue<std::string> m_log{};
  static std::mutex m_log_mutex;
  static steady_clock::time_point m_start{};
  static steady_clock::time_point m_end{};
  static steady_clock::time_point m_round_start{ steady_clock::now() };
  static device_list_t m_cuda_devices{};
  static device_map_t m_opencl_devices{};
  static uint32_t m_cpu_threads{ 0ul };
  static std::string m_worker_name{};
  static std::string m_api_ports{};
  static std::string m_api_allowed{};
  static json m_json_config{};
  static std::string m_token_name{ "0xBTC" };
  static bool m_submit_stale{ false };
  static bool m_debug{ false };
}

// --------------------------------------------------------------------

namespace MinerState
{
  template auto pushSolution( uint64_t const ) -> void;
  template auto pushSolution( std::vector<uint64_t> const ) -> void;

  auto Init() -> void
  {
    std::ifstream in( "nabiki.json" );
    if( !in )
    {
      std::cerr << "Unable to open configuration file 'nabiki.json'.\n"sv;
      std::abort();
    }

    in >> m_json_config;
    in.close();

    json::iterator iter{ m_json_config.find( "address" ) };
    if( iter == m_json_config.end() ||
        ( iter->is_string() &&
          iter->get<std::string>().length() != 42 ) )
    {
      std::cerr << "No valid wallet address set in configuration - how are you supposed to get paid?\n"sv;
      std::abort();
    }
    setAddress( iter->get<std::string>() );

    iter = m_json_config.find( "pool" );
    if( iter == m_json_config.end() ||
        ( iter->is_string() &&
          iter->get<std::string>().length() < 15 ) )
    {
      std::cerr << "No pool address set in configuration - this isn't a solo miner!\n"sv;
      std::abort();
    }
    setPoolUrl( iter->get<std::string>() );

    // this has to come before diff is set
    iter = m_json_config.find( "token" );
    if( iter != m_json_config.end() &&
        iter->is_string() &&
        iter->get<std::string>().length() > 0u )
    {
      setTokenName( iter->get<std::string>() );
    }
    else
    {
      setTokenName( "0xBitcoin"s );
    }

    iter = m_json_config.find( "customdiff" );
    if( iter != m_json_config.end() &&
        iter->is_number_unsigned() &&
        iter->get<uint64_t>() > 0u )
    {
      setCustomDiff( iter->get<uint64_t>() );
    }

    iter = m_json_config.find( "submitstale"s );
    if( iter != m_json_config.end() &&
        iter->is_boolean() )
    {
      setSubmitStale( m_json_config["submitstale"].get<bool>() );
    }

    iter = m_json_config.find( "debug" );
    if( iter != m_json_config.end() &&
        iter->is_boolean() )
    {
      m_debug = m_json_config["debug"].get<bool>();
    }

    iter = m_json_config.find( "cuda" );
    if( iter != m_json_config.end() &&
        iter->is_array() &&
        iter->size() > 0u )
    {
      Cuda cu{};
      if( !cu.Init || cu.Init( 0u ) ){ return; }

      int32_t device_count;
      cu.DeviceGetCount( &device_count );

      json::const_iterator it_en, it_dev, it_int;
      for( auto const& device : m_json_config["cuda"] )
      {
        it_en = device.find( "enabled"s );
        it_dev = device.find( "device"s );
        it_int = device.find( "intensity"s );
        if( ( it_en != device.end() &&
              it_en->is_boolean() &&
              it_en->get<bool>() ) &&
              ( it_dev != device.end() &&
                it_dev->is_number_integer() &&
                it_dev->get<int32_t>() < device_count ) )
        {
          m_cuda_devices.emplace_back( *it_dev,
            ( ( it_int != device.end() &&
                it_int->is_number() )
              ? it_int->get<double>()
              : DEFAULT_INTENSITY ) );
        }
      }
    }

    iter = m_json_config.find( "opencl"s );
    if( iter != m_json_config.end() &&
        iter->is_object() &&
        iter->size() > 0u )
    {
      Opencl cl{};
      if( !cl.GetDeviceIDs ){ return; }

      std::vector<cl_platform_id> platforms;
      {
        cl_uint plat_count{ 0 };
        cl.GetPlatformIDs( 0u, nullptr, &plat_count );
        platforms.resize( plat_count );
        cl.GetPlatformIDs( plat_count, platforms.data(), nullptr );
      }
      cl_uint device_count{ 0 };

      for( auto const& [platId, platName] : opencl_platforms )
      {
        device_count = 0;
        for( auto const& p : platforms )
        {
          std::string plat_name;
          {
            size_t name_size{ 0 };
            cl.GetPlatformInfo( p, CL_PLATFORM_NAME, 0u, nullptr, &name_size );
            char* name{ new char[name_size] };
            cl.GetPlatformInfo( p, CL_PLATFORM_NAME, name_size, name, nullptr );
            plat_name = name;
            delete[] name;
          }
          if( plat_name.find( platName ) != std::string::npos )
          {
            cl.GetDeviceIDs( p, CL_DEVICE_TYPE_ALL, 0u, nullptr, &device_count );
            break;
          }
        }

        json::iterator it_plat = iter->find( platId );
        if( it_plat->is_array() && it_plat->size() > 0u )
        {
          device_list_t devices;

          json::const_iterator it_en, it_dev, it_int;
          for( auto const& device : *it_plat )
          {
            it_en = device.find( "enabled"s );
            it_dev = device.find( "device"s );
            it_int = device.find( "intensity"s );
            if( ( it_en != device.end() &&
                  it_en->is_boolean() &&
                  it_en->get<bool>() ) &&
                  ( it_dev != device.end() &&
                    it_dev->is_number_integer() &&
                    it_dev->get<cl_uint>() < device_count ) )
            {
              devices.emplace_back( *it_dev,
                ( ( it_int != device.end() &&
                    it_int->is_number() )
                  ? it_int->get<double>()
                  : DEFAULT_INTENSITY ) );
            }
          }

          if( devices.size() > 0u )
          {
            m_opencl_devices.emplace_back( platId, devices );
          }
        }
      }
    }

    iter = m_json_config.find( "threads"s );
    if( iter != m_json_config.end() &&
        iter->is_number() &&
        iter->get<uint32_t>() > 0 )
    {
      m_cpu_threads = iter->get<uint32_t>();
    }

    iter = m_json_config.find( "worker_name"s );
    if( iter != m_json_config.end() &&
        iter->is_string() )
    {
      m_worker_name = iter->get<std::string>();
    }

    iter = m_json_config.find( "telemetry"s );
    if( iter != m_json_config.end() )
    {
      if( iter->is_boolean() &&
          iter->get<bool>() )
      {
        m_api_ports = "4863"s;
        //m_api_allowed = ""s;
      }
      else if( iter->is_object())
      {
        json::iterator it_en{ iter->find( "enabled"s ) };
        if( it_en != iter->end() &&
            it_en->is_boolean() &&
            it_en->get<bool>() )
        {
          json::iterator it_port{ iter->find( "port"s ) };
          if( it_port != iter->end() && it_port->is_number_unsigned() )
          {
            uint64_t temp{ it_port->get<uint64_t>() };
            if( temp > 0 && temp < 65535 )
            {
              m_api_ports = std::to_string( temp );
            }
          }
          else if( it_port->is_string() )
          {
            std::string temp{ it_port->get<std::string>() };
            if( temp.length() > 0u && temp != "0"s )
            {
              m_api_ports = it_port->get<std::string>();
            }
          }
          else
          {
            Log::pushLog( R"(API enabled but "port" not present or malformed. Using default port 4863.)"s );
            m_api_ports = "4863"s;
          }

          //json::iterator it_acl{ iter->find( "acl" ) };
          //if( it_acl != m_json_config["telemetry"].end() &&
          //    it_acl->is_string() &&
          //    it_acl->get<std::string>().length() > 0u )
          //{
          //  m_api_allowed = it_acl->get<std::string>();
          //}
          //else
          //{
          //  pushLog( R"(API configuration error: "acl" present but malformed. Using default empty ACL.)"s );
          //  m_api_allowed = ""s;
          //}
        }
      }
    }

    reinterpret_cast<uint64_t&>( m_solution[0] ) = 06055134500533075101ull;

    std::mt19937_64 gen{ std::random_device{}() };
    std::uniform_int_distribution<uint64_t> urInt{ 0, UINT64_MAX };

    for( uint_fast8_t i_rand{ 1 }; i_rand < 4; ++i_rand )
    {
      reinterpret_cast<uint64_t*>( m_solution.data() )[i_rand] = urInt( gen );
    }

    std::memset( &m_solution[12], 0, 8 );

    {
      guard lock( m_message_mutex );
      std::memcpy( &m_message[52], m_solution.data(), 32 );
    }

    m_start = steady_clock::now();
    m_round_start = steady_clock::now();
  }

  auto getIncSearchSpace( uint64_t const& threads ) -> uint64_t const
  {
    UI::UpdateHashrate( m_hash_count_printable.fetch_add( threads, std::memory_order_acq_rel ) );

    return m_hash_count.fetch_add( threads, std::memory_order_acq_rel );
  }

  auto resetCounter() -> void
  {
    m_hash_count_printable.store( 0ull, std::memory_order_release );

    m_round_start = steady_clock::now();
  }

  auto getRoundStartTime() -> std::chrono::time_point<std::chrono::steady_clock> const&
  {
    return m_round_start;
  }

  auto getPrintableHashCount() -> uint64_t const
  {
    return m_hash_count_printable.load( std::memory_order_acquire );
  }

  auto getSolution() -> std::string
  {
    std::string retStr{};

    if( !m_solutions_queue.empty() )
    {
      guard lock( m_solutions_mutex );
      retStr = m_solutions_queue.back();
      m_solutions_queue.pop_back();
    }

    return retStr;
  }

  auto getAllSolutions() -> std::vector<std::string>
  {
    std::vector<std::string> retVec;

    if( !m_solutions_queue.empty() )
    {
      guard lock( m_solutions_mutex );
      retVec.swap( m_solutions_queue );
    }

    return retVec;
  }

  template<typename T>
  auto pushSolution( T const sols ) -> void
  {
    static hash_t ret{ m_solution };

    guard lock( m_solutions_mutex );
    if constexpr( std::is_integral_v<T> )
    {
      std::memcpy( &ret[12], &sols, 8 );
      m_solutions_queue.emplace_back( bytesToString( ret ) );
    }
    else
    {
      m_solutions_queue.reserve( sols.size() + m_solutions_queue.size() );
      for( auto const& val : sols )
      {
        std::memcpy( &ret[12], &val, 8 );
        m_solutions_queue.emplace_back( bytesToString( ret ) );
      }
    }
  }

  auto incSolCount( uint64_t const& count ) -> void
  {
    m_sol_count.fetch_add( count, std::memory_order_acq_rel );

    UI::UpdateSolutions( m_sol_count.load( std::memory_order_acquire ) );
  }

  auto getSolCount() -> uint64_t const
  {
    return m_sol_count.load( std::memory_order_acquire );
  }

  auto getPrefix() -> std::string const
  {
    prefix_t temp;

    {
      guard lock( m_message_mutex );
      //std::copy( m_message.begin(), m_message.begin() + 51, temp.begin() );
      std::memcpy( temp.data(), m_message.data(), 52 );
    }

    return bytesToString( temp );
  }

  auto getOldPrefix() -> std::string const
  {
    prefix_t temp;

    {
      guard lock( m_message_mutex );
      //std::copy( m_challenge_old.begin(), m_challenge_old.begin() + 31, temp.begin() );
      //std::copy( m_message.begin() + 31, m_message.begin() + 51, temp.begin() );
      std::memcpy( temp.data(), m_challenge_old.data(), 32 );
      std::memcpy( &temp[32], m_message.data(), 20 );
    }

    return bytesToString( temp );
  }

  auto setChallenge( std::string_view const challenge ) -> void
  {
    hash_t temp;
    hexToBytes( challenge, temp );

    {
      guard lock( m_message_mutex );

      if( getSubmitStale() )
        //std::copy( m_message.begin(), m_message.begin() + 31, m_challenge_old.begin() );
        std::memcpy( m_challenge_old.data(), m_message.data(), 32 );

      //std::move( temp.begin(), temp.begin() + 31, m_message.begin() );
      std::memmove( m_message.data(), temp.data(), 32 );
    }

    if( !getSubmitStale() )
    {
      guard lock( m_solutions_mutex );
      std::vector<std::string>().swap( m_solutions_queue );
    }

    UI::UpdateChallenge( challenge.substr( 2, 8 ) );

    {
      guard lock{ m_is_ready_mutex };
      m_challenge_ready.store( true, std::memory_order_release );
    }
    m_is_ready.notify_one();
    setMidstate();
  }

  auto getChallenge() -> std::string const
  {
    hash_t temp;

    {
      guard lock( m_message_mutex );
      //std::copy( m_message.begin(), m_message.begin() + 31, temp.begin() );
      std::memcpy( temp.data(), m_message.data(), 32 );
    }

    return bytesToString( temp );
  }

  auto getPreviousChallenge() -> std::string const
  {
    hash_t temp;

    {
      guard lock( m_message_mutex );
      //std::copy( m_message.begin(), m_message.begin() + 31, temp.begin() );
      std::memcpy( temp.data(), m_challenge_old.data(), 32 );
    }

    return bytesToString( temp );
  }

  auto setPoolAddress( std::string_view const address ) -> void
  {
    address_t temp;
    hexToBytes( address, temp );

    {
      guard lock( m_message_mutex );
      //std::move( temp.begin(), temp.end(), m_message.begin() + 31 );
      std::memmove( &m_message[32], temp.data(), 20 );
    }

    {
      guard lock( m_is_ready_mutex );
      m_pool_address_ready.store( true, std::memory_order_release );
    }
    m_is_ready.notify_one();
    setMidstate();
  }

  auto getPoolAddress() -> std::string const
  {
    address_t temp;

    {
      guard lock( m_message_mutex );
      //std::copy( m_message.begin() + 31, m_message.begin() + 51, temp.begin() );
      std::memcpy( temp.data(), &m_message[32], 20 );
    }

    return bytesToString( temp );
  }

  auto setTarget( BigUnsigned const& target ) -> void
  {
    if( target == m_target ) return;

    {
      guard lock( m_target_mutex );
      m_target = target;
    }

    m_target_num.store( target.getBlock( 3 ), std::memory_order_release );
  }

  auto getTarget() -> BigUnsigned const
  {
    guard lock( m_target_mutex );
    return m_target;
  }

  auto getTargetNum() -> uint64_t const
  {
    return m_target_num.load( std::memory_order_acquire );
  }

  auto getMaximumTarget() -> BigUnsigned const&
  {
    return m_maximum_target;
  }

  auto getMessage() -> message_t const
  {
    guard lock( m_message_mutex );
    return m_message;
  }

  auto setMidstate() -> void
  {
    if( !m_challenge_ready.load( std::memory_order_acquire ) ||
        !m_pool_address_ready.load( std::memory_order_acquire ) )
    { return; }

    std::array<uint64_t, 11> message{ 0 };

    {
      guard lock( m_message_mutex );
      //std::copy( m_message.begin(), m_message.end(), reinterpret_cast<uint8_t*>( message.data() ) );
      std::memcpy( message.data(), m_message.data(), 84 );
    }

    std::array<uint64_t, 25> mid;
    uint64_t C[5], D[5];
    C[0] = message[0] ^ message[5] ^ message[10] ^ 0x100000000ull;
    C[1] = message[1] ^ message[6] ^ 0x8000000000000000ull;
    C[2] = message[2] ^ message[7];
    C[3] = message[3] ^ message[8];
    C[4] = message[4] ^ message[9];

    D[0] = rotl64( C[1], 1 ) ^ C[4];
    D[1] = rotl64( C[2], 1 ) ^ C[0];
    D[2] = rotl64( C[3], 1 ) ^ C[1];
    D[3] = rotl64( C[4], 1 ) ^ C[2];
    D[4] = rotl64( C[0], 1 ) ^ C[3];

    mid[0] = message[0] ^ D[0];
    mid[1] = rotl64( message[6] ^ D[1], 44 );
    mid[2] = rotl64( D[2], 43 );
    mid[3] = rotl64( D[3], 21 );
    mid[4] = rotl64( D[4], 14 );
    mid[5] = rotl64( message[3] ^ D[3], 28 );
    mid[6] = rotl64( message[9] ^ D[4], 20 );
    mid[7] = rotl64( message[10] ^ D[0] ^ 0x100000000ull, 3 );
    mid[8] = rotl64( 0x8000000000000000ull ^ D[1], 45 );
    mid[9] = rotl64( D[2], 61 );
    mid[10] = rotl64( message[1] ^ D[1], 1 );
    mid[11] = rotl64( message[7] ^ D[2], 6 );
    mid[12] = rotl64( D[3], 25 );
    mid[13] = rotl64( D[4], 8 );
    mid[14] = rotl64( D[0], 18 );
    mid[15] = rotl64( message[4] ^ D[4], 27 );
    mid[16] = rotl64( message[5] ^ D[0], 36 );
    mid[17] = rotl64( D[1], 10 );
    mid[18] = rotl64( D[2], 15 );
    mid[19] = rotl64( D[3], 56 );
    mid[20] = rotl64( message[2] ^ D[2], 62 );
    mid[21] = rotl64( message[8] ^ D[3], 55 );
    mid[22] = rotl64( D[4], 39 );
    mid[23] = rotl64( D[0], 41 );
    mid[24] = rotl64( D[1], 2 );

    {
      guard lock( m_midstate_mutex );
      //std::move( mid.begin(), mid.end(), reinterpret_cast<uint64_t*>(m_midstate.data()) );
      std::memmove( m_midstate.data(), mid.data(), 200 );
    }
  }

  auto getMidstate() -> state_t const
  {
    if( !m_midstate_ready ) setMidstate();

    guard lock( m_midstate_mutex );
    return m_midstate;
  }

  auto setAddress( std::string_view const address ) -> void
  {
    {
      guard lock( m_address_mutex );
      m_address = address;
    }

    UI::UpdateAddress( address.substr( 0, 8 ) );
  }

  auto getAddress() -> std::string const
  {
    guard lock( m_address_mutex );
    return m_address;
  }

  auto setCustomDiff( uint64_t const& diff ) -> void
  {
    m_custom_diff = true;
    setDiff( diff );
  }

  auto getCustomDiff() -> bool const&
  {
    return m_custom_diff;
  }

  auto setDiff( uint64_t const& diff ) -> void
  {
    m_diff.store( diff, std::memory_order_release );
    {
      guard lock{ m_is_ready_mutex };
      m_diff_ready.store( true, std::memory_order_release );
    }
    m_is_ready.notify_one();
    setTarget( m_maximum_target / diff );

    UI::UpdateDifficulty( diff );
  }

  auto getDiff() -> uint64_t const
  {
    return m_diff.load( std::memory_order_acquire );
  }

  auto setPoolUrl( std::string_view const pool ) -> void
  {
    if( pool.find( "mine0xbtc.eu"s ) != std::string::npos )
    {
      std::cerr << "Selected pool '"sv << pool
                << "' is blocked for deceiving the community and apparent scamming.\n"sv
                << "Please select a different pool to mine on.\n"sv;
      std::abort();
    }
    {
      guard lock( m_pool_url_mutex );
      m_pool_url = pool;
    }

    UI::UpdateUrl( pool );
  }

  auto getPoolUrl() -> std::string const
  {
    guard lock( m_pool_url_mutex );
    return m_pool_url;
  }

  auto getCudaDevices() -> device_list_t const&
  {
    return m_cuda_devices;
  }

  auto getClDevices() -> device_map_t const&
  {
    return m_opencl_devices;
  }

  auto getCpuThreads() -> uint32_t const&
  {
    return m_cpu_threads;
  }

  auto setTokenName( std::string_view const token ) -> void
  {
    m_token_name = token;
    std::string temp{ token };
    std::transform(
#if defined _MSC_VER // GCC is not cooperative
                    std::execution::par_unseq,
#endif
                    token.begin(),
                    token.end(),
                    temp.begin(),
                    []( uint8_t c ) { return static_cast<char>( std::tolower( c ) ); } );
    if( temp == "0xcate" || temp == "0xcatether" )
    {
      m_maximum_target <<= 224;
    }
    else
    {
      m_maximum_target <<= 234;
    }
  }

  auto setSubmitStale( bool const& submitStale ) -> void
  {
    m_submit_stale = submitStale;
  }

  auto getSubmitStale() -> bool const&
  {
    return m_submit_stale;
  }

  auto getWorkerName() -> std::string_view
  {
    return m_worker_name;
  }

  auto getTelemetryPorts() -> std::string_view
  {
    return m_api_ports;
  }

  auto getTelemetryAcl() -> std::string_view
  {
    return m_api_allowed;
  }

  auto waitUntilReady() -> void
  {
    cond_lock lock( m_is_ready_mutex );
    m_is_ready.wait( lock, [&] {
        return (m_diff_ready.load( std::memory_order_acquire ) &&
                m_challenge_ready.load( std::memory_order_acquire ) &&
                m_pool_address_ready.load( std::memory_order_acquire ));
      } );
  }

  auto isDebug() -> bool const&
  {
    return m_debug;
  }
}
