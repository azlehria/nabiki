#ifndef _COMMO_H_
#define _COMMO_H_

#include <cstdint>

namespace Commo
{
  auto Init() -> void;
  auto Cleanup() -> void;
  auto GetPing() -> uint64_t const;
}

#endif // !_COMMO_H_
