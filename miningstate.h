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

#if !defined _MININGSTATE_H_
#define _MININGSTATE_H_

#include "types.h"
#include "BigInt/BigUnsigned.hh"

#include <cstdint>
#include <atomic>
#include <mutex>
#include <string>

class MiningState
{
public:
  MiningState();
  ~MiningState();

  auto getIncSearchSpace( uint64_t const& threads ) -> uint64_t const;
  auto resetCounter() -> void;
  auto getSolution() -> std::string const;
  auto getAllSolutions() -> std::vector<std::string> const;
  template<typename T>
  auto pushSolution( T const sols ) -> void;
  auto incSolCount( uint64_t const& count ) -> void;
  auto getSolCount() -> uint64_t const;
  auto getSolNew() -> bool const;
  auto getPrefix() -> std::string const;
  auto getOldPrefix() -> std::string const;
  auto setChallenge( std::string_view const challenge ) -> void;
  auto getChallenge() -> std::string const;
  auto getPreviousChallenge() -> std::string const;
  auto setPoolAddress( std::string_view const address ) -> void;
  auto getPoolAddress() -> std::string const;
  auto setTarget( BigUnsigned const& target ) -> void;
  auto getTarget() -> BigUnsigned const;
  auto getTargetNum() -> uint64_t const;
  auto getMaximumTarget() -> BigUnsigned const&;
  auto getMessage() -> message_t const;
  auto setMidstate() -> void;
  auto getMidstate() -> state_t const;
  auto setAddress( std::string_view const address ) -> void;
  auto getAddress() -> std::string const;
  auto setCustomDiff( uint64_t const& diff ) -> void;
  auto getCustomDiff() -> bool const&;
  auto setDiff( uint64_t const& diff ) -> void;
  auto getDiff() -> uint64_t const;
  auto setPoolUrl( std::string_view const pool ) -> void;
  auto getPoolUrl() -> std::string const;

private:
  message_t m_message;
  state_t m_midstate;
  std::atomic<bool> m_midstate_ready;
  std::mutex m_midstate_mutex;
  hash_t m_challenge_old;
  std::mutex m_message_mutex;
  std::atomic<bool> m_challenge_ready;
  std::atomic<bool> m_pool_address_ready;
  std::atomic<uint64_t> m_sol_count;
  std::atomic<uint64_t> m_target_num;
  BigUnsigned m_target;
  BigUnsigned m_maximum_target;
  std::mutex m_target_mutex;
  bool m_custom_diff;
  std::atomic<uint64_t> m_diff;
  std::atomic<bool> m_diff_ready;
  std::string m_address;
  std::mutex m_address_mutex;
  std::string m_pool_url;
  std::mutex m_pool_url_mutex;
  std::mutex m_solutions_mutex;
  std::vector<uint64_t> m_solutions_queue;
  hash_t m_solution;
  std::atomic<uint64_t> m_hash_count;
};

#endif // _MININGSTATE_H_
