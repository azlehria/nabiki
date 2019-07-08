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

#include "cpusolver.h"
#include "devicetelemetry.h"

#include <cstring>
#include <chrono>

using namespace std::chrono;

// --------------------------------------------------------------------

namespace
{
  static auto bswap64( uint64_t const& in )
  {
    uint64_t out{ 0 };
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
}

CPUSolver::CPUSolver( double const& intensity ) noexcept :
  m_telemetry_handle( Nabiki::MakeDeviceTelemetryObject( nullptr ) ),
  m_stop( false ),
  m_new_target( true ),
  m_new_message( true ),
  m_device_initialized( false ),
  temp_time( 0 ),
  temp_average( 0 ),
  m_hash_count( 0 ),
  m_hash_count_samples( 0 ),
  m_hash_average( 0 ),
  m_target( 0 ),
  m_message{},
  h_solution_count( 0 ),
  h_solutions{},
  m_intensity( intensity <= 41.99 ? intensity : 41.99 ),
  m_threads( intensity == 1 ? 1 : static_cast<uint64_t>(std::pow( 2, m_intensity )) )
{
  sph_keccak256_init( &m_ctx );
}

CPUSolver::~CPUSolver()
{
  if( m_run_thread.joinable() )
    m_run_thread.join();
}

auto CPUSolver::findSolution() -> void
{
  uint64_t solution;
  message_t in_buffer;
  hash_t out_buffer;

  do
  {
    if( m_new_target )
    {
      m_target = MinerState::getTargetNum();
      m_new_target = false;
    }
    if( m_new_message )
    {
      m_message = MinerState::getMessage();
      m_new_message = false;
    }

    solution = MinerState::getIncSearchSpace( 1 );
    std::memcpy( in_buffer.data(), m_message.data(), 84 );
    std::memcpy( &in_buffer[64], &solution, 8 );

    sph_keccak256( &m_ctx, in_buffer.data(), in_buffer.size() );
    sph_keccak256_close( &m_ctx, out_buffer.data() );

    updateHashrate();

    if( bswap64( reinterpret_cast<uint64_t&>(out_buffer[0]) ) < m_target )
    {
      MinerState::pushSolution( solution );
    }
  }
  while( !m_stop );
}

auto CPUSolver::startFinding() -> void
{
  m_run_thread = std::thread( &CPUSolver::findSolution, this );
}
