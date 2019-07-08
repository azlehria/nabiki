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


#if !defined  _MINERCORE_H_
#define  _MINERCORE_H_

#include "types.h"
#include "platforms.h"
#include "isolver.h"

#include <cstdint>
#include <vector>
#include <string_view>
#include <memory>

#define MAJOR_VER 0
#define MINOR_VER 3
#define PATCH_VER 0
#define VER_HELPER1(x) #x
#define VER_HELPER2(x,y,z) VER_HELPER1(Nabiki v##x##.##y##.##z)sv
#define VER_HELPER3(x,y,z) VER_HELPER2(x, y, z)
#define VER_HELPER VER_HELPER3(MAJOR_VER, MINOR_VER, PATCH_VER)
namespace MinerCore
{
  using namespace std::string_view_literals;
  std::string_view constexpr MINER_VERSION{ VER_HELPER };

  auto updateTarget() -> void;
  auto updateMessage() -> void;

  auto run() -> int32_t;
  auto stop() -> void;

  auto getHashrates() -> std::vector<double> const;
  auto getDevice( size_t devIndex ) -> ISolver*;
  auto getDeviceReferences() -> std::vector<std::shared_ptr<ISolver>> const;

  auto getActiveDeviceCount() -> uint_fast16_t const;

  auto getUptime() -> int64_t const;
}

#endif // !_MINERCORE_H_
