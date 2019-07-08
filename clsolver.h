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

#if !defined _CLSOLVER_H_
#define _CLSOLVER_H_

#include "isolver.h"
#include "types.h"
#include "devicetelemetry.h"
#include "DynamicLibs/dlopencl.h"

#include <cstdint>
#include <cmath>
#include <memory>
#include <string>
#include <atomic>
#include <thread>

class CLSolver : public ISolver
{
public:
  CLSolver( cl_device_id const& device, double const& intensity ) noexcept;
  ~CLSolver();

  auto findSolution() -> void final;

  auto startFinding() -> void final;
  auto inline stopFinding() -> void final
  { m_stop = true; }

  auto inline getName() const -> std::string const& final
  { return m_telemetry_handle->getName(); }
  auto inline getHashrate() -> double const final
  { return m_hash_average.load( std::memory_order_acquire ) / 1000.; }

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
  CLSolver() = delete;
  CLSolver( CLSolver const& ) = delete;
  CLSolver& operator=( CLSolver const& ) = delete;

  auto inline updateHashrate() -> void
  {
    // store duration, incremented with (m_round_end - m_round_start)
    // store total hashes
    // calculate average when queried?
    using namespace std::chrono;

    // goofy, yes; but it results in timing the _entire loop_
    m_round_end = steady_clock::now() - m_start;
    temp_time = static_cast<uint64_t>((m_round_end - m_round_start).count() / 1000000);
    m_round_start = steady_clock::now() - m_start;

    if( !temp_time ) { return; } // less than 1 nanosecond

    // gimmick the first round because the first kernel run has extra, strange overhead
    if( !m_first_round_passed )
    {
      m_hash_average.store( (m_threads / temp_time) / 15, std::memory_order_release );
      m_first_round_passed = true;
      return;
    }

    temp_average = m_hash_average.load( std::memory_order_acquire ) * 19 / 20;

    // fairly accurate, but needs work to smooth without requiring a delay
    temp_average += (m_threads / temp_time) / 20;
    m_hash_average.store( temp_average, std::memory_order_release );
  }

  std::unique_ptr<IDeviceTelemetry> m_telemetry_handle;

  bool m_stop;
  bool m_new_target;
  bool m_new_message;
  bool m_device_initialized;

  uint32_t h_solution_count;
  uint64_t h_solutions[256];

  uint64_t temp_time;
  uint64_t temp_average;
  uint64_t m_hash_count;
  bool m_first_round_passed;
  std::atomic<uint64_t> m_hash_average;

  double m_intensity;
  uint64_t m_threads;

  std::thread m_run_thread;

  std::chrono::steady_clock::time_point m_start;
  std::chrono::steady_clock::duration m_round_start;
  std::chrono::steady_clock::duration m_round_end;

  cl_platform_id m_platform;
  cl_device_id m_device;
  cl_context m_context;
  cl_program m_program;
  cl_command_queue m_queue;
  cl_kernel m_kernel;

  cl_mem d_solution_count;
  cl_mem d_solutions;
  cl_mem d_mid;
  uint64_t h_threads;

  size_t m_global_work_size;
  size_t m_local_work_size;

  Opencl cl;
};

#endif // !_CLSOLVER_H_
