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

#if !defined _CLSOLVER_CL_
#define _CLSOLVER_CL_

using namespace std::string_literals;

std::string const clKeccakSource{ R"(  
#pragma OPENCL EXTENSION cl_khr_int32_base_atomics : enable 

constant ulong const RC[22] = { 0x0000000000008082, 0x800000000000808a,
  0x8000000080008000, 0x000000000000808b, 0x0000000080000001, 0x8000000080008081,
  0x8000000000008009, 0x000000000000008a, 0x0000000000000088, 0x0000000080008009,
  0x000000008000000a, 0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
  0x8000000000008003, 0x8000000000008002, 0x8000000000000080, 0x000000000000800a,
  0x800000008000000a, 0x8000000080008081, 0x8000000000008080, 0x0000000080000001
};

ulong const R64( ulong const x, ulong const y )
{
  return (x << y) ^ (x >> (64u - y));
}

ulong const xor5( ulong const a, ulong const b, ulong const c, ulong const d, ulong const e )
{
  return a ^ b ^ c ^ d ^ e;
}

ulong const xor3( ulong const a, ulong const b, ulong const c )
{
  return a ^ b ^ c;
}

ulong const chi( ulong const a, ulong const b, ulong const c )
{
  return bitselect( a ^ c, a, b );
}

kernel
void cl_mine( constant ulong const* const restrict mid,
              ulong const target,
              global ulong* restrict sols,
              global volatile uint* restrict sol_cnt,
              ulong const threads )
{
  ulong const nounce = threads + get_global_id(0);

  uchar i, j;
  ulong s[25], C[5], D;
  ulong n[11] = { R64(nounce,  7) };
  n[ 1] = R64(n[ 0],  1);
  n[ 2] = R64(n[ 1],  6);
  n[ 3] = R64(n[ 2],  2);
  n[ 4] = R64(n[ 3],  4);
  n[ 5] = R64(n[ 4],  7);
  n[ 6] = R64(n[ 5], 12);
  n[ 7] = R64(n[ 6],  5);
  n[ 8] = R64(n[ 7], 11);
  n[ 9] = R64(n[ 8],  7);
  n[10] = R64(n[ 9],  1);

  C[0] = mid[ 0];
  C[1] = mid[ 1];
  C[2] = mid[ 2] ^ n[ 7];
  C[3] = mid[ 3];
  C[4] = mid[ 4] ^ n[ 2];
  s[ 0] = chi( C[0], C[1], C[2] ) ^ 0x0000000000000001;
  s[ 1] = chi( C[1], C[2], C[3] );
  s[ 2] = chi( C[2], C[3], C[4] );
  s[ 3] = chi( C[3], C[4], C[0] );
  s[ 4] = chi( C[4], C[0], C[1] );

  C[0] = mid[ 5];
  C[1] = mid[ 6] ^ n[ 4];
  C[2] = mid[ 7];
  C[3] = mid[ 8];
  C[4] = mid[ 9] ^ n[ 9];
  s[ 5] = chi( C[0], C[1], C[2] );
  s[ 6] = chi( C[1], C[2], C[3] );
  s[ 7] = chi( C[2], C[3], C[4] );
  s[ 8] = chi( C[3], C[4], C[0] );
  s[ 9] = chi( C[4], C[0], C[1] );

  C[0] = mid[10];
  C[1] = mid[11] ^ n[ 0];
  C[2] = mid[12];
  C[3] = mid[13] ^ n[ 1];
  C[4] = mid[14];
  s[10] = chi( C[0], C[1], C[2] );
  s[11] = chi( C[1], C[2], C[3] );
  s[12] = chi( C[2], C[3], C[4] );
  s[13] = chi( C[3], C[4], C[0] );
  s[14] = chi( C[4], C[0], C[1] );

  C[0] = mid[15] ^ n[ 5];
  C[1] = mid[16];
  C[2] = mid[17];
  C[3] = mid[18] ^ n[ 3];
  C[4] = mid[19];
  s[15] = chi( C[0], C[1], C[2] );
  s[16] = chi( C[1], C[2], C[3] );
  s[17] = chi( C[2], C[3], C[4] );
  s[18] = chi( C[3], C[4], C[0] );
  s[19] = chi( C[4], C[0], C[1] );

  C[0] = mid[20] ^ n[10];
  C[1] = mid[21] ^ n[ 8];
  C[2] = mid[22] ^ n[ 6];
  C[3] = mid[23];
  C[4] = mid[24];
  s[20] = chi( C[0], C[1], C[2] );
  s[21] = chi( C[1], C[2], C[3] );
  s[22] = chi( C[2], C[3], C[4] );
  s[23] = chi( C[3], C[4], C[0] );
  s[24] = chi( C[4], C[0], C[1] );

  for( i = 0; i < 22; ++i )
  {
    for( j = 0; j < 5; ++j )
    {
      C[(j + 6) % 5] = xor5( s[j], s[j + 5], s[j + 10], s[j + 15], s[j + 20] );
    }

    for( j = 0; j < 5; ++j )
    {
			D = R64( C[(j + 2) % 5], 1 );
      s[j]      = xor3( s[j]     , D, C[j] );
      s[j +  5] = xor3( s[j +  5], D, C[j] );
      s[j + 10] = xor3( s[j + 10], D, C[j] );
      s[j + 15] = xor3( s[j + 15], D, C[j] );
      s[j + 20] = xor3( s[j + 20], D, C[j] );
    }

    C[0] = s[1];
    s[ 1] = R64( s[ 6], 44 );
    s[ 6] = R64( s[ 9], 20 );
    s[ 9] = R64( s[22], 61 );
    s[22] = R64( s[14], 39 );
    s[14] = R64( s[20], 18 );
    s[20] = R64( s[ 2], 62 );
    s[ 2] = R64( s[12], 43 );
    s[12] = R64( s[13], 25 );
    s[13] = R64( s[19],  8 );
    s[19] = R64( s[23], 56 );
    s[23] = R64( s[15], 41 );
    s[15] = R64( s[ 4], 27 );
    s[ 4] = R64( s[24], 14 );
    s[24] = R64( s[21],  2 );
    s[21] = R64( s[ 8], 55 );
    s[ 8] = R64( s[16], 45 );
    s[16] = R64( s[ 5], 36 );
    s[ 5] = R64( s[ 3], 28 );
    s[ 3] = R64( s[18], 21 );
    s[18] = R64( s[17], 15 );
    s[17] = R64( s[11], 10 );
    s[11] = R64( s[ 7],  6 );
    s[ 7] = R64( s[10],  3 );
    s[10] = R64( C[0],  1 );

    for( j = 0; j < 25; j += 5 )
    {
      C[0] = s[j];
      C[1] = s[j + 1];
      C[2] = s[j + 2];
      C[3] = s[j + 3];
      C[4] = s[j + 4];
      s[j]     = chi( C[0], C[1], C[2] );
      s[j + 1] = chi( C[1], C[2], C[3] );
      s[j + 2] = chi( C[2], C[3], C[4] );
      s[j + 3] = chi( C[3], C[4], C[0] );
      s[j + 4] = chi( C[4], C[0], C[1] );
    }

    s[0] = s[0] ^ RC[i];
  }

  for( j = 0; j < 5; ++j )
  {
    C[(j + 6) % 5] = xor5( s[j], s[j + 5], s[j + 10], s[j + 15], s[j + 20] );
  }

  D = R64(C[2], 1);
  s[ 0] = xor3( s[ 0], D, C[0] );
  D = R64(C[3], 1);
  s[ 6] = xor3( s[ 6], D, C[1] );
  D = R64(C[4], 1);
  s[12] = xor3( s[12], D, C[2] );
  s[ 6] = R64(s[ 6], 44 );
  s[12] = R64(s[12], 43 );

  s[ 0] = chi( s[ 0], s[ 6], s[12] ) ^ 0x8000000080008008;

  if( as_ulong( as_uchar8( s[0] ).s76543210 ) <= target )
  {
		uint cIdx = atomic_inc( sol_cnt );
    if( cIdx >= 256 ) return;
    
    sols[cIdx] = nounce;
  }
}
)"s };

#endif // !_CLSOLVER_CL_
