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

#if !defined _WINAPI_HELPERS_H_
#define _WINAPI_HELPERS_H_

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

NUMBERFMT extern fmtInt;
NUMBERFMT extern fmtFloat;
HINSTANCE extern hRootInstance;
HWND extern hMainWindow;
HFONT extern hFont;
HFONT extern hFontBold;

auto inline SetFont( HWND hWnd, HFONT hFnt ) -> int64_t
{
  return SendMessageW( hWnd, WM_SETFONT, uint64_t( hFnt ), MAKELPARAM( FALSE, 0 ) );
}

auto inline MakeStatic( wchar_t const* __restrict lpWindowName, int32_t x, int32_t y, int32_t nWidth, uint32_t addlFlags, HWND& hParent, HINSTANCE& hInst ) -> HWND
{
  return CreateWindowExW( NULL, L"STATIC", lpWindowName, WS_CHILD | WS_VISIBLE | addlFlags, x, y, nWidth, 16, hParent, NULL, hInst, NULL );
}

auto inline MakeLine( int32_t x, int32_t y, int32_t nWidth, HWND& hParent, HINSTANCE& hInst ) -> HWND
{
  return CreateWindowExW( NULL, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME, x, y, nWidth, 1, hParent, NULL, hInst, NULL );
}

auto inline UpdateControl( HWND& hWnd, wchar_t const* __restrict lpString )
{
  SetWindowTextW( hWnd, lpString );
  InvalidateRect( hWnd, NULL, FALSE );
}

auto inline MakeLabelledControl( HWND& lblWnd, wchar_t const* lblString, int32_t x, int32_t y, int32_t lblWidth, HWND& ctlWnd, wchar_t const* __restrict ctlString, int32_t ctlWidth, uint32_t addlFlags, HWND& hParent, HINSTANCE& hInst )
{
  lblWnd = MakeStatic( lblString, x, y, lblWidth, NULL, hParent, hInst );
  SetFont( lblWnd, hFontBold );
  ctlWnd = MakeStatic( ctlString, x + lblWidth + 5, y, ctlWidth, addlFlags, hParent, hInst );
  SetFont( ctlWnd, hFont );
}

auto inline MakeSection( HWND& lblWnd, wchar_t const* __restrict lblString, int32_t x, int32_t y, int32_t lblWidth, HWND& lineWnd, int32_t secWidth, HWND& hParent, HINSTANCE& hInst )
{
  lblWnd = MakeStatic( lblString, x, y, lblWidth, NULL, hParent, hInst );
  SetFont( lblWnd, hFontBold );
  lineWnd = MakeLine( x + lblWidth + 9, y + 7, secWidth - lblWidth - 9, hParent, hInst );
}

auto inline RescaleWindow( HWND& hWnd, int32_t x, int32_t y, int32_t cx, int32_t cy, uint32_t addlFlags )
{
  SetWindowPos( hWnd, NULL, x, y, cx, cy, addlFlags | SWP_NOZORDER | SWP_NOACTIVATE );
  InvalidateRect( hWnd, NULL, FALSE );
}

auto inline RescaleControl( HWND& hWnd, int32_t x, int32_t y, int32_t cx, int32_t cy )
{
  SetWindowPos( hWnd, NULL, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE );
  InvalidateRect( hWnd, NULL, FALSE );
}

#endif
