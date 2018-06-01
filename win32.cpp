#include "platforms.h"
#include "hybridminer.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

auto SetBasicState() -> void
{
  char path[MAX_PATH];
  ExpandEnvironmentStringsA( "%ProgramFiles%\\NVIDIA Corporation\\NVSMI", path, MAX_PATH );
  SetDllDirectory( path );

  HANDLE hOut = GetStdHandle( STD_OUTPUT_HANDLE );
  DWORD dwMode = 0;
  GetConsoleMode( hOut, &dwMode );
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode( hOut, dwMode );
  SetConsoleTitleA( MINER_VERSION.data() );
  SetConsoleCtrlHandler( SignalHandler, TRUE );
}

auto UseOldUI() -> bool
{
  static bool t_old_ui = []() -> bool
  {
    OSVERSIONINFO winVer;
    ZeroMemory( &winVer, sizeof( OSVERSIONINFO ) );
    winVer.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

    // Stop deprecating things you don't have a _full_ replacement for!
#pragma warning( push )
#pragma warning( disable: 4996 )
    GetVersionEx( &winVer );
#pragma warning( pop )

    if( winVer.dwMajorVersion < 10 || winVer.dwBuildNumber < 14392 )
    {
      return true;
    }
    return false;
  }();

  return t_old_ui;
}

auto WINAPI SignalHandler( DWORD dwSig ) -> BOOL
{
  if( dwSig == CTRL_C_EVENT || dwSig == CTRL_BREAK_EVENT || dwSig == CTRL_CLOSE_EVENT )
  {
    HybridMiner::stop();
    return TRUE;
  }
  return FALSE;
}
