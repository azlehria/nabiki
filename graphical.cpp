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

#include "ui.h"
#include "minercore.h"
#include "miner_state.h"
#include "resource.h"
#include "winapi_helpers.h"
#include "devicepanel.h"

#include <string>
#include <sstream>
#include <chrono>
#include <tchar.h>
#include <vector>

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

HINSTANCE hRootInstance;
HWND hMainWindow;
HFONT hFont;
HFONT hFontBold;
std::vector<DevicePanel> devices;

using namespace std::chrono;

namespace
{
  enum : uint32_t
  {
    WM_APP_UPDATE_CHALLENGE = WM_APP,
    WM_APP_UPDATE_DIFFICULTY,
    WM_APP_UPDATE_URL,
    WM_APP_UPDATE_ADDRESS,
    WM_APP_UPDATE_HASHRATE,
    WM_APP_UPDATE_SOLUTIONS,
    WM_APP_UPDATE_DEV_SOLUTIONS,
  };

  static HWND hChallengeLabel;
  static HWND hDifficultyLabel;
  static HWND hPoolUrlLabel;
  static HWND hAddressLabel;
  static HWND hHashrateLabel;
  static HWND hSolutionsLabel;
  static HWND hDevSolutionsLabel;

  static HWND hChallengeText;
  static HWND hDifficultyText;
  static HWND hPoolUrlText;
  static HWND hAddressText;
  static HWND hHashrateText;
  static HWND hSolutionsText;
  static HWND hDevSolutionsText;

  static HWND hPoolSection;
  static HWND hMinerSection;
  static HWND hDeviceSection;

  static HWND hPoolLine;
  static HWND hMinerLine;
  static HWND hDeviceLine;

  static double hashrate{ 0 };
  static uint64_t solutionCount{ 0u };
  static uint64_t devSolutionCount{ 0u };

  static std::wstring sChallenge{ L"--" };
  static wchar_t sDiff[16]{ L"--" };
  static std::wstring sPoolUrl{ L"--" };
  static std::wstring sAddress{ L"--" };
  static wchar_t sHashrate[16]{ L"--" };
  static wchar_t sSolutions[16]{ L"0" };
  static wchar_t sDevSolutions[16]{ L"0" };

  static auto MakeFonts() -> void
  {
    NONCLIENTMETRICSW ncm{ sizeof( NONCLIENTMETRICSW ) };
    SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, NULL );

    if( !UseSimpleUI )
    {
      int32_t const lDpi{ static_cast<int32_t>(GetDpiForWindow( hMainWindow )) };
      if( lDpi && lDpi != USER_DEFAULT_SCREEN_DPI )
      {
        ncm.lfMessageFont.lfHeight = ncm.lfMessageFont.lfHeight * lDpi / USER_DEFAULT_SCREEN_DPI;
      }
    }
    hFont = CreateFontIndirectW( &ncm.lfMessageFont );

    ncm.lfMessageFont.lfWeight = FW_BOLD;
    hFontBold = CreateFontIndirectW( &ncm.lfMessageFont );
  }

  static auto UpdateDpiScaling( HWND hwnd ) -> void
  {
    if( !UseSimpleUI ) return;

    int32_t const lDpi{ static_cast<int32_t>(GetDpiForWindow( hwnd )) };

    if( lDpi == USER_DEFAULT_SCREEN_DPI ) return;

    int32_t const staticHeight{ MulDiv( 16, lDpi, USER_DEFAULT_SCREEN_DPI ) };

    RECT rect;
    GetWindowRect( hwnd, &rect );
    int x_padding{ GetSystemMetrics( SM_CXVSCROLL ) },
        y_padding{ GetSystemMetrics( SM_CXHSCROLL ) };
    RescaleWindow( hwnd, NULL, NULL, MulDiv( (rect.right - rect.left - x_padding), lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( (rect.bottom - rect.top - y_padding), lDpi, USER_DEFAULT_SCREEN_DPI ), SWP_NOMOVE );

    RescaleControl( hPoolSection, MulDiv( 10, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 5, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 25, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hPoolLine, MulDiv( 44, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 12, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 261, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 1, lDpi, USER_DEFAULT_SCREEN_DPI ) );
    RescaleControl( hMinerSection, MulDiv( 315, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 5, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 33, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hMinerLine, MulDiv( 357, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 12, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 253, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 1, lDpi, USER_DEFAULT_SCREEN_DPI ) );
    RescaleControl( hDeviceSection, MulDiv( 10, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 65, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 45, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hDeviceLine, MulDiv( 64, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 72, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 546, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 1, lDpi, USER_DEFAULT_SCREEN_DPI ) );

    RescaleControl( hChallengeLabel, MulDiv( 20, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 57, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hChallengeText, MulDiv( 82, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 51, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hDifficultyLabel, MulDiv( 156, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 55, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hDifficultyText, MulDiv( 216, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 69, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hPoolUrlLabel, MulDiv( 20, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 44, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 25, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hPoolUrlText, MulDiv( 50, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 44, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 312, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hAddressLabel, MulDiv( 325, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 48, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hAddressText, MulDiv( 378, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 60, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hHashrateLabel, MulDiv( 325, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 44, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 52, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hHashrateText, MulDiv( 382, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 44, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 56, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hSolutionsLabel, MulDiv( 471, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 54, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hSolutionsText, MulDiv( 530, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 26, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 70, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hDevSolutionsLabel, MulDiv( 471, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 44, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 64, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
    RescaleControl( hDevSolutionsText, MulDiv( 540, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 44, lDpi, USER_DEFAULT_SCREEN_DPI ), MulDiv( 60, lDpi, USER_DEFAULT_SCREEN_DPI ), staticHeight );
  }

  static auto DrawControls( HWND hwnd ) -> void
  {
    // Y+26 header to first line
    // Y+18 line to line
    // Y+21 line to new header
    MakeSection( hPoolSection, L"Pool", 10, 5, 25, hPoolLine, 295, hwnd, hRootInstance );

    MakeLabelledControl( hChallengeLabel, L"Challenge:", 20, 26, 57, hChallengeText, sChallenge.c_str(), 51, SS_RIGHT, hwnd, hRootInstance );
    MakeLabelledControl( hDifficultyLabel, L"Difficulty:", 156, 26, 55, hDifficultyText, sDiff, 69, SS_RIGHT, hwnd, hRootInstance );

    MakeLabelledControl( hPoolUrlLabel, L"URL:", 20, 44, 25, hPoolUrlText, sPoolUrl.c_str(), 312, NULL, hwnd, hRootInstance );

    MakeSection( hMinerSection, L"Miner", 315, 5, 33, hMinerLine, 295, hwnd, hRootInstance );

    MakeLabelledControl( hAddressLabel, L"Address:", 325, 26, 48, hAddressText, sAddress.c_str(), 60, SS_RIGHT, hwnd, hRootInstance );
    MakeLabelledControl( hSolutionsLabel, L"Solutions:", 471, 26, 54, hSolutionsText, sSolutions, 70, SS_RIGHT, hwnd, hRootInstance );

    MakeLabelledControl( hHashrateLabel, L"Hashrate:", 325, 44, 52, hHashrateText, sHashrate, 56, SS_RIGHT, hwnd, hRootInstance );
    MakeLabelledControl( hDevSolutionsLabel, L"Dev shares:", 471, 44, 64, hDevSolutionsText, sDevSolutions, 60, SS_RIGHT, hwnd, hRootInstance );

    MakeSection( hDeviceSection, L"Devices", 10, 65, 45, hDeviceLine, 600, hwnd, hRootInstance );

    UpdateDpiScaling( hwnd );
  }

  static auto __stdcall WndProc( HWND hwnd, uint32_t msg, uint64_t wParam, int64_t lParam ) -> int64_t
  {
    switch( msg )
    {
      case WM_APP_UPDATE_CHALLENGE:
        UpdateControl( hChallengeText, sChallenge.c_str() );
        break;
      case WM_APP_UPDATE_DIFFICULTY:
        UpdateControl( hDifficultyText, sDiff );
        break;
      case WM_APP_UPDATE_URL:
        UpdateControl( hPoolUrlText, sPoolUrl.c_str() );
        break;
      case WM_APP_UPDATE_ADDRESS:
        UpdateControl( hAddressText, sAddress.c_str() );
        break;
      case WM_APP_UPDATE_HASHRATE:
        {
          static auto time_counter{ steady_clock::now() + 100ms };
          if( time_counter > steady_clock::now() ) break;
          time_counter = steady_clock::now() + 100ms;

          hashrate = 0;
          for( auto const& temp : MinerCore::getHashrates() )
          {
            hashrate += temp;
          }

          GetNumberFormatW( LOCALE_USER_DEFAULT, NULL, std::to_wstring( hashrate ).c_str(), &fmtFloat, sHashrate, 16 );
          UpdateControl( hHashrateText, sHashrate );
          break;
        }
      case WM_APP_UPDATE_SOLUTIONS:
        GetNumberFormatW( LOCALE_USER_DEFAULT, NULL, std::to_wstring( solutionCount ).c_str(), &fmtInt, sSolutions, 16 );
        UpdateControl( hSolutionsText, sSolutions );
        break;
      case WM_APP_UPDATE_DEV_SOLUTIONS:
        GetNumberFormatW( LOCALE_USER_DEFAULT, NULL, std::to_wstring( devSolutionCount ).c_str(), &fmtInt, sDevSolutions, 16 );
        UpdateControl( hDevSolutionsText, sDevSolutions );
        break;
      //case WM_COMMAND:
      //  {
      //    if( HIWORD( wParam ) != BN_CLICKED )
      //    { break; }

      //    auto id = size_t{ LOWORD( wParam ) };
      //    if( id < devices.size() )
      //    {
      //      SetWindowTextW( (HWND)lParam, L"STOP" );
      //    }
      //  }
      //  break;
      case WM_TIMER:
      {
        for( auto& device : devices )
        {
          device.UpdateStatus();
        }
        break;
      }
      case WM_DPICHANGED:
        MakeFonts();
        for( auto& dev : devices )
        {
          dev.Rescale();
        }
        UpdateDpiScaling( hwnd );
        break;
      case WM_CHAR:
        if( VK_SPACE == static_cast<char>( wParam ) )
        {
          if( devices.empty() )
          {
            if( MinerCore::getActiveDeviceCount() > 0 )
            {
              //device layout testing
              for( auto const& device_ref : MinerCore::getDeviceReferences() )
              {
                devices.emplace_back( device_ref );
              }
            }
            else
            {
              break;
            }
          }
          for( auto const& dev : devices )
          {
            ShowWindow( dev, IsWindowVisible( dev ) ? SW_HIDE : SW_SHOW );
          }
          RECT rect;
          GetWindowRect( hwnd, &rect );
          SetWindowPos( hwnd, NULL, NULL, NULL, (rect.right - rect.left),
                        ((IsWindowVisible( devices[0] ) ? 1 : -1) * static_cast<int>(devices.size()) * 23) + (rect.bottom - rect.top),
                        SWP_NOMOVE );
          if( IsWindowVisible( devices[0] ) )
          {
            SendMessageW( hwnd, WM_TIMER, NULL, NULL );
            SetTimer( hwnd, 0u, 200u, NULL );
          }
          else
          {
            KillTimer( hwnd, 0u );
          }
        }
        break;
      case WM_CREATE:
        MakeFonts();
        DrawControls( hwnd );
        break;
      case WM_CLOSE:
        MinerCore::stop();
        DestroyWindow( hwnd );
        break;
      case WM_NCDESTROY:
        PostQuitMessage( 0 );
        break;
      default:
        return DefWindowProcW( hwnd, msg, wParam, lParam );
    }
    return 0;
  }

  static auto MakeWinClass() -> bool
  {
    WNDCLASSEX windowClass{ /*cbSize*/        sizeof( WNDCLASSEXW ),
                            /*style*/         0,
                            /*lpfnWndProc*/   WndProc,
                            /*cbClsExtra*/    0,
                            /*cbWndExtra*/    0,
                            /*hInstance*/     hRootInstance,
                            /*hIcon*/         LoadIconW( NULL, IDI_APPLICATION ),
                            /*hCursor*/       LoadCursorW( NULL, IDC_ARROW ),
                            /*hbrBackground*/ HBRUSH( COLOR_WINDOW ),
                            /*lpszMenuName*/  NULL,
                            /*lpszClassName*/ L"nabiki",
                            /*hIconSm*/       LoadIconW( NULL, IDI_APPLICATION ) };

    if( !RegisterClassExW( &windowClass ) )
    {
      MessageBoxW( NULL, L"Window Registration Failed!", L"Error!",
                   MB_ICONEXCLAMATION | MB_OK );
      return false;
    }

    return true;
  }

  static auto MakeWindow() -> bool
  {
    hMainWindow = CreateWindowExW( NULL, L"nabiki",
                                   std::wstring( MinerCore::MINER_VERSION.begin(),
                                                 MinerCore::MINER_VERSION.end() ).c_str(),
                                   WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 640, /*196*/ 127,
                                   NULL, NULL, hRootInstance, NULL );

    if( hMainWindow == NULL )
    {
      MessageBoxW( NULL, L"Window Creation Failed!", L"Error!",
                   MB_ICONEXCLAMATION | MB_OK );
      return false;
    }

    ShowWindow( hMainWindow, SW_SHOW );
    UpdateWindow( hMainWindow );

    return true;
  }
}

namespace UI
{
  auto Run() -> int32_t
  {
    hRootInstance = GetModuleHandleW( NULL );

    if( !MakeWinClass() || !MakeWindow() || !DevicePanel::MakePanelClass() )
      return 0;

    SetThreadExecutionState( ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED );

    MSG message;
    // "Boolean" signed integer with semantic differences
    // between positive/negative values. Wonderful.
    while( GetMessageW( &message, NULL, NULL, NULL ) > WM_NULL )
    {
      TranslateMessage( &message );
      DispatchMessageW( &message );
    }

    SetThreadExecutionState( ES_CONTINUOUS );

    return static_cast<int>( message.wParam );
  }

  auto Stop() -> void
  {}

  auto Init() -> void
  {}

  auto UpdateChallenge( std::string_view const challenge ) -> void
  {
    sChallenge = { challenge.begin(), challenge.end() };
    PostMessageW( hMainWindow, WM_APP_UPDATE_CHALLENGE, NULL, NULL );
  }

  auto UpdateDifficulty( uint64_t const& diff ) -> void
  {
    GetNumberFormatW( LOCALE_USER_DEFAULT, NULL, std::to_wstring( diff ).c_str(), &fmtInt, sDiff, 16 );
    PostMessageW( hMainWindow, WM_APP_UPDATE_DIFFICULTY, NULL, NULL );
  }

  auto UpdateUrl( std::string_view const& url ) -> void
  {
    sPoolUrl = { url.begin(), url.end() };
    PostMessageW( hMainWindow, WM_APP_UPDATE_URL, NULL, NULL );
  }

  auto UpdateAddress( std::string_view const address ) -> void
  {
    sAddress = { address.begin(), address.end() };
    PostMessageW( hMainWindow, WM_APP_UPDATE_ADDRESS, NULL, NULL );
  }

  auto UpdateHashrate( [[maybe_unused]] uint64_t const& count ) -> void
  {
    PostMessageW( hMainWindow, WM_APP_UPDATE_HASHRATE, NULL, NULL );
  }

  auto UpdateSolutions( uint64_t const& count ) -> void
  {
    solutionCount = count;
    PostMessageW( hMainWindow, WM_APP_UPDATE_SOLUTIONS, NULL, NULL );
  }

  auto UpdateDevSolutions( uint64_t const& count ) -> void
  {
    devSolutionCount = count;
    PostMessageW( hMainWindow, WM_APP_UPDATE_DEV_SOLUTIONS, NULL, NULL );
  }
}

#endif // GUI
