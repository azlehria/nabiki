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

#include "utils.h"
#include "types.h"

#include <string>
#include <string_view>
#include <stdexcept>
#include <cassert>
#include <cstdint>
#include <cstdlib>

using namespace std::literals;

namespace
{
  static std::string_view constexpr ascii{ "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"sv };

  static auto fromAscii( uint8_t const c ) -> uint8_t const
  {
    if( c >= '0' && c <= '9' ) { return (c - 48u); }
    if( c >= 'a' && c <= 'f' ) { return (c - 87u); }
    if( c >= 'A' && c <= 'F' ) { return (c - 55u); }

    throw std::runtime_error( "invalid character" );
  }

  static auto ascii_r( uint8_t const a, uint8_t const b ) -> uint8_t const
  {
    return uint8_t( fromAscii( a ) << 4u ^ fromAscii( b ) );
  }

  template<typename T>
  static auto constexpr HexToBytes( std::string_view const hex, T& bytes ) -> void
  {
    //for( uint8_t i{ 0 }; i < hex.length() / 2; ++i )
    //{
    //  bytes[i] = uint8_t(ascii.find( hex.substr( i * 2u, 2u ) )) / 2u;
    //}
    for( std::string_view::size_type i = 0, j = 0; i < hex.length(); i += 2, ++j )
    {
      bytes[j] = ascii_r( uint8_t(hex[i]), uint8_t(hex[i + 1]) );
    }
  }
}

namespace Nabiki::Utils
{
  auto replaceSubstring( std::string& strOld, std::string_view const strFind, std::string_view const strNew ) -> void
  {
    std::string::size_type pos{ strOld.find( strFind ) };
    if( pos == std::string::npos ) { return; }

    do
    {
      pos = strOld.replace( pos, strFind.size(), strNew ).find( strFind, pos + strFind.size() );
    }
    while( pos != std::string::npos );
  }

  template<typename T>
  auto hexToBytes( std::string_view const hex, T& bytes ) -> void
  {
    assert( hex.length() % 2u == 0 );
    if( hex.substr( 0u, 2u ) == "0x"s || hex.substr( 0u, 2u ) == "0X"s )
      HexToBytes( hex.substr( 2u ), bytes );
    else
      HexToBytes( hex, bytes );
  }

  template<typename T>
  auto bytesToString( T buffer ) -> std::string
  {
    std::string output;
    output.reserve( buffer.size() * 2u + 1u );

    for( auto const& byte : buffer )
    {
      output += ascii.substr( 2ull * byte, 2u );
    }

    return output;
  }
}

// make these available externally
template auto Nabiki::Utils::hexToBytes( std::string_view const, message_t& ) -> void;
template auto Nabiki::Utils::hexToBytes( std::string_view const, hash_t& ) -> void;
template auto Nabiki::Utils::hexToBytes( std::string_view const, address_t& ) -> void;
template auto Nabiki::Utils::bytesToString( prefix_t ) -> std::string;
template auto Nabiki::Utils::bytesToString( hash_t ) -> std::string;
template auto Nabiki::Utils::bytesToString( address_t ) -> std::string;
