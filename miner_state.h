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

#if !defined _MINER_STATE_H_
#define _MINER_STATE_H_

#include "types.h"
#include "BigInt/BigIntegerLibrary.hh"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>

// this really needs to be broken down
namespace MinerState
{
  using namespace std::chrono;
  using std::string;
  using std::string_view;

  auto Init() -> void;

  auto getIncSearchSpace( uint64_t const& threads ) -> uint64_t const;
  auto resetCounter() -> void;
  auto getPrintableHashCount() -> uint64_t const;
  auto getRoundStartTime() -> time_point<steady_clock> const&;

  template<typename T>
  auto pushSolution( T const sols ) -> void;
  auto getSolution() -> string;
  auto getAllSolutions() -> std::vector<std::string>;
  auto incSolCount( uint64_t const& count = 1 ) -> void;
  auto getSolCount() -> uint64_t const;

  auto setTarget( BigUnsigned const& target ) -> void;
  auto getTarget() -> BigUnsigned const;
  auto getTargetNum() -> uint64_t const;
  auto getMaximumTarget() -> BigUnsigned const&;

  auto getPrefix() -> string const;
  auto getOldPrefix() -> string const;
  auto setChallenge( string_view const challenge ) -> void;
  auto getChallenge() -> string const;
  auto getPreviousChallenge() -> string const;
  auto setPoolAddress( string_view const address ) -> void;
  auto getPoolAddress() -> string const;
  auto getMessage() -> message_t const;
  auto setMidstate() -> void;
  auto getMidstate() -> state_t const;

  auto setAddress( string_view const address ) -> void;
  auto getAddress() -> string const;

  auto setCustomDiff( uint64_t const& diff ) -> void;
  auto getCustomDiff() -> bool const&;
  auto setDiff( uint64_t const& diff ) -> void;
  auto getDiff() -> uint64_t const;

  auto setPoolUrl( string_view const pool ) -> void;
  auto getPoolUrl() -> string const;

  auto getCudaDevices() -> device_list_t const&;
  auto getClDevices() -> device_map_t const&;
  auto getCpuThreads() -> uint32_t const&;

  auto setTokenName( string_view const token ) -> void;

  auto setSubmitStale( bool const& submitStale ) -> void;
  auto getSubmitStale() -> bool const&;

  auto getWorkerName() -> string_view;

  auto getTelemetryPorts() -> string_view;
  auto getTelemetryAcl() -> string_view;

  auto waitUntilReady() -> void;
  auto isDebug() -> bool const&;
}

#endif // !_MINER_STATE_H_
