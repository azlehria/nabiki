#ifndef _COMMO_H_
#define _COMMO_H_

#include <cstdint>
#include <string>
#include <vector>

namespace Commo
{
  auto Init() -> void;
  auto Cleanup() -> void;

  auto GetPing() -> uint64_t const;
  auto GetTotalShares() -> uint64_t const;
  auto GetConnectionErrorCount() -> uint64_t const;
  auto GetConnectionErrorLog() -> std::vector<std::string> const;
}

#endif // !_COMMO_H_
