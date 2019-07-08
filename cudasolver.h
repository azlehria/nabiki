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

#if !defined _CUDASOLVER_H_
#define _CUDASOLVER_H_

#include "types.h"
#include "isolver.h"
#include "devicetelemetry.h"
#include "DynamicLibs/dlcuda.h"

#include <cstdint>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

class CUDASolver : public ISolver
{
public:
  CUDASolver( int32_t const& device, double const& intensity );
  ~CUDASolver();

  auto findSolution() -> void final;

  auto startFinding() -> void final;
  auto stopFinding() -> void final
  { m_stop = true; }

  auto getName() const -> std::string const& final
  { return m_telemetry_handle->getName(); }
  auto getHashrate() -> double const final
  {
    guard lock{ m_hashrate_mutex };
    return static_cast<double>(m_hash_count) / (static_cast<double>(m_working_time) / 1e3);
  }

  auto getClockMem() const -> uint32_t const final
  { return m_telemetry_handle->getClockMem(); }
  auto getClockCore() const -> uint32_t const final
  { return m_telemetry_handle->getClockCore(); }
  auto getPowerWatts() const -> uint32_t const final
  { return m_telemetry_handle->getPowerWatts(); }
  auto getFanSpeed() const -> uint32_t const final
  { return m_telemetry_handle->getFanSpeed(); }
  auto getTemperature() const -> uint32_t const final
  { return m_telemetry_handle->getTemperature(); }
  auto getIntensity() const -> double const final
  { return m_intensity; }

  auto updateTarget() -> void final
  { m_new_target = true; }
  auto updateMessage() -> void final
  { m_new_message = true; }

private:
  CUDASolver() = delete;
  CUDASolver( CUDASolver const& ) = delete;
  CUDASolver& operator=( CUDASolver const& ) = delete;

  auto cudaResetSolution() -> void;

  auto updateHashrate() -> void;

  std::unique_ptr<IDeviceTelemetry> m_telemetry_handle;

  bool m_stop;
  bool m_new_target;
  bool m_new_message;
  bool m_device_initialized;

  uint64_t m_hash_count;
  uint64_t m_working_time;
  std::mutex m_hashrate_mutex;

  CUdevice m_device;
  CUcontext m_context;
  CUstream m_stream;
  CUmodule m_module;
  CUfunction m_kernel;

  uint64_t h_solution_count;
  uint64_t h_solutions[256];

  double m_intensity;
  uint64_t m_threads;

  std::thread m_run_thread;

  std::chrono::steady_clock::time_point m_round_start;
  std::chrono::steady_clock::time_point m_round_end;

  uint64_t h_threads;

  uint32_t m_grid;
  uint32_t m_block;

  CUdeviceptr d_mid;
  CUdeviceptr d_target;
  CUdeviceptr d_threads;

  CUdeviceptr d_solution_count;
  CUdeviceptr d_solutions;

  Cuda cu;

#if defined NDEBUG
#  define cuSafeCall( err ) err
#else // NDEBUG
#  define cuSafeCall( err ) cuSafeCall__( ( err ), __FILE__, __LINE__, m_device )

  auto cuSafeCall__( CUresult const& err, char const* file, int32_t const& line, int32_t const& device_id ) -> void
  {
    if( !err ) { return; }

    char const* err_str;
    cu.GetErrorString( err, &err_str );

    if( !err_str )
    {
      std::cerr << "CUDA device " << device_id
                << " encountered error #" << err << " in file '" << file
                << "' on line " << line
                << ". Unable to retrieve error description.\n";
    }
    std::cerr << "CUDA device " << device_id
              << " encountered error #" << err << " in file '" << file
              << "' on line " << line
              << " : " << err_str << ".\n";
    throw;
  }
#endif // NDEBUG

};

#endif // !_CUDASOLVER_H_
