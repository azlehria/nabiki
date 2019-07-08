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

#include "miningstate.h"
#include "types.h"
#include "BigInt/BigUnsigned.hh"
#include "ui.h"
#include "utils.h"
#include "platforms.h"

#include <cstdint>
#include <atomic>
#include <mutex>
#include <string>
#include <iostream>

using namespace Nabiki::Utils;
using namespace std::string_view_literals;
using namespace std::chrono;

namespace
{
  static std::atomic<uint64_t> m_hash_count_printable;
}

MiningState::MiningState() :
  m_midstate_ready( false ),
  m_challenge_ready( false ),
  m_pool_address_ready( false ),
  m_sol_count( 0u ),
  m_target_num( 0u ),
  m_target( 0u ),
  m_maximum_target( 0u ),
  m_custom_diff( false ),
  m_diff( 1u ),
  m_diff_ready( false ),
  m_hash_count( 0u )
{}

MiningState::~MiningState()
{}

auto MiningState::getIncSearchSpace( uint64_t const& threads ) -> uint64_t const
{
  UI::UpdateHashrate( m_hash_count_printable.load( std::memory_order_acquire ) );

  m_hash_count_printable.fetch_add( threads, std::memory_order_acq_rel );
  return m_hash_count.fetch_add( threads, std::memory_order_acq_rel );
}

auto MiningState::resetCounter() -> void
{
  m_hash_count_printable.store( 0ull, std::memory_order_release );

  m_round_start = steady_clock::now();
}

auto MiningState::getSolution() -> std::string const
{
  if( m_solutions_queue.empty() )
    return ""s;

  static hash_t ret{ m_solution };

  {
    guard lock( m_solutions_mutex );
    std::memcpy( &ret[12], &m_solutions_queue.back(), 8 );
    m_solutions_queue.pop_back();
  }

  return bytesToString( ret );
}

auto MiningState::getAllSolutions() -> std::vector<std::string> const
{
  std::vector<std::string> retVec;
  if( m_solutions_queue.empty() )
    return retVec;

  static hash_t ret{ m_solution };

  {
    guard lock( m_solutions_mutex );
    for( auto const& val : m_solutions_queue )
    {
      std::memcpy( &ret[12], &val, 8 );
      retVec.emplace_back( bytesToString( ret ) );
    }
    std::vector<uint64_t>().swap( m_solutions_queue );
  }

  return retVec;
}

template<typename T>
auto MiningState::pushSolution( T const sols ) -> void
{
  guard lock( m_solutions_mutex );
  if constexpr( std::is_integral_v<T> )
  {
    m_solutions_queue.emplace_back( sols );
  }
  else
  {
    m_solutions_queue.insert( m_solutions_queue.end(), sols.begin(), sols.end() );
  }
}

auto MiningState::incSolCount( uint64_t const& count ) -> void
{
  m_sol_count.fetch_add( count, std::memory_order_acq_rel );
  if( count > 0 ) m_new_solution.store( true, std::memory_order_release );

  UI::UpdateSolutions( m_sol_count.load( std::memory_order_acquire ) );
}

auto MiningState::getSolCount() -> uint64_t const
{
  return m_sol_count.load( std::memory_order_acquire );
}

auto MiningState::getSolNew() -> bool const
{
  return m_new_solution.exchange( false, std::memory_order_acq_rel );
}

auto MiningState::getPrefix() -> std::string const
{
  prefix_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), m_message.data(), 52 );
  }

  return bytesToString( temp );
}

auto MiningState::getOldPrefix() -> std::string const
{
  prefix_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), m_challenge_old.data(), 32 );
    std::memcpy( &temp[32], m_message.data(), 20 );
  }

  return bytesToString( temp );
}

auto MiningState::setChallenge( std::string_view const challenge ) -> void
{
  hash_t temp;
  hexToBytes( challenge, temp );

  {
    guard lock( m_message_mutex );

    if( getSubmitStale() )
      std::memcpy( m_challenge_old.data(), m_message.data(), 32 );

    std::memcpy( m_message.data(), temp.data(), 32 );
  }

  if( !getSubmitStale() )
  {
    guard lock( m_solutions_mutex );
    std::vector<uint64_t>().swap( m_solutions_queue );
  }

  UI::UpdateChallenge( challenge.substr( 2, 8 ) );

  m_challenge_ready.store( true, std::memory_order_release );
  setMidstate();
}

auto MiningState::getChallenge() -> std::string const
{
  hash_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), m_message.data(), 32 );
  }

  return bytesToString( temp );
}

auto MiningState::getPreviousChallenge() -> std::string const
{
  hash_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), m_challenge_old.data(), 32 );
  }

  return bytesToString( temp );
}

auto MiningState::setPoolAddress( std::string_view const address ) -> void
{
  hash_t temp;
  hexToBytes( address, temp );

  {
    guard lock( m_message_mutex );
    std::memcpy( &m_message[32], temp.data(), 20 );
  }

  m_pool_address_ready.store( true, std::memory_order_release );
  setMidstate();
}

auto MiningState::getPoolAddress() -> std::string const
{
  address_t temp;

  {
    guard lock( m_message_mutex );
    std::memcpy( temp.data(), &m_message[32], 20 );
  }

  return bytesToString( temp );
}

auto MiningState::setTarget( BigUnsigned const& target ) -> void
{
  if( target == m_target ) return;

  {
    guard lock( m_target_mutex );
    m_target = target;
  }

  m_target_num.store( target.getBlock( 3 ), std::memory_order_release );
}

auto MiningState::getTarget() -> BigUnsigned const
{
  guard lock( m_target_mutex );
  return m_target;
}

auto MiningState::getTargetNum() -> uint64_t const
{
  return m_target_num.load( std::memory_order_acquire );
}

auto MiningState::getMaximumTarget() -> BigUnsigned const&
{
  return m_maximum_target;
}

auto MiningState::getMessage() -> message_t const
{
  guard lock( m_message_mutex );
  return m_message;
}

auto MiningState::setMidstate() -> void
{
  if( !m_challenge_ready || !m_pool_address_ready ) return;

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
    std::memcpy( m_midstate.data(), mid, 200 );
  }
}

auto MiningState::getMidstate() -> state_t const
{
  if( !m_midstate_ready ) setMidstate();

  guard lock( m_midstate_mutex );
  return m_midstate;
}

auto MiningState::setAddress( std::string_view const address ) -> void
{
  {
    guard lock( m_address_mutex );
    m_address = address;
  }

  UI::UpdateAddress( address.substr( 0, 8 ) );
}

auto MiningState::getAddress() -> std::string const
{
  guard lock( m_address_mutex );
  return m_address;
}

auto MiningState::setCustomDiff( uint64_t const& diff ) -> void
{
  m_custom_diff = true;
  setDiff( diff );
}

auto MiningState::getCustomDiff() -> bool const&
{
  return m_custom_diff;
}

auto MiningState::setDiff( uint64_t const& diff ) -> void
{
  m_diff.store( diff, std::memory_order_release );
  m_diff_ready.store( true, std::memory_order_release );
  setTarget( m_maximum_target / diff );

  UI::UpdateDifficulty( diff );
}

auto MiningState::getDiff() -> uint64_t const
{
  return m_diff.load( std::memory_order_acquire );
}

auto MiningState::setPoolUrl( std::string_view const pool ) -> void
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

auto MiningState::getPoolUrl() -> std::string const
{
  guard lock( m_pool_url_mutex );
  return m_pool_url;
}
