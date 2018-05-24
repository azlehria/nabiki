#ifndef _ISOLVER_H_
#define _ISOLVER_H_

#include <cstdint>
#include "types.h"

class ISolver
{
public:
  virtual ~ISolver() {};

  auto virtual findSolution() -> void = 0;
  auto virtual stopFinding() -> void = 0;

  auto virtual getHashrate() const -> double const = 0;
  auto virtual getTemperature() const -> uint32_t const = 0;
  auto virtual getDeviceState() const -> device_info_t const = 0;

  auto virtual updateTarget() -> void = 0;
  auto virtual updateMessage() -> void = 0;
};

#endif // !_ISOLVER_H_
