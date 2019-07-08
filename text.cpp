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

#include "ui.h"
#include "log.h"
#include "miner_state.h"
#include "minercore.h"
#include "platforms.h"
#include "events.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <functional>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>

using namespace std::chrono;
using namespace std::literals;
using namespace Nabiki::Events;

namespace
{
  static double hashrate{ 0 };
  static seconds timer{};
  static time_point<steady_clock> timerNext{};
  static std::locale systemLocale( "" );

  static std::string sAddress{};
  static uint64_t lDiff{};
  static std::string sChallenge{};
  static std::string sPoolUrl{};
  static double dHashrate{};
  static uint64_t lHashCount{};
  static uint64_t lSolutions{};
  static uint64_t lDevSolutions{};
  static bool bNewSolution{ false };

  static std::atomic<double> m_stop{ false };

  static std::thread worker;

  enum : int32_t
  {
    UI_UPDATE_CHALLENGE = 1,
    UI_UPDATE_DIFFICULTY,
    UI_UPDATE_URL,
    UI_UPDATE_ADDRESS,
    UI_UPDATE_HASHRATE,
    UI_UPDATE_SOLUTIONS,
    UI_UPDATE_DEV_SOLUTIONS,
    UI_PRINT_BASE,
  };

  static size_t constexpr qEvents{ 1 };

  static auto printUiBase() -> void
  {
    uint_fast16_t const logTop{ 5u + (MinerState::isDebug() ? MinerCore::getActiveDeviceCount() : 0u) };

    std::cout << "\x1b[?25l\x1b[2J\x1b(0"sv
              << "\x1b[1;1flqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqk"sv
              << "\x1b[4;1fmqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqj"sv
              << "\x1b[2;1fx\x1b[2;35fx\x1b[2;62fx\x1b[2;80fx"sv
              << "\x1b[3;1fx\x1b[3;35fx\x1b[3;62fx\x1b[3;80fx"sv
              << "\x1b(B\x1b[2;2fChallenge:"sv
              << "\x1b[3;2fDifficulty:"sv
              << "\x1b[2;37fHashes this round"sv
              << "\x1b[2;63fRound time:"sv
              << "\x1b[3;63fAccount:"sv
              << "\x1b[2;31fMH/s"sv
              << "\x1b[3;31fSols"sv
              << "\x1b[s\x1b[3;29f\x1b[38;5;221m0\x1b[0m\x1b[u"sv
              << "\x1b[1;64f"sv << MinerCore::MINER_VERSION.substr( 7 )
              << "\x1b]2;"sv << MinerCore::MINER_VERSION << "\x07"sv
              << "\x1b[" << logTop << "r\x1b[" << logTop << ";1f"sv;
  }

  static auto printUiSimple() -> void
  {
    timer = duration_cast<seconds>(steady_clock::now() - MinerState::getRoundStartTime());

    static auto time_counter{ steady_clock::now() + 1s };
    if( time_counter > steady_clock::now() ) return;
    //time_counter = steady_clock::now() + 5s;

    std::stringstream ss_out;

    ss_out << Log::getPrintableTimeStamp() << std::setfill( ' ' )
           << std::setw( 10 ) << std::fixed << std::setprecision( 2 )
           << dHashrate
           << " MH/s  Sols:"sv
           << std::setw( 6 ) << lSolutions;
    if( bNewSolution )
    {
      ss_out << "^\n";
      bNewSolution = false;
    }
    else
    {
      ss_out << "  Search time: "sv
             << std::setfill( '0' )
             << std::setw( 2 ) << duration_cast<minutes>( timer ).count() << ":"sv
             << std::setw( 2 ) << timer.count() % 60
             << '\r';
    }

    std::cout << ss_out.str();
  }

  static auto printUi( queue_message_t const& message ) -> void
  {
    std::stringstream ss_out;

    // maybe breaking the control codes into macros is a good idea . . .
    switch( message.message )
    {
      case UI_UPDATE_SOLUTIONS:
        ss_out << "\x1b[s\x1b[3;22f\x1b[38;5;221m"sv
               << std::setw( 8 ) << lSolutions
               << "\x1b[0m\x1b[u"sv;
        break;
      case UI_UPDATE_DIFFICULTY:
        ss_out << "\x1b[s\x1b[3;14f\x1b[38;5;34m"sv
               << lDiff
               << "\x1b[0m\x1b[u"sv;
        break;
      case UI_UPDATE_ADDRESS:
        ss_out << "\x1b[s\x1b[3;72f\x1b[38;5;33m"sv
               << sAddress
               << "\x1b[0m\x1b[u"sv;
        break;
      case UI_UPDATE_CHALLENGE:
        ss_out << "\x1b[s\x1b[2;13f\x1b[38;5;34m"sv
               << sChallenge
               << "\x1b[0m\x1b[u"sv;
        break;
      case UI_UPDATE_HASHRATE:
      {
        timer = duration_cast<seconds>( steady_clock::now() - MinerState::getRoundStartTime() );

        ss_out << "\x1b[s\x1b[2;75f\x1b[38;5;33m"sv
               << std::setfill( '0' )
               << std::setw( 2 ) << duration_cast<minutes>( timer ).count() << ":"sv
               << std::setw( 2 ) << timer.count() % 60
               << "\x1b[2;22f\x1b[38;5;221m"sv
               << std::setw( 8 ) << std::setfill( ' ' ) << std::fixed << std::setprecision( 2 )
               << dHashrate;
        if( MinerState::isDebug() )
        {
          uint_fast32_t line{ 5u };
          for( auto const& device : MinerCore::getDeviceReferences() )
          {
            ss_out << "\x1b[0m\x1b["sv << line << ";0f"sv << device->getName().substr( 8 )
                   << "\x1b["sv << line << ";16f"sv << device->getTemperature() << " C"sv
                   << std::setw( 11 ) << std::setprecision( 2 ) << device->getHashrate()
                   << " MH/s"sv << std::setw( 8 ) << device->getClockCore() << " MHz"sv
                   << std::setw( 8 ) << device->getClockMem() << " MHz"sv
                   << std::setw( 4 ) << device->getFanSpeed() << "%"sv
                   << std::setw( 5 ) << device->getPowerWatts() << "W\n"sv;
            ++line;
          }
        }
        // avoid C26444 warning
        (void)ss_out.imbue( systemLocale );
        ss_out << "\x1b[3;36f\x1b[38;5;208m"sv
               << std::setw( 25 ) << lHashCount
               << "\x1b[0m\x1b[u"sv;
        break;
      }
      case UI_PRINT_BASE:
        printUiBase();
        return;
      default:
        return;
    }
    std::cout << ss_out.str() << Log::getLog();
  }
}

namespace UI
{
  auto Run() -> int32_t
  {
    queue_message_t message;

    while( GetEvent( message, qEvents ) > NABIKI_EVENT_STOP )
    {
      if( UseSimpleUI )
      {
        printUiSimple();
      }
      else
      {
        printUi( message );
      }
    }

    if( !UseSimpleUI )
    {
      std::cerr << "\x1b[s\x1b[?25h\x1b[r\x1b[u"sv;
    }

    return 0;
  }

  auto Stop() -> void
  {
    SendEvent( NABIKI_EVENT_STOP, qEvents );
  }

  auto Init() -> void
  {
    SendEvent( UI_PRINT_BASE, qEvents );
  }

  auto UpdateChallenge( std::string_view const challenge ) -> void
  {
    sChallenge = challenge;
    SendEvent( UI_UPDATE_CHALLENGE, qEvents );
  }

  auto UpdateDifficulty( uint64_t const& diff ) -> void
  {
    lDiff = diff;
    SendEvent( UI_UPDATE_DIFFICULTY, qEvents );
  }

  auto UpdateUrl( std::string_view const& url ) -> void
  {
    sPoolUrl = url;
    SendEvent( UI_UPDATE_URL, qEvents );
  }

  auto UpdateAddress( std::string_view const address ) -> void
  {
    sAddress = address;
    SendEvent( UI_UPDATE_ADDRESS, qEvents );
  }

  auto UpdateHashrate( uint64_t const& count ) -> void
  {
    // print every every 100 milliseconds . . . more or less
    static auto time_counter{ steady_clock::now() + 100ms };
    if( time_counter > steady_clock::now() ) return;
    time_counter = steady_clock::now() + 100ms;

    hashrate = 0;
    for( auto const& temp : MinerCore::getHashrates() )
    {
      hashrate += temp;
    }

    dHashrate = hashrate;
    lHashCount = count;
    SendEvent( UI_UPDATE_HASHRATE, qEvents );
  }

  auto UpdateSolutions( uint64_t const& count ) -> void
  {
    lSolutions = count;
    bNewSolution = true;
    SendEvent( UI_UPDATE_SOLUTIONS, qEvents );
  }

  auto UpdateDevSolutions( uint64_t const& count ) -> void
  {
    lDevSolutions = count;
    SendEvent( UI_UPDATE_DEV_SOLUTIONS, qEvents );
  }
}
