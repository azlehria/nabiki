#ifndef _PLATFORMS_H_
#define _PLATFORMS_H_

auto SetBasicState() -> void;
auto UseOldUI() -> bool;

#ifdef _MSC_VER
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>

auto WINAPI SignalHandler( DWORD dwSig ) -> BOOL;

#else // _MSC_VER

void SignalHandler( int signal );

#endif // _MSC_VER

#endif // !_PLATFORMS_H_
