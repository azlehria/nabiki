#ifndef _CLSOLVER_CL_
#define _CLSOLVER_CL_

using namespace std::string_literals;

std::string const clKeccakSource = R"(
#define ROTL64(x, y) (((x) << (y)) ^ ((x) >> (64 - (y))))
#define ROTR64(x, y) (((x) >> (y)) ^ ((x) << (64 - (y))))

__constant const ulong RC[24] = {
  0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
  0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
  0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
  0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
  0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
  0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
  0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
  0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

// ulong bswap_64( ulong const input )
// {
// #if CUDA_VERSION
//   ulong output;
//   asm( "{"
//        "  prmt.b32 %0, %3, 0, 0x0123;"
//        "  prmt.b32 %1, %2, 0, 0x0123;"
//        "}" : "=r"(as_uint2(output).x), "=r"(as_uint2(output).y)
//            : "r"(as_uint2(input).x), "r"(as_uint2(input).y) );
//   return output;
// #else

// #endif
// }

ulong xor5( ulong const a, ulong const b, ulong const c, ulong const d, ulong const e )
{
#if CUDA_VERSION
  ulong output;
  asm( "{"
       "  xor.b64 %0, %1, %2;"
       "  xor.b64 %0, %0, %3;"
       "  xor.b64 %0, %0, %4;"
       "  xor.b64 %0, %0, %5;"
       "}" : "=l"(output) : "l"(a), "l"(b), "l"(c), "l"(d), "l"(e) );
  return output;
#else
  return a ^ b ^ c ^ d ^ e;
#endif
}

ulong xor3( ulong const a, ulong const b, ulong const c )
{
#if CUDA_VERSION
  ulong output;
#if 0//CUDA_VERSION >= 500
  asm( "{"
       "  lop3.b32 %0, %2, %4, %6, 0x96;"
       "  lop3.b32 %1, %3, %5, %7, 0x96;"
       "}" : "=r"(as_uint2(output).x), "=r"(as_uint2(output).y)
           : "r"(as_uint2(a).x), "r"(as_uint2(a).y),
             "r"(as_uint2(b).x), "r"(as_uint2(b).y),
             "r"(as_uint2(c).x), "r"(as_uint2(c).y) );
#else
  asm( "{"
       "  xor.b64 %0, %1, %2;"
       "  xor.b64 %0, %0, %3;"
       "}" : "=l"(output) : "l"(a), "l"(b), "l"(c) );
#endif
  return output;
#else
  return a ^ b ^ c;
#endif
}

ulong chi( ulong const a, ulong const b, ulong const c )
{
#if 0//CUDA_VERSION >= 500
  ulong output;
  asm( "{"
       "  lop3.b32 %0, %2, %4, %6, 0xD2;"
       "  lop3.b32 %1, %3, %5, %7, 0xD2;"
       "}" : "=r"(as_uint2(output).x), "=r"(as_uint2(output).y)
           : "r"(as_uint2(a).x), "r"(as_uint2(a).y),
             "r"(as_uint2(b).x), "r"(as_uint2(b).y),
             "r"(as_uint2(c).x), "r"(as_uint2(c).y) );
  return output;
#else
  // bitselect( a ^ c, a, b );
  return a ^ ((~b) & c);
#endif
}

kernel
void cl_mine( global ulong* d_mid,
              global ulong* d_target,
              local ulong* solution,
              local volatile uint* cnt,
              ulong threads )
{
  ulong nounce = threads + get_global_id(0);

  printf( "%llu\n%llu\n%llu\n", d_target, solution, threads );

  ulong state[25], C[5], D[5];
  ulong n[11] = { ROTL64(nounce,  7) };
  n[ 1] = ROTL64(n[ 0],  1);
  n[ 2] = ROTL64(n[ 1],  6);
  n[ 3] = ROTL64(n[ 2],  2);
  n[ 4] = ROTL64(n[ 3],  4);
  n[ 5] = ROTL64(n[ 4],  7);
  n[ 6] = ROTL64(n[ 5], 12);
  n[ 7] = ROTL64(n[ 6],  5);
  n[ 8] = ROTL64(n[ 7], 11);
  n[ 9] = ROTL64(n[ 8],  7);
  n[10] = ROTL64(n[ 9],  1);

  C[0] = d_mid[ 0];
  C[1] = d_mid[ 1];
  C[2] = d_mid[ 2] ^ n[ 7];
  C[3] = d_mid[ 3];
  C[4] = d_mid[ 4] ^ n[ 2];
  state[ 0] = chi( C[0], C[1], C[2] ) ^ RC[0];
  state[ 1] = chi( C[1], C[2], C[3] );
  state[ 2] = chi( C[2], C[3], C[4] );
  state[ 3] = chi( C[3], C[4], C[0] );
  state[ 4] = chi( C[4], C[0], C[1] );

  C[0] = d_mid[ 5];
  C[1] = d_mid[ 6] ^ n[ 4];
  C[2] = d_mid[ 7];
  C[3] = d_mid[ 8];
  C[4] = d_mid[ 9] ^ n[ 9];
  state[ 5] = chi( C[0], C[1], C[2] );
  state[ 6] = chi( C[1], C[2], C[3] );
  state[ 7] = chi( C[2], C[3], C[4] );
  state[ 8] = chi( C[3], C[4], C[0] );
  state[ 9] = chi( C[4], C[0], C[1] );

  C[0] = d_mid[10];
  C[1] = d_mid[11] ^ n[ 0];
  C[2] = d_mid[12];
  C[3] = d_mid[13] ^ n[ 1];
  C[4] = d_mid[14];
  state[10] = chi( C[0], C[1], C[2] );
  state[11] = chi( C[1], C[2], C[3] );
  state[12] = chi( C[2], C[3], C[4] );
  state[13] = chi( C[3], C[4], C[0] );
  state[14] = chi( C[4], C[0], C[1] );

  C[0] = d_mid[15] ^ n[ 5];
  C[1] = d_mid[16];
  C[2] = d_mid[17];
  C[3] = d_mid[18] ^ n[ 3];
  C[4] = d_mid[19];
  state[15] = chi( C[0], C[1], C[2] );
  state[16] = chi( C[1], C[2], C[3] );
  state[17] = chi( C[2], C[3], C[4] );
  state[18] = chi( C[3], C[4], C[0] );
  state[19] = chi( C[4], C[0], C[1] );

  C[0] = d_mid[20] ^ n[10];
  C[1] = d_mid[21] ^ n[ 8];
  C[2] = d_mid[22] ^ n[ 6];
  C[3] = d_mid[23];
  C[4] = d_mid[24];
  state[20] = chi( C[0], C[1], C[2] );
  state[21] = chi( C[1], C[2], C[3] );
  state[22] = chi( C[2], C[3], C[4] );
  state[23] = chi( C[3], C[4], C[0] );
  state[24] = chi( C[4], C[0], C[1] );

#if CUDA_VERSION >= 350
#  pragma unroll
#endif
  for( uchar i = 1; i < 23; ++i )
  {
    for( uchar x = 0; x < 5; ++x )
    {
      C[(x + 6) % 5] = xor5( state[x], state[x + 5], state[x + 10], state[x + 15], state[x + 20] );
    }

#if CUDA_VERSION >= 350
    for( uchar x = 0; x < 5; ++x )
    {
			D[x] = ROTL64(C[(x + 2) % 5], 1);
      state[x]      = xor3( state[x]     , D[x], C[x] );
      state[x +  5] = xor3( state[x +  5], D[x], C[x] );
      state[x + 10] = xor3( state[x + 10], D[x], C[x] );
      state[x + 15] = xor3( state[x + 15], D[x], C[x] );
      state[x + 20] = xor3( state[x + 20], D[x], C[x] );
    }
#else
    for( uchar x = 0; x < 5; ++x )
    {
      D[x] = ROTL64(C[(x + 2) % 5], 1) ^ C[x];
      state[x]      = state[x]      ^ D[x];
      state[x +  5] = state[x +  5] ^ D[x];
      state[x + 10] = state[x + 10] ^ D[x];
      state[x + 15] = state[x + 15] ^ D[x];
      state[x + 20] = state[x + 20] ^ D[x];
    }
#endif

    C[0] = state[1];
    state[ 1] = ROTR64( state[ 6], 20 );
    state[ 6] = ROTL64( state[ 9], 20 );
    state[ 9] = ROTR64( state[22],  3 );
    state[22] = ROTR64( state[14], 25 );
    state[14] = ROTL64( state[20], 18 );
    state[20] = ROTR64( state[ 2],  2 );
    state[ 2] = ROTR64( state[12], 21 );
    state[12] = ROTL64( state[13], 25 );
    state[13] = ROTL64( state[19],  8 );
    state[19] = ROTR64( state[23],  8 );
    state[23] = ROTR64( state[15], 23 );
    state[15] = ROTL64( state[ 4], 27 );
    state[ 4] = ROTL64( state[24], 14 );
    state[24] = ROTL64( state[21],  2 );
    state[21] = ROTR64( state[ 8],  9 );
    state[ 8] = ROTR64( state[16], 19 );
    state[16] = ROTR64( state[ 5], 28 );
    state[ 5] = ROTL64( state[ 3], 28 );
    state[ 3] = ROTL64( state[18], 21 );
    state[18] = ROTL64( state[17], 15 );
    state[17] = ROTL64( state[11], 10 );
    state[11] = ROTL64( state[ 7],  6 );
    state[ 7] = ROTL64( state[10],  3 );
    state[10] = ROTL64( C[0], 1 );

    for( uchar x = 0; x < 25; x += 5 )
    {
      C[0] = state[x];
      C[1] = state[x + 1];
      C[2] = state[x + 2];
      C[3] = state[x + 3];
      C[4] = state[x + 4];
      state[x]     = chi( C[0], C[1], C[2] );
      state[x + 1] = chi( C[1], C[2], C[3] );
      state[x + 2] = chi( C[2], C[3], C[4] );
      state[x + 3] = chi( C[3], C[4], C[0] );
      state[x + 4] = chi( C[4], C[0], C[1] );
    }

    state[0] = state[0] ^ RC[i];
  }

  for( uchar x = 0; x < 5; ++x )
  {
    C[(x + 6) % 5 ] = xor5( state[x], state[x + 5], state[x + 10], state[x + 15], state[x + 20] );
  }

  D[0] = ROTL64(C[2], 1);
  D[1] = ROTL64(C[3], 1);
  D[2] = ROTL64(C[4], 1);

  state[ 0] = xor3( state[ 0], D[0], C[0] );
  state[ 6] = xor3( state[ 6], D[1], C[1] );
  state[12] = xor3( state[12], D[2], C[2] );
  state[ 6] = ROTR64(state[ 6], 20);
  state[12] = ROTR64(state[12], 21);

  state[ 0] = chi( state[ 0], state[ 6], state[12] ) ^ RC[23];

  if( as_ulong(as_uchar8(state[0]).s7s6s5s4s3s2s1s0) <= *d_target )
  {
    uint cIdx = atomic_add( cnt, 1 );
    if( cIdx >= 256 ) return;
    
    solution[cIdx] = nounce;
  }
}
)"s;

#endif // !_CLSOLVER_CL_
