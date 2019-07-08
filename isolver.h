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

#if !defined _ISOLVER_H_
#define _ISOLVER_H_

#include <cstdint>
#include <string>

class ISolver
{
public:
  virtual ~ISolver() {}

  auto virtual startFinding() -> void = 0;
  auto virtual stopFinding() -> void = 0;

  auto virtual getName() const -> std::string const& = 0;
  auto virtual getHashrate() -> double const = 0;

  auto virtual getClockMem() const -> uint32_t const = 0;
  auto virtual getClockCore() const -> uint32_t const = 0;
  auto virtual getPowerWatts() const -> uint32_t const = 0;
  auto virtual getFanSpeed() const -> uint32_t const = 0;
  auto virtual getTemperature() const -> uint32_t const = 0;
  auto virtual getIntensity() const -> double const = 0;

  auto virtual updateTarget() -> void = 0;
  auto virtual updateMessage() -> void = 0;
  auto virtual findSolution() -> void = 0;
};

#endif // !_ISOLVER_H_
