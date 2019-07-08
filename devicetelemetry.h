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

#if !defined _DEVICETELEMETRY_H_
#define _DEVICETELEMETRY_H_

#include "types.h"

#include <string>
#include <cstdint>
#include <memory>

class IDeviceTelemetry
{
public:
  virtual ~IDeviceTelemetry() = 0;

  auto virtual getName() -> std::string const& = 0;
  auto virtual getClockMem() -> uint32_t const = 0;
  auto virtual getClockCore() -> uint32_t const = 0;
  auto virtual getPowerWatts() -> uint32_t const = 0;
  auto virtual getFanSpeed() -> uint32_t const = 0;
  auto virtual getTemperature() -> uint32_t const = 0;
  auto virtual getDeviceType() -> device_type_t const = 0;
};

namespace Nabiki
{
  template<typename T>
  auto MakeDeviceTelemetryObject( T const& device ) -> std::unique_ptr<IDeviceTelemetry>;
}

#endif // _DEVICETELEMETRY_H_
