#ifndef _TYPES_H_
#define _TYPES_H_

#include <cstdint>
#include <array>
#include <vector>
#include <string>

using namespace std::literals::string_literals;

// to hell with standards that take a useful, _common_ convention and
// reserve it for their own use (I'm looking at you, POSIX)
typedef std::array<uint8_t,  52u> prefix_t;
typedef std::array<uint8_t,  32u> hash_t;
typedef std::array<uint8_t,  84u> message_t;
typedef std::array<uint8_t, 200u> state_t;
typedef std::array<uint8_t,  20u> address_t;
typedef std::array<uint8_t,   8u> solution_t;

typedef std::vector<std::pair<int32_t, double>> device_list_t;
typedef std::vector<std::pair<std::string, device_list_t>> device_map_t;

typedef struct _device_info
{
  std::string name;
  uint64_t core;
  uint64_t memory;
  uint32_t power;
  uint32_t temperature;
  uint32_t fan;
  double hashrate;
} device_info_t;

#endif // !_TYPES_H_
