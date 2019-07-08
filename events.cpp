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

#include "events.h"
#include "types.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <algorithm>

namespace
{
  static std::condition_variable cv;
  static std::mutex mut;
  static std::deque<Nabiki::Events::queue_message_t> eventQueue;

  struct queue_checker
  {
    queue_checker( size_t handle ) :
      Q( handle )
    {}
    auto operator()( Nabiki::Events::queue_message_t& in_queue ) -> bool
    {
      return in_queue.queue_handle == Q;
    }
    size_t Q;
  };
}

namespace Nabiki::Events
{
  auto SendEvent( int32_t const& in_message, size_t const& in_queue, uint64_t const& in_param ) -> void
  {
    {
      guard lock( mut );
      eventQueue.emplace_back( queue_message_t{ in_queue, in_message, in_param } );
    }
    cv.notify_one();
  }

  auto GetEvent( queue_message_t& out_message, size_t const& in_queue ) -> int32_t
  {
    auto checker = queue_checker( in_queue );
    {
      cond_lock lock( mut );
      cv.wait( lock, [&] {
        return !eventQueue.empty() && std::any_of( eventQueue.begin(), eventQueue.end(), checker );
        } );
      auto message = std::find_if( eventQueue.begin(), eventQueue.end(), checker );
      std::memcpy( &out_message, &*message, sizeof out_message );
      eventQueue.erase( message );
    }
    return out_message.message;
  }
}
