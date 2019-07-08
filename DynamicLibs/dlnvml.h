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

#if !defined(_DLNVML_H_)
#define _DLNVML_H_

#include "libhelper.hpp"
#include <cwchar>
#include <nvml.h>

class Nvml
{
private:
#if defined(_MSC_VER)
  static constexpr wchar_t libname[] = L"nvml.dll";
#else
  static constexpr char libname[] = "libnvidia-ml.so";
#endif

  DllHelper dll_{ libname };

public:
  decltype(nvmlInit_v2)* Init = dll_["nvmlInit_v2"];
  decltype(nvmlShutdown)* Shutdown = dll_["nvmlShutdown"];
  decltype(nvmlDeviceGetHandleByPciBusId_v2)* DeviceGetHandleByPciBusId = dll_["nvmlDeviceGetHandleByPciBusId_v2"];
  decltype(nvmlDeviceGetName)* DeviceGetName = dll_["nvmlDeviceGetName"];
  decltype(nvmlDeviceGetClockInfo)* DeviceGetClockInfo = dll_["nvmlDeviceGetClockInfo"];
  decltype(nvmlDeviceGetPowerUsage)* DeviceGetPowerUsage = dll_["nvmlDeviceGetPowerUsage"];
  decltype(nvmlDeviceGetFanSpeed)* DeviceGetFanSpeed = dll_["nvmlDeviceGetFanSpeed"];
  decltype(nvmlDeviceGetTemperature)* DeviceGetTemperature = dll_["nvmlDeviceGetTemperature"];
  decltype(nvmlDeviceGetHandleByUUID)* DeviceGetHandleByUUID = dll_["nvmlDeviceGetHandleByUUID"];
};

#endif // !defined(_DLNVML_H_)
