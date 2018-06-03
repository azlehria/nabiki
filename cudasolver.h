#ifndef _CUDASOLVER_H_
#define _CUDASOLVER_H_

#include "types.h"
#include "isolver.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <vector_types.h>
#include <nvml.h>

class CUDASolver : public ISolver
{
public:
  CUDASolver() = delete;
  CUDASolver( int32_t const device, double const intensity ) noexcept;
  ~CUDASolver();

  auto findSolution() -> void final;
  auto stopFinding() -> void final;

  auto getHashrate() const -> double const final;
  auto getTemperature() const -> uint32_t const final;
  auto getDeviceState() const -> device_info_t const final;

  auto updateTarget() -> void final;
  auto updateMessage() -> void final;

private:
  CUDASolver( CUDASolver const& ) = delete;
  CUDASolver& operator=( CUDASolver const& ) = delete;

  static uint32_t constexpr TPB35{  384u };
  static uint32_t constexpr TPB50{ 1024u };

  auto pushTarget() -> void;
  auto pushMessage() -> void;

  auto cudaInit() -> void;
  auto cudaCleanup() -> void;

  auto cudaResetSolution() -> void;

  auto getNextSearchSpace() -> uint64_t const;
  auto getTarget() const -> uint64_t const;
  auto getMidstate() const -> state_t const;
  auto pushSolutions() const -> void;

  std::chrono::steady_clock::time_point m_start;

  std::thread m_run_thread;

  nvmlDevice_t m_nvml_handle;

  std::string m_name;

  bool m_stop;
  bool m_new_target;
  bool m_new_message;

  uint64_t m_hash_count;
  uint64_t m_hash_count_samples;
  std::atomic<double> m_hash_average;

  double m_intensity;
  uint64_t m_threads;

  bool m_gpu_initialized;
  int32_t m_device;
  uint32_t* h_solution_count;
  uint32_t* d_solution_count;
  uint64_t* h_solutions;
  uint64_t* d_solutions;

  dim3 m_grid;
  dim3 m_block;
};

#endif // !_CUDASOLVER_H_
