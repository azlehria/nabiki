#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include <cstdint>

#include "CivetWeb/civetweb.h"

namespace Telemetry
{
  auto init() -> void;
  auto cleanup() -> void;
};

#endif // !_TELEMETRY_H_
