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

#if !defined(_DLCUDA_H_)
#define _DLCUDA_H_

#include "libhelper.hpp"
#include <cwchar>
#include <cuda.h>

#define DeclareLibAlias(x,y) decltype(x)* y = dll_[#x]
#define DeclareLibFunction(x) decltype(x)* x = dll_[#x]

class Cuda
{
private:
#if defined(_MSC_VER)
  static constexpr wchar_t libname[] = L"nvcuda.dll";
#else
  static constexpr char libname[] = "libcuda.so";
#endif

  DllHelper dll_{ libname };

public:
  DeclareLibAlias( cuInit, Init );
  //decltype(cuInit)* Init = dll_["cuInit"]();
  decltype(cuDeviceGet)* DeviceGet = dll_["cuDeviceGet"];
  decltype(cuDeviceGetAttribute)* DeviceGetAttribute = dll_["cuDeviceGetAttribute"];
  decltype(cuDeviceGetCount)* DeviceGetCount = dll_["cuDeviceGetCount"];
  decltype(cuCtxCreate_v2)* CtxCreate = dll_["cuCtxCreate_v2"];
  decltype(cuCtxDestroy_v2)* CtxDestroy = dll_["cuCtxDestroy_v2"];
  decltype(cuCtxSetCurrent)* CtxSetCurrent = dll_["cuCtxSetCurrent"];
  decltype(cuCtxPopCurrent_v2)* CtxPopCurrent = dll_["cuCtxPopCurrent_v2"];
  decltype(cuCtxPushCurrent_v2)* CtxPushCurrent = dll_["cuCtxPushCurrent_v2"];
  decltype(cuCtxSetLimit)* CtxSetLimit = dll_["cuCtxSetLimit"];
  decltype(cuCtxSynchronize)* CtxSynchronize = dll_["cuCtxSynchronize"];
  decltype(cuStreamCreate)* StreamCreate = dll_["cuStreamCreate"];
  decltype(cuStreamDestroy_v2)* StreamDestroy = dll_["cuStreamDestroy_v2"];
  decltype(cuStreamSynchronize)* StreamSynchronize = dll_["cuStreamSynchronize"];
  decltype(cuModuleLoadDataEx)* ModuleLoadDataEx = dll_["cuModuleLoadDataEx"];
  decltype(cuModuleLoadFatBinary)* ModuleLoadFatBinary = dll_["cuModuleLoadFatBinary"];
  decltype(cuModuleUnload)* ModuleUnload = dll_["cuModuleUnload"];
  decltype(cuModuleGetFunction)* ModuleGetFunction = dll_["cuModuleGetFunction"];
  decltype(cuModuleGetGlobal_v2)* ModuleGetGlobal = dll_["cuModuleGetGlobal_v2"];
  decltype(cuMemcpyHtoD_v2)* MemcpyHtoD = dll_["cuMemcpyHtoD_v2"];
  decltype(cuMemcpyDtoH_v2)* MemcpyDtoH = dll_["cuMemcpyDtoH_v2"];
  decltype(cuMemcpyHtoDAsync_v2)* MemcpyHtoDAsync = dll_["cuMemcpyHtoDAsync_v2"];
  decltype(cuMemcpyDtoHAsync_v2)* MemcpyDtoHAsync = dll_["cuMemcpyDtoHAsync_v2"];
  decltype(cuLaunchKernel)* LaunchKernel = dll_["cuLaunchKernel"];
  decltype(cuGetErrorString)* GetErrorString = dll_["cuGetErrorString"];
};

#endif // !defined(_DLCUDA_H_)
