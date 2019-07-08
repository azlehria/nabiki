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

#if !defined _CPUSOLVER_H_
#define _CPUSOLVER_H_

#include "miner_state.h"
#include "types.h"
#include "sph_keccak.h"
#include "isolver.h"
#include "devicetelemetry.h"

#include <cstdint>
#include <cmath>
#include <memory>
#include <string>
#include <atomic>
#include <thread>

class CPUSolver : public ISolver
{
public:
  CPUSolver() noexcept :
    CPUSolver( 1 )
  {}
  CPUSolver( double const& intensity ) noexcept;
  ~CPUSolver();

  auto findSolution() -> void final;

  auto startFinding() -> void final;
  auto inline stopFinding() -> void final
  { m_stop = true; }

  auto inline getName() const -> std::string const& final
  { return m_telemetry_handle->getName(); }
  auto inline getHashrate() -> double const final
  { return m_hash_average.load( std::memory_order_acquire ) * 1000000.0; }

  auto inline getClockMem() const -> uint32_t const final
  { return m_telemetry_handle->getClockMem(); }
  auto inline getClockCore() const -> uint32_t const final
  { return m_telemetry_handle->getClockCore(); }
  auto inline getPowerWatts() const -> uint32_t const final
  { return m_telemetry_handle->getPowerWatts(); }
  auto inline getFanSpeed() const -> uint32_t const final
  { return m_telemetry_handle->getFanSpeed(); }
  auto inline getTemperature() const -> uint32_t const final
  { return m_telemetry_handle->getTemperature(); }
  auto inline getIntensity() const -> double const final
  { return m_intensity; }

  auto inline updateTarget() -> void final
  { m_new_target = true; }
  auto inline updateMessage() -> void final
  { m_new_message = true; }

private:
  //CPUSolver() = delete;
  CPUSolver( CPUSolver const& ) = delete;
  CPUSolver& operator=( CPUSolver const& ) = delete;

  auto inline updateHashrate() -> void
  {
    using namespace std::chrono;

    if( m_hash_count_samples < 100 )
    {
      ++m_hash_count_samples;
    }

    m_hash_count += m_threads;
    temp_time = static_cast<double>(duration_cast<nanoseconds>(steady_clock::now() - m_start).count()) / 1000000000;

    temp_average = m_hash_average.load( std::memory_order_acquire );
    temp_average += ((m_hash_count / temp_time) - temp_average) / m_hash_count_samples;
    if( !std::isnan( temp_average ) && !std::isinf( temp_average ) )
    {
      m_hash_average.store( temp_average, std::memory_order_release );
    }
  }

  std::unique_ptr<IDeviceTelemetry> m_telemetry_handle;

  bool m_stop;
  bool m_new_target;
  bool m_new_message;
  bool m_device_initialized;

  double temp_time;
  double temp_average;
  uint64_t m_hash_count;
  uint64_t m_hash_count_samples;
  std::atomic<double> m_hash_average;

  uint64_t m_target;
  message_t m_message;

  uint32_t h_solution_count;
  uint64_t h_solutions[256];

  double m_intensity;
  uint64_t m_threads;

  std::thread m_run_thread;

  std::chrono::steady_clock::time_point m_start;

  sph_keccak256_context m_ctx;
};

#endif // !_SOLVER_H_
