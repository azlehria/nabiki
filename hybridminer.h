/*
  Header file to declare prototypes

*/

#ifndef  _HYBRIDMINER_H_
#define  _HYBRIDMINER_H_

#include "types.h"
#include <cstdint>
#include <vector>
#include <string_view>

using namespace std::string_view_literals;

std::string_view constexpr MINER_VERSION{ "Nabiki v0.2.0"sv };

namespace HybridMiner
{
  auto updateTarget() -> void;
  auto updateMessage() -> void;

  auto run() -> void;
  auto stop() -> void;

  auto getHashrates() -> std::vector<double> const;
  auto getTemperatures() -> std::vector<uint32_t> const;
  auto getDeviceStates() -> std::vector<device_info_t> const;

  auto getUptime() -> uint64_t const;
}

#endif // ! _CPUMINER_H_
