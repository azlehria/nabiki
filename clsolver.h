#ifndef _CLSOLVER_H_
#define _CLSOLVER_H_

#include "isolver.h"
#include "types.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <string>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wignored-attributes"
#endif // __GNUC__
#ifdef __APPLE__
#  include <OpenCL/cl.hpp>
#else
#  include <CL/cl.hpp>
#endif
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

class CLSolver : public ISolver
{
public:
  CLSolver() = delete;
  CLSolver( cl::Device const &device, double const intensity ) noexcept;
  ~CLSolver();

  auto findSolution() -> void final;
  auto stopFinding() -> void final;

  auto getHashrate() const -> double const final;
  auto getTemperature() const -> uint32_t const final;
  auto getDeviceState() const -> device_info_t const final;

  auto updateTarget() -> void final;
  auto updateMessage() -> void final;

private:
  auto pushTarget() -> void;
  auto pushMessage() -> void;

  auto clInit() -> void;
  auto clCleanup() -> void;

  auto clResetSolution() -> void;

  auto getNextSearchSpace() -> uint64_t const;
  auto pushSolution() const -> void;

  std::thread m_run_thread;

  std::atomic<bool> m_stop;
  std::atomic<bool> m_stopped;
  std::atomic<bool> m_new_target;
  std::atomic<bool> m_new_message;

  std::atomic<uint64_t> m_hash_count;
  std::atomic<uint64_t> m_hash_count_samples;
  std::atomic<double> m_hash_average;

  double m_intensity;
  uint64_t m_threads;

  uint_fast8_t m_device_failure_count;
  bool m_gpu_initialized;

  cl::Platform m_platform;
  cl::Device m_device;
  cl::Context m_context;
  cl::Program m_program;
  cl::CommandQueue m_queue;
  cl::Kernel m_kernel;

  cl::Buffer d_solution_count;
  uint64_t h_solution_count;
  cl::Buffer d_solutions;
  uint64_t h_solutions[256];
  cl::Buffer d_mid;
  cl::Buffer d_target;
  cl::Buffer d_threads;

  cl::NDRange m_global_work_size;
  cl::NDRange m_local_work_size;

  std::chrono::steady_clock::time_point m_start;
};

#endif // !_CLSOLVER_H_
