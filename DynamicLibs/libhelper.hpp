/*
Copyright (c) 2017 Benoit Blanchon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#if !defined(_LIBHELPER_HPP_)
#define _LIBHELPER_HPP_

#include <type_traits>

#if defined(_MSC_VER)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dlfcn.h>

using FARPROC = void*;
using HMODULE = void*;
using LPCTSTR = char const*;
using LPCSTR = char const*;

inline auto LoadLibrary( LPCTSTR lib ) -> void*
{
  return dlopen( lib, RTLD_NOW );
}
inline constexpr auto& GetProcAddress = dlsym;
inline constexpr auto& FreeLibrary = dlclose;
#endif // defined(_MSC_VER)

class ProcPtr
{
public:
  explicit ProcPtr( FARPROC ptr ) : _ptr( ptr ) {}

  template <typename T, typename = std::enable_if_t<std::is_function_v<T>>>
  operator T*() const
  {
    return reinterpret_cast<T*>(reinterpret_cast<void*>(_ptr));
  }

private:
  FARPROC _ptr;
};

class DllHelper
{
public:
  explicit DllHelper( LPCTSTR filename ) : _module( LoadLibrary( filename ) ) {}

  ~DllHelper() { FreeLibrary( _module ); }

  ProcPtr operator[]( LPCSTR proc_name ) const
  {
    return ProcPtr( GetProcAddress( _module, proc_name ) );
  }

  static HMODULE _parent_module;

private:
  HMODULE _module;
};

#endif // !defined(_LIBHELPER_HPP_  
