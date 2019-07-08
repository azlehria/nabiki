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

#if defined GUI

#include "devicepanel.h"
#include "winapi_helpers.h"
#include "platforms.h"
#include "minercore.h"
#include "isolver.h"

#include <cinttypes>
#include <cwchar>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

extern std::vector<DevicePanel> devices;

namespace
{
  static int32_t next_y{ 80 };
  static uint64_t panel_number{ 0 };

  static auto __stdcall PnlProc( HWND hwnd, uint32_t msg, uint64_t wParam, int64_t lParam ) -> int64_t
  {
    switch( msg )
    {
    case WM_COMMAND:
      {
        if( HIWORD( wParam ) != BN_CLICKED )
        { break; }

        if( wParam < panel_number )
        {
          wchar_t bName[8]{ 0 };
          GetWindowTextW( reinterpret_cast<HWND>(lParam), bName, 7 );
          if( !wcscmp( bName, L"Start" ) )
          {
            SetWindowTextW( (HWND)lParam, L"Stop" );
            devices[wParam].Start();
          }
          else
          {
            SetWindowTextW( (HWND)lParam, L"Start" );
            devices[wParam].Stop();
          }
        }
      }
      break;
    default:
      return DefWindowProcW( hwnd, msg, wParam, lParam );
    }
    return 0;
  }
}

DevicePanel::DevicePanel( std::shared_ptr<ISolver> const solver ) :
  solver( solver ),
  panel( CreateWindowExW( NULL, WC_DIALOG, L"", WS_VISIBLE | WS_CHILD, 10, next_y, 600, 23, hMainWindow, NULL, hRootInstance, NULL ) ),
  name( MakeStatic( L"-", 10, 3, 140, NULL, panel, hRootInstance ) ),
  temp( MakeStatic( L"100", 155, 3, 30, SS_RIGHT, panel, hRootInstance ) ),
  temp_cap( MakeStatic( L"°C", 185, 3, 13, NULL, panel, hRootInstance ) ),
  core( MakeStatic( L"9999", 210, 3, 30, SS_RIGHT, panel, hRootInstance ) ),
  core_cap( MakeStatic( L"MHz", 243, 3, 26, NULL, panel, hRootInstance ) ),
  mem( MakeStatic( L"9999", 275, 3, 30, SS_RIGHT, panel, hRootInstance ) ),
  mem_cap( MakeStatic( L"MHz", 308, 3, 26, NULL, panel, hRootInstance ) ),
  intensity( MakeStatic( L"22.22", 340, 3, 35, SS_RIGHT, panel, hRootInstance ) ),
  hashrate( MakeStatic( L"9999.99", 400, 3, 40, SS_RIGHT, panel, hRootInstance ) ),
  hashrate_cap( MakeStatic( L"MH/s", 443, 3, 31, NULL, panel, hRootInstance ) ),
  button( CreateWindowExW( NULL, L"BUTTON", L"Stop", /*WS_TABSTOP |*/ WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 530, 0, 65, 23, panel, reinterpret_cast<HMENU>(panel_number++), hRootInstance, NULL ) ),
  y_offset( next_y )
{
  SetFont( name, hFontBold );
  SetFont( temp, hFont );
  SetFont( temp_cap, hFont );
  SetFont( core, hFont );
  SetFont( core_cap, hFont );
  SetFont( mem, hFont );
  SetFont( mem_cap, hFont );
  SetFont( intensity, hFont );
  SetFont( hashrate, hFont );
  SetFont( hashrate_cap, hFont );
  SetFont( button, hFont );

  next_y += 23;

  Rescale();
  ShowWindow( panel, SW_HIDE );
}

auto DevicePanel::Rescale() -> void
{
  if( !UseSimpleUI ) return;

  int32_t const lDpi{ static_cast<int32_t>(GetDpiForWindow( hMainWindow )) };

  if( lDpi == USER_DEFAULT_SCREEN_DPI ) return;

  RescaleControl( panel, MulDiv( 10, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( y_offset, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 600, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 23, lDpi, USER_DEFAULT_SCREEN_DPI ) );

  int32_t const staticHeight{ MulDiv( 16, lDpi, USER_DEFAULT_SCREEN_DPI ) };
  int32_t const staticYOffset{ MulDiv( 3, lDpi, USER_DEFAULT_SCREEN_DPI ) };

  RescaleControl( name, MulDiv( 10, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 140, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );

  RescaleControl( temp, MulDiv( 155, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 30, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( temp_cap, MulDiv( 185, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 13, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( core, MulDiv( 210, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 30, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( core_cap, MulDiv( 243, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( mem, MulDiv( 275, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 30, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( mem_cap, MulDiv( 308, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( intensity, MulDiv( 340, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 35, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( hashrate, MulDiv( 395, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 45, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( hashrate_cap, MulDiv( 443, lDpi, USER_DEFAULT_SCREEN_DPI ), staticYOffset, MulDiv( 31, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  RescaleControl( button, MulDiv( 530, lDpi, USER_DEFAULT_SCREEN_DPI ), 0, MulDiv( 65, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 23, lDpi, USER_DEFAULT_SCREEN_DPI ) );
}

auto DevicePanel::UpdateStatus() -> void
{
  if( GetWindowTextLength( name ) == 1 )
  {
    UpdateControl( name, std::wstring( solver->getName().begin(), solver->getName().end() ).c_str() );
  }
  UpdateControl( temp, std::to_wstring( solver->getTemperature() ).c_str() );
  GetNumberFormatW( LOCALE_USER_DEFAULT, NULL, std::to_wstring( solver->getClockCore() ).c_str(), &fmtInt, t_string, 16 );
  UpdateControl( core, t_string );
  GetNumberFormatW( LOCALE_USER_DEFAULT, NULL, std::to_wstring( solver->getClockMem() ).c_str(), &fmtInt, t_string, 16 );
  UpdateControl( mem, t_string );
  GetNumberFormatW( LOCALE_USER_DEFAULT, NULL, std::to_wstring( solver->getHashrate() ).c_str(), &fmtFloat, t_string, 16 );
  UpdateControl( hashrate, t_string );
  GetNumberFormatW( LOCALE_USER_DEFAULT, NULL, std::to_wstring( solver->getIntensity() ).c_str(), &fmtFloat, t_string, 16 );
  UpdateControl( intensity, t_string );
}

auto DevicePanel::Start() -> void
{
  solver->startFinding();
}

auto DevicePanel::Stop() -> void
{
  solver->stopFinding();
}

auto DevicePanel::MakePanelClass() -> bool
{
  WNDCLASSEX panelClass{ /*cbSize*/        sizeof( WNDCLASSEXW ),
                         /*style*/         0,
                         /*lpfnWndProc*/   PnlProc,
                         /*cbClsExtra*/    0,
                         /*cbWndExtra*/    DLGWINDOWEXTRA,
                         /*hInstance*/     hRootInstance,
                         /*hIcon*/         LoadIconW( NULL, IDI_APPLICATION ),
                         /*hCursor*/       LoadCursorW( NULL, IDC_ARROW ),
                         /*hbrBackground*/ HBRUSH( COLOR_WINDOW ),
                         /*lpszMenuName*/  NULL,
                         /*lpszClassName*/ WC_DIALOG,
                         /*hIconSm*/       LoadIconW( NULL, IDI_APPLICATION ) };

  if( !RegisterClassExW( &panelClass ) )
  {
    MessageBoxW( NULL, L"Window Registration Failed!", L"Error!",
      MB_ICONEXCLAMATION | MB_OK );
    return false;
  }

  return true;
}

#endif // GUI
