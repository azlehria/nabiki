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

#if defined _MSC_VER

#include "platforms.h"
#include "minercore.h"
#include "miner_state.h"
#include "winapi_helpers.h"

#define NOATOM
#define NOCLIPBOARD
#define NOKERNEL
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOSERVICE
#define NOSOUND
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NOMCX
#define _AMD64_
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <array>
#include <string>

long NTAPI NtQueryTimerResolution( uint32_t* MinimumResolution, uint32_t* MaximumResolution, uint32_t* CurrentResolution );
long NTAPI NtSetTimerResolution( uint32_t DesiredResolution, BOOLEAN SetResolution, uint32_t CurrentResolution );

using namespace std::literals;

bool UseSimpleUI = false;
NUMBERFMTW fmtInt{ 0 };
NUMBERFMTW fmtFloat{ 0 };

namespace
{
  static wchar_t decimalSeparator[4];
  static wchar_t groupSeparator[4];
  static uint32_t minTimerRes{ 0 };
  static uint32_t maxTimerRes{ 0 };
  static uint32_t prevTimerRes{ 0 };
  static HMODULE ntdll;
  static decltype(NtQueryTimerResolution)* QueryTimerResolution;
  static decltype(NtSetTimerResolution)* SetTimerResolution;

  static auto MakeLocale() -> void
  {
    wchar_t grouping[4];

    GetLocaleInfoW( LOCALE_USER_DEFAULT,
                    LOCALE_RETURN_NUMBER | LOCALE_ILZERO,
                    reinterpret_cast<wchar_t*>(&fmtInt.LeadingZero),
                    sizeof( fmtInt.LeadingZero ) / sizeof( wchar_t ) );
    GetLocaleInfoW( LOCALE_USER_DEFAULT,
                    LOCALE_RETURN_NUMBER | LOCALE_SGROUPING,
                    grouping, 4 );
    GetLocaleInfoW( LOCALE_USER_DEFAULT,
                    LOCALE_SDECIMAL,
                    decimalSeparator, 4 );
    GetLocaleInfoW( LOCALE_USER_DEFAULT,
                    LOCALE_STHOUSAND,
                    groupSeparator, 4 );
    GetLocaleInfoW( LOCALE_USER_DEFAULT,
                    LOCALE_RETURN_NUMBER | LOCALE_INEGNUMBER,
                    reinterpret_cast<wchar_t*>(&fmtInt.NegativeOrder),
                    sizeof( fmtInt.NegativeOrder ) / sizeof( wchar_t ) );

    if( wcscmp( grouping, L"3" ) ) // WTF even is this?
      fmtInt.Grouping = 30;
    else if( wcscmp( grouping, L"3;2;0" ) ) // Indic
      fmtInt.Grouping = 32;
    else // if( wcscmp( grouping, L"3;0" ) ) // default
      fmtInt.Grouping = 3;

    fmtInt.lpDecimalSep = decimalSeparator;
    fmtInt.lpThousandSep = groupSeparator;

    memcpy( &fmtFloat, &fmtInt, sizeof( fmtInt ) );

    fmtInt.NumDigits = 0;
    fmtFloat.NumDigits = 2;
  }

  auto __stdcall SignalHandler( DWORD dwSig ) noexcept -> int32_t
  {
    switch( dwSig )
    {
    case CTRL_C_EVENT: [[fallthrough]];
    case CTRL_BREAK_EVENT: [[fallthrough]];
    case CTRL_CLOSE_EVENT:
      try
      {
        MinerCore::stop();
      }
      catch( ... )
      {
        //do something here? we're killing everything anyway . . .
      }
      return TRUE;
    default:
      return FALSE;
    }
  }
}

auto __stdcall wWinMain( [[maybe_unused]] _In_ HINSTANCE hInstance,
                         [[maybe_unused]] _In_opt_ HINSTANCE hPrevInstance,
                         [[maybe_unused]] _In_ wchar_t* lpCmdLine,
                         [[maybe_unused]] _In_ int32_t nCmdShow ) -> int32_t
{
  return MinerCore::run();
}

auto InitBaseState() -> void
{
  wchar_t path[MAX_PATH];
  ExpandEnvironmentStringsW( LR"(%ProgramFiles%\NVIDIA Corporation\NVSMI)", path, MAX_PATH );
  SetDllDirectoryW( path );

  MakeLocale();

  //ntdll = GetModuleHandleW( L"ntdll" );
  //if( ntdll )
  //{
  //  QueryTimerResolution = reinterpret_cast<decltype(NtQueryTimerResolution)*>(GetProcAddress( ntdll, "NtQueryTimerResolution" ));
  //  SetTimerResolution = reinterpret_cast<decltype(NtSetTimerResolution)*>(GetProcAddress( ntdll, "NtSetTimerResolution" ));
  //  QueryTimerResolution( &minTimerRes, &maxTimerRes, &prevTimerRes );
  //}

  OSVERSIONINFOEXW winVer{ sizeof( OSVERSIONINFOEXW ), 10, 0, 10586 };
  uint64_t winVerMask{ VerSetConditionMask( NULL, VER_MAJORVERSION, VER_GREATER_EQUAL ) };
  winVerMask = VerSetConditionMask( winVerMask, VER_BUILDNUMBER, VER_GREATER_EQUAL );
  //uint64_t winVerMask{ 0 };
  //VER_SET_CONDITION( winVerMask, VER_MAJORVERSION, VER_GREATER_EQUAL );
  //VER_SET_CONDITION( winVerMask, VER_BUILDNUMBER, VER_GREATER_EQUAL );
  UseSimpleUI = !VerifyVersionInfoW( &winVer, VER_MAJORVERSION | VER_BUILDNUMBER, winVerMask );

  HANDLE hOut{ GetStdHandle( STD_OUTPUT_HANDLE ) };
  if( hOut == INVALID_HANDLE_VALUE ) { return; }

  DWORD dwMode{ 0 };
  GetConsoleMode( hOut, &dwMode );
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode( hOut, dwMode );
  SetConsoleTitleW( std::wstring( MinerCore::MINER_VERSION.begin(), MinerCore::MINER_VERSION.end() ).c_str() );
  SetConsoleCtrlHandler( SignalHandler, TRUE );

  SetConsoleOutputCP( CP_UTF8 );
}

auto CleanupBaseState() -> void {}

auto GetRawCpuName() -> std::string
{
  std::array<int32_t, 4> registers;
  CpuIdEx( registers.data(), 0x80000000, 0 );
  if( registers[0] < 0x80000004 )
  {
    // detect AMD/Intel
    CpuIdEx( registers.data(), 0, 0 );
    return "Unknown "s + (registers[1] == 0x756e6547 ? "Intel "s : (registers[1] == 0x68747541 ? "AMD "s : ""s)) + "CPU"s;
  }
  std::string out;
  out.reserve( 48u );
  for( uint32_t i{ 0x80000002 }; i < 0x80000005; ++i )
  {
    CpuIdEx( registers.data(), static_cast<int32_t>(i), 0 );
    out.append( reinterpret_cast<char*>(registers.data()), 16u );
  }
  return out;
}

#endif // _MSC_VER
