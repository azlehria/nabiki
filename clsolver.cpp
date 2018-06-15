#include <cmath>
#include "clsolver.h"
#include "clsolver.cl"

// don't put this in the header . . .
#include "miner_state.h"

using namespace std::literals::string_literals;
using namespace std::chrono;

CLSolver::CLSolver( cl::Device const &device, double const intensity ) noexcept :
m_stop( false ),
m_new_target( true ),
m_new_message( true ),
m_hash_count( 0u ),
m_hash_count_samples( 0u ),
m_hash_average( 0 ),
m_intensity( intensity <= 41.99 ? intensity : 41.99 ),
m_threads( static_cast<uint64_t>(std::pow( 2, intensity <= 41.99 ? intensity : 41.99 )) ),
m_device_failure_count( 0u ),
m_gpu_initialized( false ),
m_device( device ),
h_solution_count( 0u ),
h_solutions{ 0u },
m_global_work_size{ 1u },
m_local_work_size{ 1u },
m_start( steady_clock::now() )
{
  m_run_thread = std::thread( &CLSolver::findSolution, this );
}

CLSolver::~CLSolver()
{
  if( m_run_thread.joinable() )
    m_run_thread.join();
}

auto CLSolver::getHashrate() const -> double const
{
  return m_hash_average;
}

// OpenCL device temp - how?
auto CLSolver::getTemperature() const -> uint32_t const
{
  return 0;
}

auto CLSolver::getDeviceState() const -> device_info_t const
{
  return { ""s, 0, 0, 0, 0, 0, 0 };
}

auto CLSolver::clInit() -> void
{
  if( m_gpu_initialized ) return;

  cl_int error;

  m_context = cl::Context( m_device, NULL, NULL, NULL, &error );
  m_queue = cl::CommandQueue( m_context, 0, &error );

  m_platform = m_device.getInfo<CL_DEVICE_PLATFORM>();
  std::string platform_name{ m_platform.getInfo<CL_PLATFORM_NAME>() };
  d_solution_count = cl::Buffer( m_context, (cl_mem_flags)CL_MEM_READ_WRITE|CL_MEM_ALLOC_HOST_PTR, 256 * 8, NULL, &error );
  d_solutions = cl::Buffer( m_context, (cl_mem_flags)CL_MEM_READ_WRITE|CL_MEM_ALLOC_HOST_PTR, 8, NULL, &error );
  d_mid = cl::Buffer( m_context, (cl_mem_flags)CL_MEM_READ_ONLY|CL_MEM_ALLOC_HOST_PTR, 25 * 8, NULL, &error );
  d_target = cl::Buffer( m_context, (cl_mem_flags)CL_MEM_READ_ONLY|CL_MEM_ALLOC_HOST_PTR, 8, NULL, &error );
  d_threads = cl::Buffer( m_context, (cl_mem_flags)CL_MEM_READ_ONLY|CL_MEM_ALLOC_HOST_PTR, 8, NULL, &error );

  cl::Program::Sources sources;

  sources.push_back( { clKeccakSource.c_str(), clKeccakSource.length() } );

  m_program = cl::Program( m_context, sources, &error );

  cl_uint compute_version{ 0 };
  std::string options{};
  if( platform_name.find( "NVIDIA" ) != std::string::npos )
  {
    compute_version = m_device.getInfo<CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV>( &error ) * 100;
    compute_version += ( m_device.getInfo<CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV>() * 10 );

    //m_local_work_size = compute_version > 500u ? 1024u : 384u;

    options = "-DCUDA_VERSION="s + std::to_string( compute_version );
  }

  if( m_program.build( { m_device }, options.c_str() ) != CL_SUCCESS )
  {
    std::cerr << "Error building OpenCL kernel: "s + m_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>( m_device ) + "\n"s;
    std::exit( EXIT_FAILURE );
  }

  m_kernel = cl::Kernel( m_program, "cl_mine", &error );

  //if( compute_version == 0u )
  //{
    m_local_work_size = m_kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>( m_device, &error );
  //}
  m_global_work_size = m_threads;
  MinerState::pushLog( std::to_string( *m_local_work_size ) );
  clResetSolution();

  m_gpu_initialized = true;
}

auto CLSolver::clCleanup() -> void
{
  //cudaSafeCall( cudaSetDevice( m_device ) );

  //cudaSafeCall( cudaThreadSynchronize() );

  //cudaSafeCall( cudaFree( d_solution ) );
  //cudaSafeCall( cudaFreeHost( h_solution ) );

  //cudaSafeCall( cudaDeviceReset() );

  m_gpu_initialized = false;
}

auto CLSolver::clResetSolution() -> void
{
  h_solution_count = 0u;
  cl_int error = m_queue.enqueueWriteBuffer( d_solution_count, CL_TRUE, 0, 8, &h_solution_count );
  if( error != CL_SUCCESS )
  {
    MinerState::pushLog( "OpenCL error #"s + std::to_string( error ) + " in clResetSolution"s );
    std::exit( EXIT_FAILURE );
  }
}

auto CLSolver::pushMessage() -> void
{
  state_t message{ MinerState::getMidstate() };
  cl_int error = m_queue.enqueueWriteBuffer( d_mid, CL_TRUE, 0, 200, message.data() );
  if( error != CL_SUCCESS )
  {
    MinerState::pushLog( "OpenCL error #"s + std::to_string( error ) + " in pushMessage"s );
    std::exit( EXIT_FAILURE );
  }
  m_new_message = false;
}

auto CLSolver::pushTarget() -> void
{
  uint64_t target{ MinerState::getTargetNum() };
  cl_int error = m_queue.enqueueWriteBuffer( d_target, CL_TRUE, 0, 8, &target );
  if( error != CL_SUCCESS )
  {
    MinerState::pushLog( "OpenCL error #"s + std::to_string( error ) + " in pushTarget"s );
    std::exit( EXIT_FAILURE );
  }
  m_new_target = false;
}

auto CLSolver::findSolution() -> void
{
  clInit();

  while( !m_stop )
  {
    if( m_new_target ) { pushTarget(); }
    if( m_new_message ) { pushMessage(); }

    m_kernel.setArg( 0, d_mid );
    m_kernel.setArg( 1, d_target );
    m_kernel.setArg( 2, d_solutions );
    m_kernel.setArg( 3, d_solution_count );
    cl_ulong temp = getNextSearchSpace();
    m_kernel.setArg( 4, temp );

    m_queue.enqueueNDRangeKernel( m_kernel, 1, m_global_work_size, m_local_work_size );

    //cudaError_t cudaerr = cudaDeviceSynchronize();
    //if( cudaerr != cudaSuccess )
    //{
    //  fprintf( stderr,
    //           "Kernel launch failed with error %d: \x1b[38;5;196m%s.\x1b[0m\n",
    //           cudaerr,
    //           cudaGetErrorString( cudaerr ) );

    //  ++m_device_failure_count;

    //  if( m_device_failure_count >= 3 )
    //  {
    //    fprintf( stderr,
    //             "Kernel launch has failed %u times. Try reducing your overclocks.\n",
    //             m_device_failure_count );
    //    exit( EXIT_FAILURE );
    //  }

    //  --m_intensity;
    //  m_threads = static_cast<uint64_t>( std::pow( 2, m_intensity ) );
    //  printf( "Reducing intensity to %.2f and restarting.\n",
    //    ( m_intensity ) );
    //  clCleanup();
    //  clInit();
    //  m_new_target = true;
    //  m_new_message = true;
    //  continue;
    //}

    memcpy( h_solutions, m_queue.enqueueMapBuffer( d_solutions, CL_FALSE, CL_MAP_READ, 0, 256 * 8 ), 256 * 8 );
    h_solution_count = *static_cast<uint64_t*>( m_queue.enqueueMapBuffer( d_solution_count, CL_FALSE, CL_MAP_READ, 0, 8 ) );

    if( h_solution_count > 0u )
    {
      pushSolution();
      clResetSolution();
    }
  }

  clCleanup();
}

auto CLSolver::updateTarget() -> void
{
  m_new_target = true;
}

auto CLSolver::updateMessage() -> void
{
  m_new_message = true;
}

auto CLSolver::stopFinding() -> void
{
  m_stop = true;
}

auto CLSolver::getNextSearchSpace() -> uint64_t const
{
  m_hash_count += m_threads;
  double t{ static_cast<double>(duration_cast<milliseconds>(steady_clock::now() - m_start).count()) / 1000 };

  if( m_hash_count_samples < 100 )
  {
    ++m_hash_count_samples;
  }

  double temp_average{ m_hash_average };
  temp_average += ((m_hash_count / t) / 1000000 - temp_average) / m_hash_count_samples;
  if( std::isnan( temp_average ) || std::isinf( temp_average ) )
  {
    temp_average = m_hash_average;
  }
  m_hash_average = temp_average;
  return MinerState::getIncSearchSpace( m_threads );
}

auto CLSolver::pushSolution() const -> void
{
  for( uint_fast32_t i{ 0 }; i < h_solution_count; ++i )
    MinerState::pushSolution( h_solutions[i] );
}
