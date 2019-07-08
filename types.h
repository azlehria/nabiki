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

#if !defined _TYPES_H_
#define _TYPES_H_

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

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

using guard     = std::lock_guard<std::mutex>;
using cond_lock = std::unique_lock<std::mutex>;

struct device_info_t
{
  std::string name;
  double hashrate;
  uint32_t core;
  uint32_t memory;
  uint32_t power;
  uint32_t temperature;
  uint32_t fan;
};

enum device_type_t
{
  DEVICE_CUDA,
  DEVICE_OPENCL,
  DEVICE_CPU
};

#endif // !_TYPES_H_
