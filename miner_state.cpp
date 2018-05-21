#define __STDC_WANT_LIB_EXT1__ 1

#include "miner_state.h"
#include "hybridminer.h"
#include "json.hpp"

#define SPH_KECCAK_64 1
#include "sph_keccak.h"

#include <ctime>
#include <cctype>
#include <cassert>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <atomic>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <cuda_runtime.h>

#ifdef _MSC_VER
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif // _MSC_VER

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wignored-attributes"
#endif // __GNUC__
#ifdef __APPLE__
#  include <OpenCL/cl.hpp>
#else
#  include <CL/cl.hpp>
#endif
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

using namespace std::literals::string_literals;
using namespace std::chrono;

typedef std::lock_guard<std::mutex> guard;

namespace
{
  static uint_fast8_t constexpr PREFIX_LENGTH{ 52u };
  static uint_fast8_t constexpr UINT256_LENGTH{ 32u };
  static uint_fast8_t constexpr MESSAGE_LENGTH{ 84u };
  static double constexpr DEFAULT_INTENSITY{ 23.0 };
  static char constexpr ascii[][3] = {
    "00","01","02","03","04","05","06","07","08","09","0a","0b","0c","0d","0e","0f",
    "10","11","12","13","14","15","16","17","18","19","1a","1b","1c","1d","1e","1f",
    "20","21","22","23","24","25","26","27","28","29","2a","2b","2c","2d","2e","2f",
    "30","31","32","33","34","35","36","37","38","39","3a","3b","3c","3d","3e","3f",
    "40","41","42","43","44","45","46","47","48","49","4a","4b","4c","4d","4e","4f",
    "50","51","52","53","54","55","56","57","58","59","5a","5b","5c","5d","5e","5f",
    "60","61","62","63","64","65","66","67","68","69","6a","6b","6c","6d","6e","6f",
    "70","71","72","73","74","75","76","77","78","79","7a","7b","7c","7d","7e","7f",
    "80","81","82","83","84","85","86","87","88","89","8a","8b","8c","8d","8e","8f",
    "90","91","92","93","94","95","96","97","98","99","9a","9b","9c","9d","9e","9f",
    "a0","a1","a2","a3","a4","a5","a6","a7","a8","a9","aa","ab","ac","ad","ae","af",
    "b0","b1","b2","b3","b4","b5","b6","b7","b8","b9","ba","bb","bc","bd","be","bf",
    "c0","c1","c2","c3","c4","c5","c6","c7","c8","c9","ca","cb","cc","cd","ce","cf",
    "d0","d1","d2","d3","d4","d5","d6","d7","d8","d9","da","db","dc","dd","de","df",
    "e0","e1","e2","e3","e4","e5","e6","e7","e8","e9","ea","eb","ec","ed","ee","ef",
    "f0","f1","f2","f3","f4","f5","f6","f7","f8","f9","fa","fb","fc","fd","fe","ff"
  };

  static auto fromAscii( uint8_t const c ) -> uint8_t
  {
    if( c >= '0' && c <= '9' )
      return ( c - '0' );
    if( c >= 'a' && c <= 'f' )
      return ( c - 'a' + 10 );
    if( c >= 'A' && c <= 'F' )
      return ( c - 'A' + 10 );

    throw std::runtime_error( "invalid character" );
  }

  static auto ascii_r( uint8_t const a, uint8_t const b ) -> uint8_t
  {
    return fromAscii( a ) * 16 + fromAscii( b );
  }

  static auto HexToBytes( std::string const hex, uint8_t bytes[] ) -> void
  {
    for( std::string::size_type i = 0, j = 0; i < hex.length(); i += 2, ++j )
    {
      bytes[j] = ascii_r( hex[i], hex[i + 1] );
    }
  }

  static std::mutex m_solutions_mutex;
  static std::queue<uint64_t> m_solutions_queue;
  static hash_t m_solution;
  static std::atomic<uint64_t> m_hash_count{ 0ull };
  static std::atomic<uint64_t> m_hash_count_printable{ 0ull };
  static message_t m_message;
  static hash_t m_challenge_old;
  static std::mutex m_message_mutex;
  static std::atomic<bool> m_message_ready{ false };
  static std::atomic<uint64_t> m_sol_count{ 0ull };
  static std::mutex m_print_mutex;
  static std::queue<std::string> m_log;
  static std::mutex m_log_mutex;
  static std::string m_challenge_printable;
  static std::string m_address_printable;
  static std::atomic<uint64_t> m_target_num{ 0ull };
  static BigUnsigned m_target{ 0ul };
  static BigUnsigned m_maximum_target{ 1ul };
  static std::mutex m_target_mutex;
  static std::atomic<bool> m_custom_diff{ false };
  static std::atomic<uint64_t> m_diff{ 1ull };
  static std::atomic<bool> m_diff_ready{ false };
  static std::atomic<bool> m_new_solution{ false };
  static std::string m_address;
  static std::mutex m_address_mutex;
  static std::string m_pool_url;
  static std::mutex m_pool_url_mutex;
  static steady_clock::time_point m_start;
  static steady_clock::time_point m_end;
  static steady_clock::time_point m_round_start;
  static std::atomic<bool> m_old_ui{ false };
  static std::vector<std::pair<int32_t, double>> m_cuda_devices;
  static std::atomic<uint32_t> m_cpu_threads{ 0ul };
  static nlohmann::json m_json_config;
  static std::string m_token_name{ "0xBTC" };
  static std::atomic<bool> m_submit_stale{ false };
};

// --------------------------------------------------------------------

auto MinerState::initState() -> void
{
  std::ifstream in( "nabiki.json" );
  if( !in )
  {
    std::cerr << "Unable to open configuration file '0xbitcoin.json'.\n"s;
    std::exit( EXIT_FAILURE );
  }

  in >> m_json_config;
  in.close();

  if( m_json_config.find( "address" ) == m_json_config.end() ||
      ( m_json_config["address"].is_string() &&
        m_json_config["address"].get<std::string>().length() != 42 ) )
  {
    std::cerr << "No valid wallet address set in configuration - how are you supposed to get paid?\n"s;
    std::exit( EXIT_FAILURE );
  }
  if( m_json_config.find( "pool" ) == m_json_config.end() ||
      ( m_json_config["pool"].is_string() &&
        m_json_config["pool"].get<std::string>().length() < 15 ) )
  {
    std::cerr << "No pool address set in configuration - this isn't a solo miner!\n"s;
    std::exit( EXIT_FAILURE );
  }

  setAddress( m_json_config["address"] );
  setPoolUrl( m_json_config["pool"] );

// this has to come before diff is set
  if( m_json_config.find( "token" ) != m_json_config.end() &&
      m_json_config["token"].is_string() &&
      m_json_config["token"].get<std::string>().length() > 0u )
  {
    setTokenName( m_json_config["token"].get<std::string>() );
  }
  else
  {
    setTokenName( "0xBitcoin"s );
  }

  if( m_json_config.find( "customdiff" ) != m_json_config.end() &&
      m_json_config["customdiff"].is_number_unsigned() &&
      m_json_config["customdiff"].get<uint64_t>() > 0u )
  {
    setCustomDiff( m_json_config["customdiff"].get<uint64_t>() );
  }

  if( m_json_config.find( "submitstale" ) != m_json_config.end() &&
      m_json_config["submitstale"].is_boolean() )
  {
    MinerState::setSubmitStale( m_json_config["submitstale"].get<bool>() );
  }

  int32_t device_count;
  cudaGetDeviceCount( &device_count );

  if( m_json_config.find( "cuda" ) != m_json_config.end() &&
      m_json_config["cuda"].is_array() &&
      m_json_config["cuda"].size() > 0u )
  {
    for( auto& device : m_json_config["cuda"] )
    {
      if( ( device.find( "enabled" ) != device.end() &&
            device["enabled"].is_boolean() &&
            device["enabled"].get<bool>() ) &&
          ( device.find( "device" ) != device.end() &&
            device["device"].is_number_integer() &&
            device["device"].get<int32_t>() < device_count ) )
      {
        m_cuda_devices.emplace_back( device["device"],
                                     ( ( device.find( "intensity" ) != device.end() &&
                                         device["intensity"].is_number_float() )
                                       ? device["intensity"].get<double>()
                                       : DEFAULT_INTENSITY ) );
      }
    }
  }
  else
  {
    for( int_fast32_t i{ 0u }; i < device_count; ++i )
    {
      m_cuda_devices.emplace_back( i, DEFAULT_INTENSITY );
    }
  }

  if( m_json_config.find( "threads" ) != m_json_config.end() &&
      m_json_config["threads"].is_number() &&
      m_json_config["threads"].get<uint32_t>() > 0)
  {
    m_cpu_threads = m_json_config["threads"].get<uint32_t>();
  }

  reinterpret_cast<uint64_t&>(m_solution[0]) = 06055134500533075101ull;

  std::random_device r;
  std::mt19937_64 gen{ r() };
  std::uniform_int_distribution<uint64_t> urInt{ 0, UINT64_MAX };

  for( uint_fast8_t i_rand{ 8 }; i_rand < 32; i_rand += 8 )
  {
    reinterpret_cast<uint64_t&>(m_solution[i_rand]) = urInt( gen );
  }

  // Default init (false) is fine for non-Windows plats
#ifdef _MSC_VER
  m_old_ui = []() -> bool
    {
      OSVERSIONINFO winVer;
      ZeroMemory( &winVer, sizeof(OSVERSIONINFO) );
      winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

      // Stop deprecating things you don't have a _full_ replacement for!
#pragma warning( push )
#pragma warning( disable: 4996 )
      GetVersionEx( &winVer );
#pragma warning( pop )

      if( ( winVer.dwMajorVersion < 10 ) ||
          ( winVer.dwMajorVersion >= 10 &&
            winVer.dwBuildNumber < 14392 ) )
      {
        return true;
      }
      return false;
    }();
#endif // _MSC_VER

  std::memset( &m_solution[12], 0, 8 );

  {
    guard lock( m_message_mutex );
    std::memcpy( &m_message[52], m_solution.data(), 32 );
  }

  std::string str_solution{ bytesToString( m_solution ) };

  m_start = steady_clock::now();
  m_round_start = steady_clock::now();
}

auto MinerState::getIncSearchSpace( uint64_t const threads ) -> uint64_t const
{
  m_hash_count_printable += threads;
  return m_hash_count.fetch_add( threads, std::memory_order_seq_cst );
}

auto MinerState::resetCounter() -> void
{
  m_hash_count_printable = 0ull;

  m_round_start = steady_clock::now();
}

auto MinerState::printStatus() -> void
{
  if( m_hash_count <= 0 ) return;

  double timer{ static_cast<double>(duration_cast<milliseconds>(steady_clock::now() - m_round_start).count()) / 1000 };

  std::stringstream ss_out;

  double hashrate{ 0 };
  for( auto& temp: HybridMiner::getHashrates() )
  {
    hashrate += temp;
  }

  ss_out << getLog();

  if( m_old_ui )
  {
    // print every every 5 seconds . . . more or less
    static auto time_counter{ steady_clock::now() + 5s };
    if( time_counter > steady_clock::now() ) return;
    time_counter = steady_clock::now() + 5s;

    ss_out << getPrintableTimeStamp()
           << std::setw( 10 ) << std::setfill( ' ' ) << std::fixed << std::setprecision( 2 )
           << hashrate
           << " MH/s  Sols:"
           << std::setw( 6 ) << std::setfill( ' ' ) << m_sol_count
           << (m_new_solution ? '^' : ' ')
           << " Search time: "
           << std::fixed << std::setprecision( 0 ) << std::setfill( '0' )
           << std::setw( 2 ) << std::floor( timer / 60 ) << ":"
           << std::setw( 2 ) << std::floor( std::fmod( timer, 60 ) )
           << '\n';

    m_new_solution = false;
  }
  else
  {
    // maybe breaking the control codes into macros is a good idea . . .
    ss_out << "\x1b[s\x1b[?25l\x1b[2;22f\x1b[38;5;221m"
           << std::setw( 8 ) << std::setfill( ' ' ) << std::fixed << std::setprecision( 2 )
           << hashrate
           << "\x1b[2;75f\x1b[38;5;33m"
           << std::fixed << std::setprecision( 0 ) << std::setfill( '0' )
           << std::setw( 2 ) << std::floor( timer / 60 ) << ":"
           << std::setw( 2 ) << std::floor( std::fmod( timer, 60 ) )
           << "\x1b[3;14f\x1b[38;5;34m"
           << m_diff
           << "\x1b[3;22f\x1b[38;5;221m"
           << std::setw( 8 ) << std::setfill( ' ' ) << m_sol_count
           << "\x1b[3;72f\x1b[38;5;33m";
    {
      guard lock( m_print_mutex );
      ss_out << m_address_printable
             <<"\x1b[2;13f\x1b[38;5;34m"
             <<  m_challenge_printable;
    }
    ss_out.imbue( std::locale( "" ) );
    ss_out << "\x1b[3;36f\x1b[38;5;208m"
           << std::setw( 25 ) << m_hash_count_printable;
    ss_out << "\x1b[0m\x1b[u\x1b[?25h";
  }

  std::cout << ss_out.str();
}

auto MinerState::getLog() -> std::string const
{
  std::stringstream ss_log;

  {
    guard lock(m_log_mutex);

    if( m_log.size() == 0 ) return ""s;

    for( uint_fast8_t i{ 0 }; i < 5 && m_log.size() != 0; ++i )
    {
      ss_log << m_log.front();
      m_log.pop();
    }
  }

  return ss_log.str();
}

auto MinerState::pushLog( std::string message ) -> void
{
  guard lock(m_log_mutex);
  m_log.push( getPrintableTimeStamp() + message + '\n' );
}

auto MinerState::getPrintableTimeStamp() -> std::string const
{
  std::stringstream ss_ts;

  auto now{ system_clock::now() };
  auto now_ms{ duration_cast<milliseconds>( now.time_since_epoch() ) % 1000 };
  std::time_t tt_ts{ system_clock::to_time_t( now ) };
  std::tm tm_ts{ *std::localtime( &tt_ts ) };

  if( !m_old_ui ) ss_ts << "\x1b[90m";
  ss_ts << std::put_time( &tm_ts, "[%T" ) << '.'
        << std::setw( 3 ) << std::setfill( '0' ) << now_ms.count() << "] ";
  if( !m_old_ui ) ss_ts << "\x1b[39m";

  return ss_ts.str();
}

auto MinerState::getPrintableHashCount() -> uint64_t
{
  return m_hash_count_printable;
}

template<typename T>
auto MinerState::hexToBytes( std::string const hex, T& bytes ) -> void
{
  assert( hex.length() % 2 == 0 );
  // assert( bytes.size() == ( hex.length() / 2 - 1 ) );
  if( hex.substr( 0, 2 ) == "0x"s )
  {
    HexToBytes( hex.substr( 2 ), &bytes[0] );
  }
  else
  {
    HexToBytes( hex, &bytes[0] );
  }
}

template<typename T>
auto MinerState::bytesToString( T const buffer ) -> std::string const
{
  std::string output;
  output.reserve( buffer.size() * 2 + 1 );

  for( auto byte : buffer )
    output += ascii[byte];

  return output;
}

auto MinerState::getSolution() -> std::string const
{
  if( m_solutions_queue.empty() )
    return "";

  uint64_t temp;
  hash_t ret{ m_solution };

  {
    guard lock( m_solutions_mutex );
    temp = m_solutions_queue.front();
    m_solutions_queue.pop();
  }

  std::memcpy( &ret[12], &temp, 8 );
  return bytesToString( ret );
}

auto MinerState::pushSolution( uint64_t const sol ) -> void
{
  guard lock( m_solutions_mutex );
  m_solutions_queue.push( sol );
}

auto MinerState::incSolCount( uint64_t const count ) -> void
{
  m_sol_count += count;
  if( count > 0 ) m_new_solution = true;
}

auto MinerState::getSolCount() -> uint64_t const
{
  return m_sol_count;
}

auto MinerState::setPrefix( std::string const prefix ) -> void
{
  assert( prefix.length() == ( PREFIX_LENGTH * 2 + 2 ) );

  message_t temp;
  hexToBytes( prefix, temp );
  std::memcpy( &temp[52], m_solution.data(), 32 );

  if( temp == m_message ) return;

  {
    guard lock( m_message_mutex );
    std::memcpy( m_challenge_old.data(), m_message.data(), 32 );
    std::memcpy( m_message.data(), temp.data(), 52 );
  }

  {
    guard lock( m_print_mutex );
    m_challenge_printable = prefix.substr( 2, 8 );
  }

  m_message_ready = true;
}

auto MinerState::getPrefix() -> std::string const
{
  prefix_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), m_message.data(), 52 );
  }

  return bytesToString( temp );
}

auto MinerState::setChallenge( std::string challenge ) -> void
{
  hash_t temp;
  hexToBytes( challenge, temp );

  {
    guard lock( m_message_mutex );
    std::memcpy( m_message.data(), temp.data(), 32 );
  }

  {
    guard lock( m_print_mutex );
    m_challenge_printable = challenge.substr( 2, 8 );
  }

  m_message_ready = true;
}

auto MinerState::getChallenge() -> std::string const
{
  hash_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), m_message.data(), 32 );
  }

  return bytesToString( temp );
}

auto MinerState::getPreviousChallenge() -> std::string const
{
  hash_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), m_challenge_old.data(), 32 );
  }

  return bytesToString( temp );
}

auto MinerState::setPoolAddress( std::string address ) -> void
{
  hash_t temp;
  hexToBytes( address, temp );

  {
    guard lock( m_message_mutex );
    std::memcpy( &m_message[32], temp.data(), 20 );
  }

  m_message_ready = true;
}

auto MinerState::getPoolAddress() -> std::string const
{
  address_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), &m_message[32], 20 );
  }

  return bytesToString( temp );
}

auto MinerState::setTarget( std::string target ) -> void
{
  assert( target.length() <= ( UINT256_LENGTH * 2 + 2 ) );

  BigUnsigned temp{ BigUnsignedInABase( target.substr( 2 ), 16 ) };

  if( temp == m_target ) return;

  {
    guard lock( m_target_mutex );
    m_target = temp;
  }

  std::string const t( static_cast<std::string::size_type>( UINT256_LENGTH * 2 + 2 ) - target.length(), '0' );

  uint64_t temp_num{ std::stoull( (t + target.substr( 2 )).substr( 0, 16 ), nullptr, 16 ) };

  m_target_num = temp_num;
}

auto MinerState::getTarget() -> BigUnsigned const
{
  guard lock( m_target_mutex );
  return m_target;
}

auto MinerState::getTargetNum() -> uint64_t const
{
  return m_target_num;
}

auto MinerState::getMaximumTarget() -> BigUnsigned const
{
  return BigUnsignedInABase( m_maximum_target, 16 );
}

auto MinerState::getMessage() -> message_t const
{
  guard lock( m_message_mutex );
  return m_message;
}

auto MinerState::getMidstate() -> state_t const
{
  uint64_t message[11]{ 0 };

  {
    guard lock( m_message_mutex );
    std::memcpy( message, m_message.data(), 84 );
  }

  uint64_t C[5], D[5], mid[25];
  C[0] = message[0] ^ message[5] ^ message[10] ^ 0x100000000ull;
  C[1] = message[1] ^ message[6] ^ 0x8000000000000000ull;
  C[2] = message[2] ^ message[7];
  C[3] = message[3] ^ message[8];
  C[4] = message[4] ^ message[9];

  D[0] = ROTL64(C[1], 1) ^ C[4];
  D[1] = ROTL64(C[2], 1) ^ C[0];
  D[2] = ROTL64(C[3], 1) ^ C[1];
  D[3] = ROTL64(C[4], 1) ^ C[2];
  D[4] = ROTL64(C[0], 1) ^ C[3];

  mid[ 0] = message[ 0] ^ D[0];
  mid[ 1] = ROTL64(message[6] ^ D[1], 44);
  mid[ 2] = ROTL64(D[2], 43);
  mid[ 3] = ROTL64(D[3], 21);
  mid[ 4] = ROTL64(D[4], 14);
  mid[ 5] = ROTL64(message[3] ^ D[3], 28);
  mid[ 6] = ROTL64(message[9] ^ D[4], 20);
  mid[ 7] = ROTL64(message[10] ^ D[0] ^ 0x100000000ull, 3 );
  mid[ 8] = ROTL64(0x8000000000000000ull ^ D[1], 45 );
  mid[ 9] = ROTL64(D[2], 61);
  mid[10] = ROTL64(message[1] ^ D[1],  1);
  mid[11] = ROTL64(message[7] ^ D[2],  6);
  mid[12] = ROTL64(D[3], 25);
  mid[13] = ROTL64(D[4],  8);
  mid[14] = ROTL64(D[0], 18);
  mid[15] = ROTL64(message[4] ^ D[4], 27);
  mid[16] = ROTL64(message[5] ^ D[0], 36);
  mid[17] = ROTL64(D[1], 10);
  mid[18] = ROTL64(D[2], 15);
  mid[19] = ROTL64(D[3], 56);
  mid[20] = ROTL64(message[2] ^ D[2], 62);
  mid[21] = ROTL64(message[8] ^ D[3], 55);
  mid[22] = ROTL64(D[4], 39);
  mid[23] = ROTL64(D[0], 41);
  mid[24] = ROTL64(D[1],  2);

  state_t ret;
  std::memcpy( ret.data(), mid, 200 );
  return ret;
}

auto MinerState::setAddress( std::string const address ) -> void
{
  {
    guard lock( m_address_mutex );
    m_address = address;
  }
  {
    guard lock( m_print_mutex );
    m_address_printable = address.substr( 0, 8 );
  }
}

auto MinerState::getAddress() -> std::string const
{
  guard lock( m_address_mutex );
  return m_address;
}

auto MinerState::setCustomDiff( uint64_t const diff ) -> void
{
  m_custom_diff = true;
  setDiff( diff );
}

auto MinerState::getCustomDiff() -> bool const
{
  return m_custom_diff;
}

auto MinerState::setDiff( uint64_t const diff ) -> void
{
  m_diff = diff;
  m_diff_ready = true;
  BigUnsigned temp{ m_maximum_target / diff };
  setTarget( "0x"s + std::string(BigUnsignedInABase( temp, 16 )) );
}

auto MinerState::getDiff() -> uint64_t const
{
  return m_diff;
}

auto MinerState::setPoolUrl( std::string const pool ) -> void
{
  if( pool.find( "mine0xbtc.eu"s ) != std::string::npos )
  {
    std::cerr << "Selected pool '" << pool << "' is blocked for deceiving the community and apparent scamming.\n"
              << "Please select a different pool to mine on.\n";
    std::exit( EXIT_FAILURE );
  }
  {
    guard lock( m_pool_url_mutex );
    m_pool_url = pool;
  }
}

auto MinerState::getPoolUrl() -> std::string const
{
  guard lock( m_pool_url_mutex );
  return m_pool_url;
}

auto MinerState::getCudaDevices() -> std::vector<std::pair<int32_t, double>> const
{
  return m_cuda_devices;
}

auto MinerState::getCpuThreads() -> uint32_t const
{
  return m_cpu_threads;
}

auto MinerState::setTokenName( std::string const token ) -> void
{
  m_token_name = token;
  std::string temp{ token };
  std::transform( token.begin(),
                  token.end(),
                  temp.begin(),
                  []( uint8_t c ) { return static_cast<char>(std::tolower(c)); } );
  if( temp == "0xcate" || temp == "0xcatether" )
  {
    m_maximum_target <<= 224;
  }
  else
  {
    m_maximum_target <<= 234;
  }
}

auto MinerState::getTokenName() -> std::string const
{
  return m_token_name;
}

auto MinerState::setSubmitStale( bool const submitStale ) -> void
{
  m_submit_stale = submitStale;
}

auto MinerState::getSubmitStale() -> bool const
{
  return m_submit_stale;
}

auto MinerState::isReady() -> bool const
{
  return m_diff_ready && m_message_ready;
}

auto MinerState::keccak256( std::string const message ) -> std::string const
{
  message_t data;
  hexToBytes( message, data );
  sph_keccak256_context ctx;
  sph_keccak256_init( &ctx );
  sph_keccak256( &ctx, data.data(), data.size() );
  hash_t out;
  sph_keccak256_close( &ctx, out.data() );
  return bytesToString( out );
}
