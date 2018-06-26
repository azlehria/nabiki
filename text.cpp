#include "text.h"
#include "miner_state.h"
#include "hybridminer.h"
#include "platforms.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>
#include <thread>

using namespace std::chrono;
using namespace std::literals;

namespace
{
  static std::function<std::ostream&( duration<uint32_t> const&, double const&, std::ostream& )> printer;

  static double hashrate{ 0 };
  static seconds timer;
  static time_point<steady_clock> timerNext;

  static std::atomic<double> m_stop{ false };

  static std::thread worker;

  static auto printUiBase() -> void
  {
    if( UseOldUI() )
      return;

    uint_fast16_t const logTop{ 5u + (MinerState::isDebug() ? HybridMiner::getActiveDeviceCount() : 0u) };

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
              << "\x1b[1;64f"sv << MINER_VERSION.substr( 7 )
              << "\x1b]2;"sv << MINER_VERSION << "\x07"sv
              << "\x1b[" << logTop << "r\x1b[" << logTop << ";1f"sv;
  }

  static auto printUiOldWindows( seconds const& time, double const& rate, std::ostream& ss_out ) -> std::ostream&
  {
    // print every every 5 seconds . . . more or less
    static auto time_counter{ steady_clock::now() + 5s };
    if( time_counter > steady_clock::now() ) return ss_out;
    time_counter = steady_clock::now() + 5s;

    ss_out << MinerState::getPrintableTimeStamp() << std::setfill( ' ' )
           << std::setw( 10 ) << std::fixed << std::setprecision( 2 )
           << rate / 1000000
           << " MH/s  Sols:"sv
           << std::setw( 6 ) << MinerState::getSolCount()
           << ( MinerState::getSolNew() ? '^' : ' ' )
           << " Search time: "sv
           << std::setfill( '0' )
           << std::setw( 2 ) << duration_cast<minutes>( time ).count() << ":"sv
           << std::setw( 2 ) << time.count()
           << '\n';

    return ss_out;
  }

  static auto printUi( seconds const& time, double const& rate, std::ostream& ss_out ) -> std::ostream&
  {
    // print every every 100 milliseconds . . . more or less
    static auto time_counter{ steady_clock::now() + 100ms };
    if( time_counter > steady_clock::now() ) return ss_out;
    time_counter = steady_clock::now() + 100ms;

    // maybe breaking the control codes into macros is a good idea . . .
    ss_out << "\x1b[s\x1b[2;75f\x1b[38;5;33m"sv
           << std::setfill( '0' )
           << std::setw( 2 ) << duration_cast<minutes>( time ).count() << ":"sv
           << std::setw( 2 ) << time.count() % 60
           << "\x1b[2;22f\x1b[38;5;221m"sv
           << std::setw( 8 ) << std::setfill( ' ' ) << std::fixed << std::setprecision( 2 )
           << rate / 1000000
           << "\x1b[3;22f\x1b[38;5;221m"sv
           << std::setw( 8 ) << MinerState::getSolCount()
           << "\x1b[3;14f\x1b[38;5;34m"sv
           << MinerState::getDiff()
           << "\x1b[3;72f\x1b[38;5;33m"sv
           << MinerState::getPrintableAddress()
           << "\x1b[2;13f\x1b[38;5;34m"sv
           << MinerState::getPrintableChallenge();
    if( MinerState::isDebug() )
    {
      uint_fast32_t line{ 5u };
      for( auto& device : HybridMiner::getDeviceStates() )
      {
        ss_out << "\x1b[0m\x1b["sv << line << ";0f"sv << device.name.substr( 12 )
               << "\x1b["sv << line << ";9f"sv << device.temperature << " C"sv
               << std::setw( 10 ) << std::setprecision( 2 ) << device.hashrate / 1000000
               << " MH/s\t"sv << device.core << " MHz\t"sv << device.memory << " MHz\t"sv
               << std::setw( 3 ) << device.fan << "%\t"sv << device.power << "W\n"sv;
        ++line;
      }
    }
    ss_out.imbue( std::locale( "" ) );
    ss_out << "\x1b[3;36f\x1b[38;5;208m"sv
           << std::setw( 25 ) << MinerState::getPrintableHashCount()
           << "\x1b[0m\x1b[u"sv;

    return ss_out;
  }

  auto printWorker() -> void
  {
    printUiBase();

    while( !m_stop.load( std::memory_order_acquire ) )
    {
      timerNext = steady_clock::now() + 1ms;

      timer = duration_cast<seconds>( steady_clock::now() - MinerState::getRoundStartTime() );

      hashrate = 0;
      for( auto& temp : HybridMiner::getHashrates() )
      {
        hashrate += temp;
      }

      printer( timer, hashrate, MinerState::getLog( std::cout ) );

      std::this_thread::sleep_until( timerNext );
    }
  }
}

namespace UI::Text
{
  auto Init() -> void
  {
    printer = std::bind( ( UseOldUI() ? ::printUiOldWindows : ::printUi ),
                         std::placeholders::_1,
                         std::placeholders::_2,
                         std::placeholders::_3 );

    worker = std::thread{ printWorker };
  }

  auto Cleanup() -> void
  {
    m_stop.store( false, std::memory_order_release );

    if( worker.joinable() )
      worker.join();
  }
}
