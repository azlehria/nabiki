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

#include "types.h"
#include "platforms.h"
#include "log.h"

#include <string>
#include <sstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <atomic>
#include <queue>

using namespace std::chrono;
using namespace std::string_literals;

namespace
{
  static std::mutex m_log_mutex;
  static std::atomic<uint64_t> m_hash_count_printable;
  static std::queue<std::string> m_log;
}

namespace Log
{
  auto getLog() -> std::string const
  {
    std::stringstream outstream;
    {
      if( m_log.size() == 0 ) { return ""s; }

      guard lock( m_log_mutex );

      do
      {
        outstream << m_log.front();
        m_log.pop();
      }
      while( m_log.size() > 0 );
    }

    return outstream.str();
  }

  auto getPrintableTimeStamp() -> std::string const
  {
    std::stringstream ss_ts;

    auto now{ system_clock::now() };
    auto now_ms{ duration_cast<milliseconds>(now.time_since_epoch()) % 1000 };
    std::time_t tt_ts{ system_clock::to_time_t( now ) };
    std::tm tm_ts{ *std::localtime( &tt_ts ) };

    if( !UseSimpleUI ) { ss_ts << "\x1b[90m"; }
    ss_ts << std::put_time( &tm_ts, "[%T" ) << '.'
      << std::setw( 3 ) << std::setfill( '0' ) << now_ms.count() << "] ";
    if( !UseSimpleUI ) { ss_ts << "\x1b[39m"; }

    return ss_ts.str();
  }

  auto pushLog( std::string message ) -> void
  {
    guard lock( m_log_mutex );
    m_log.push( getPrintableTimeStamp() + message + '\n' );
  }
}
