#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include <cstdint>

#include "CivetWeb/civetweb.h"

namespace Telemetry
{
  auto Init() -> void;
  auto Cleanup() -> void;
};

#endif // !_TELEMETRY_H_
