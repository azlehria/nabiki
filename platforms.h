#ifndef _PLATFORMS_H_
#define _PLATFORMS_H_

auto SetBasicState() -> void;
auto UseOldUI() -> bool const;

#ifdef _MSC_VER
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#  include <intrin.h>

#  define rotl64 _rotl64

auto WINAPI SignalHandler( DWORD dwSig ) -> BOOL;

#else // _MSC_VER
#  include "x86intrin.h"

#  define rotl64 __rolq

auto SignalHandler( int signal ) -> void;

#endif // _MSC_VER

#endif // !_PLATFORMS_H_
