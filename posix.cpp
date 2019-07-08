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

#include "platforms.h"
#include "minercore.h"
#include "log.h"

#include <string>
#include <array>
#include <thread>
#include <cstdint>
#include <cstring>
#include <signal.h>
#include <termios.h>
#include <cpuid.h>

using namespace std::literals;

bool UseSimpleUI = false;

namespace
{
  sigset_t set;
  termios term_original;
  std::thread sig_handler;

  auto SignalHandler() -> void
  {
    pthread_sigmask( SIG_BLOCK, &set, NULL );
    int32_t sig;
    sigwait( &set, &sig );

    tcsetattr( 0, TCSAFLUSH, &term_original );

    MinerCore::stop();
    return;
  }
}

auto InitBaseState() -> void
{
  sigemptyset( &set );

  // fatal interactive signals
  sigaddset( &set, SIGINT );
  sigaddset( &set, SIGTERM );
  sigaddset( &set, SIGHUP );
  sigaddset( &set, SIGQUIT );
  sigaddset( &set, SIGABRT );
  sigaddset( &set, SIGIOT );

  // errors
  sigaddset( &set, SIGBUS );
  sigaddset( &set, SIGFPE );
  sigaddset( &set, SIGILL );
  sigaddset( &set, SIGSEGV );
  sigaddset( &set, SIGSYS );

  pthread_sigmask( SIG_BLOCK, &set, NULL );

  termios term_new;
  tcgetattr( 0, &term_original );
  std::memcpy( &term_new, &term_original, sizeof( termios ) );
  term_new.c_lflag &= ~ECHO;
  tcsetattr( 0, TCSANOW, &term_new );

  sig_handler = std::thread( SignalHandler );
}

auto CleanupBaseState() -> void
{
  if( sig_handler.joinable() )
    sig_handler.join();
}

auto GetRawCpuName() -> std::string
{
  std::array<uint32_t, 4> ret;
  ret[0] = __get_cpuid_max( 0x80000000, NULL );
  if( ret[0] < 0x80000004 )
  {
    // detect AMD/Intel
    __get_cpuid_max( 0, &ret[1] );
    return "Unknown "s + (ret[1] == 0x756e6547 ? "Intel "s : (ret[1] == 0x68747541 ? "AMD "s : ""s)) + "CPU"s;
  }
  std::string out;
  out.reserve( 48u );
  for( uint32_t i{ 0x80000002 }; i < 0x80000005; ++i )
  {
    __cpuid( i, ret[0], ret[1], ret[2], ret[3] );
    out.append( reinterpret_cast<char*>(ret.data()), 16u );
  }
  return out;
}
