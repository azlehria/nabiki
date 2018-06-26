#include <cstring>
#include <cmath>
#include <chrono>
#include "cpusolver.h"

using namespace std::chrono;

// --------------------------------------------------------------------

auto bswap64( uint64_t const& in ) -> uint64_t
{
  uint64_t out;
  uint8_t* temp = reinterpret_cast<uint8_t*>(&out);
  uint8_t const* t_in = reinterpret_cast<uint8_t const*>(&in);

  temp[0] = t_in[7];
  temp[1] = t_in[6];
  temp[2] = t_in[5];
  temp[3] = t_in[4];
  temp[4] = t_in[3];
  temp[5] = t_in[2];
  temp[6] = t_in[1];
  temp[7] = t_in[0];

  return out;
}

CPUSolver::CPUSolver() noexcept :
m_new_target( true ),
m_new_message( true ),
m_hash_count( 0u ),
m_hash_count_samples( 0u ),
m_hash_average( 0 ),
m_stop( false ),
m_start( steady_clock::now() )
{
  sph_keccak256_init( &m_ctx );
  m_run_thread = std::thread( &CPUSolver::findSolution, this );
}

CPUSolver::~CPUSolver()
{
  if( m_run_thread.joinable() )
    m_run_thread.join();
}

auto CPUSolver::stopFinding() -> void
{
  m_stop.store( true, std::memory_order_release );
}

auto CPUSolver::findSolution() -> void
{
  uint64_t solution;
  message_t in_buffer;
  hash_t out_buffer;

  while( !m_stop.load( std::memory_order_acquire ) )
  {
    if( m_new_target.load( std::memory_order_acquire ) )
    {
      m_target.store( MinerState::getTargetNum(), std::memory_order_release );
      m_new_target.store( false, std::memory_order_release );
    }
    if( m_new_message.load( std::memory_order_acquire ) )
    {
      m_message = MinerState::getMessage();
      m_new_message.store( false, std::memory_order_release );
    }

    solution = getNextSearchSpace();
    std::memcpy( in_buffer.data(), m_message.data(), 84 );
    std::memcpy( &in_buffer[64], &solution, 8 );

    sph_keccak256( &m_ctx, in_buffer.data(), in_buffer.size() );
    sph_keccak256_close( &m_ctx, out_buffer.data() );

    if( bswap64( reinterpret_cast<uint64_t&>(out_buffer[0]) ) < MinerState::getTargetNum() )
    {
      MinerState::pushSolution( solution );
    }
  }
}

auto CPUSolver::getHashrate() const -> double const
{
  return m_hash_average.load( std::memory_order_acquire );
}

// XP CPU temp - how?
auto CPUSolver::getTemperature() const -> uint32_t const
{
  return 0;
}

auto CPUSolver::getDeviceState() const -> device_info_t const
{
  return { ""s, 0, 0, 0, 0, 0, 0 };
}

auto CPUSolver::updateTarget() -> void
{
  m_new_target.store( true, std::memory_order_release );
}
auto CPUSolver::updateMessage() -> void
{
  m_new_message.store( true, std::memory_order_release );
}

auto CPUSolver::getNextSearchSpace() -> uint64_t const
{
  m_hash_count.fetch_add( 1, std::memory_order_acq_rel );
  double t{ static_cast<double>(duration_cast<milliseconds>(steady_clock::now() - m_start).count()) / 1000 };

  if( m_hash_count_samples.load( std::memory_order_acquire ) < 100 )
  {
    m_hash_count_samples.fetch_add( 1, std::memory_order_release );
  }

  double temp_average{ m_hash_average.load( std::memory_order_acquire ) };
  temp_average += ((m_hash_count.load( std::memory_order_acquire ) / t) / 1000000 - temp_average) / m_hash_count_samples.load( std::memory_order_acquire );
  if( !std::isnan( temp_average ) && !std::isinf( temp_average ) )
  {
    m_hash_average.store( temp_average, std::memory_order_release );
  }
  return MinerState::getIncSearchSpace( 1 );
}
