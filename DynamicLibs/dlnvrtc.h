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

#if !defined(_DLNVRTC_H_)
#define _DLNVRTC_H_

#include "libhelper.hpp"
#include <cwchar>
#include <nvrtc.h>

class Nvrtc
{
private:
#if defined(_MSC_VER)
  static constexpr wchar_t libname[] = L"nvrtc.dll";
#else
  static constexpr char libname[] = "libnvrtc.so";
#endif

  DllHelper dll_{ libname };

public:
  decltype(nvrtcCreateProgram)* CreateProgram = dll_["nvrtcCreateProgram"];
  decltype(nvrtcCompileProgram)* CompileProgram = dll_["nvrtcCompileProgram"];
  decltype(nvrtcDestroyProgram)* DestroyProgram = dll_["nvrtcDestroyProgram"];
  decltype(nvrtcGetProgramLogSize)* GetProgramLogSize = dll_["nvrtcGetProgramLogSize"];
  decltype(nvrtcGetProgramLog)* GetProgramLog = dll_["nvrtcGetProgramLog"];
  decltype(nvrtcGetPTXSize)* GetPTXSize = dll_["nvrtcGetPTXSize"];
  decltype(nvrtcGetPTX)* GetPTX = dll_["nvrtcGetPTX"];
};

#endif // !defined(_DLNVRTC_H_)
