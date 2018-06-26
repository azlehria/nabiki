#ifndef _TYPES_H_
#define _TYPES_H_

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

using namespace std::literals::string_literals;

// to hell with standards that take a useful, _common_ convention and
// reserve it for their own use (I'm looking at you, POSIX)
using prefix_t   = std::array<uint8_t,  52u>;
using hash_t     = std::array<uint8_t,  32u>;
using message_t  = std::array<uint8_t,  84u>;
using state_t    = std::array<uint8_t, 200u>;
using address_t  = std::array<uint8_t,  20u>;
using solution_t = std::array<uint8_t,   8u>;

using device_list_t = std::vector<std::pair<int32_t, double>>;
using device_map_t  = std::vector<std::pair<std::string, device_list_t>>;

using guard = std::lock_guard<std::mutex>;

struct device_info_t
{
  std::string name;
  uint32_t core;
  uint32_t memory;
  uint32_t power;
  uint32_t temperature;
  uint32_t fan;
  double hashrate;
};

#endif // !_TYPES_H_
