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

#if !defined _DEVICEPANEL_H_
#define _DEVICEPANEL_H

#include "types.h"
#include "isolver.h"

#define NOATOM
#define NOCLIPBOARD
#define NOKERNEL
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOSERVICE
#define NOSOUND
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NOMCX
#define _AMD64_
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cinttypes>
#include <memory>


class DevicePanel
{
public:
  enum DeviceType
  {
    DEVICE_CPU,
    DEVICE_CUDA,
    DEVICE_CL
  };

  DevicePanel( std::shared_ptr<ISolver> const );
  ~DevicePanel() = default;

  operator HWND() const { return panel; };

  auto Rescale() -> void;

  auto UpdateStatus() -> void;

  auto Start() -> void;
  auto Stop() -> void;

  static auto MakePanelClass() -> bool;

private:
  DevicePanel& operator=( DevicePanel const & ) = delete;

  std::shared_ptr<ISolver> const solver;

  HWND panel;
  HWND name;
  HWND temp;
  HWND temp_cap;
  HWND core;
  HWND core_cap;
  HWND mem;
  HWND mem_cap;
  HWND intensity;
  HWND hashrate;
  HWND hashrate_cap;
  HWND button;

  int32_t y_offset;
  wchar_t t_string[16];
};

#endif
