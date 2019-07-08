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

#if !defined(_DLOPENCL_H_)
#define _DLOPENCL_H_

#include "libhelper.hpp"
#include <cwchar>

#if defined __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wignored-attributes"
#endif // __GNUC__
#if defined __APPLE__
#  include <OpenCL/cl.h>
#  include <OpenCL/cl_ext.h>
#else
#  include <CL/cl.h>
#  include <CL/cl_ext.h>
#endif
#if defined __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

class Opencl
{
private:
#if defined(_MSC_VER)
  static constexpr wchar_t libname[] = L"OpenCL.dll";
#else
  static constexpr char libname[] = "libOpenCL.so";
#endif

  DllHelper dll_{ libname };

public:
  decltype(clGetDeviceInfo)* GetDeviceInfo = dll_["clGetDeviceInfo"];
  decltype(clGetDeviceIDs)* GetDeviceIDs = dll_["clGetDeviceIDs"];
  decltype(clGetPlatformInfo)* GetPlatformInfo = dll_["clGetPlatformInfo"];
  decltype(clGetPlatformIDs)* GetPlatformIDs = dll_["clGetPlatformIDs"];
  decltype(clCreateContext)* CreateContext = dll_["clCreateContext"];
  decltype(clCreateCommandQueue)* CreateCommandQueue = dll_["clCreateCommandQueue"];
  decltype(clCreateBuffer)* CreateBuffer = dll_["clCreateBuffer"];
  decltype(clCreateProgramWithSource)* CreateProgramWithSource = dll_["clCreateProgramWithSource"];
  decltype(clBuildProgram)* BuildProgram = dll_["clBuildProgram"];
  decltype(clGetProgramBuildInfo)* GetProgramBuildInfo = dll_["clGetProgramBuildInfo"];
  decltype(clCreateKernel)* CreateKernel = dll_["clCreateKernel"];
  decltype(clSetKernelArg)* SetKernelArg = dll_["clSetKernelArg"];
  decltype(clGetKernelWorkGroupInfo)* GetKernelWorkGroupInfo = dll_["clGetKernelWorkGroupInfo"];
  decltype(clEnqueueWriteBuffer)* EnqueueWriteBuffer = dll_["clEnqueueWriteBuffer"];
  decltype(clEnqueueReadBuffer)* EnqueueReadBuffer = dll_["clEnqueueReadBuffer"];
  decltype(clEnqueueNDRangeKernel)* EnqueueNDRangeKernel = dll_["clEnqueueNDRangeKernel"];
  decltype(clFlush)* Flush = dll_["clFlush"];
};

#endif // !defined(_DLOPENCL_H_)
