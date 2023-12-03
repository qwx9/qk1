/*============================================================================

This C header file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014 The Regents of the University of California.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#pragma once

#define SOFTFLOAT_FAST_INT64
#define LITTLEENDIAN
#define INLINE_LEVEL 0
#define SOFTFLOAT_FAST_DIV64TO32
#define SOFTFLOAT_FAST_DIV32TO16
#define SOFTFLOAT_ROUND_ODD
#define INLINE inline

#ifdef SOFTFLOAT_FAST_INT64

#ifdef LITTLEENDIAN
struct uint128 { u64int v0, v64; };
struct uint64_extra { u64int extra, v; };
struct uint128_extra { u64int extra; struct uint128 v; };
#else
struct uint128 { u64int v64, v0; };
struct uint64_extra { u64int v, extra; };
struct uint128_extra { struct uint128 v; u64int extra; };
#endif

#endif

/*----------------------------------------------------------------------------
| These macros are used to isolate the differences in word order between big-
| endian and little-endian platforms.
*----------------------------------------------------------------------------*/
#ifdef LITTLEENDIAN
#define wordIncr 1
#define indexWord( total, n ) (n)
#define indexWordHi( total ) ((total) - 1)
#define indexWordLo( total ) 0
#define indexMultiword( total, m, n ) (n)
#define indexMultiwordHi( total, n ) ((total) - (n))
#define indexMultiwordLo( total, n ) 0
#define indexMultiwordHiBut( total, n ) (n)
#define indexMultiwordLoBut( total, n ) 0
#define INIT_UINTM4( v3, v2, v1, v0 ) { v0, v1, v2, v3 }
#else
#define wordIncr -1
#define indexWord( total, n ) ((total) - 1 - (n))
#define indexWordHi( total ) 0
#define indexWordLo( total ) ((total) - 1)
#define indexMultiword( total, m, n ) ((total) - 1 - (m))
#define indexMultiwordHi( total, n ) 0
#define indexMultiwordLo( total, n ) ((total) - (n))
#define indexMultiwordHiBut( total, n ) 0
#define indexMultiwordLoBut( total, n ) (n)
#define INIT_UINTM4( v3, v2, v1, v0 ) { v3, v2, v1, v0 }
#endif

#ifndef softfloat_shortShiftRightJam64
/*----------------------------------------------------------------------------
| Shifts 'a' right by the number of bits given in 'dist', which must be in
| the range 1 to 63.  If any nonzero bits are shifted off, they are "jammed"
| into the least-significant bit of the shifted value by setting the least-
| significant bit to 1.  This shifted-and-jammed value is returned.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
u64int softfloat_shortShiftRightJam64( u64int a, u8int dist )
    { return a>>dist | ((a & (((u64int) 1<<dist) - 1)) != 0); }
#else
u64int softfloat_shortShiftRightJam64( u64int a, u8int dist );
#endif
#endif

#ifndef softfloat_shiftRightJam32
/*----------------------------------------------------------------------------
| Shifts 'a' right by the number of bits given in 'dist', which must not
| be zero.  If any nonzero bits are shifted off, they are "jammed" into the
| least-significant bit of the shifted value by setting the least-significant
| bit to 1.  This shifted-and-jammed value is returned.
|   The value of 'dist' can be arbitrarily large.  In particular, if 'dist' is
| greater than 32, the result will be either 0 or 1, depending on whether 'a'
| is zero or nonzero.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE u32int softfloat_shiftRightJam32( u32int a, u16int dist )
{
    return
        (dist < 31) ? a>>dist | ((u32int) (a<<(-dist & 31)) != 0) : (a != 0);
}
#else
u32int softfloat_shiftRightJam32( u32int a, u16int dist );
#endif
#endif

#ifndef softfloat_shiftRightJam64
/*----------------------------------------------------------------------------
| Shifts 'a' right by the number of bits given in 'dist', which must not
| be zero.  If any nonzero bits are shifted off, they are "jammed" into the
| least-significant bit of the shifted value by setting the least-significant
| bit to 1.  This shifted-and-jammed value is returned.
|   The value of 'dist' can be arbitrarily large.  In particular, if 'dist' is
| greater than 64, the result will be either 0 or 1, depending on whether 'a'
| is zero or nonzero.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (3 <= INLINE_LEVEL)
INLINE u64int softfloat_shiftRightJam64( u64int a, u32int dist )
{
    return
        (dist < 63) ? a>>dist | ((u64int) (a<<(-dist & 63)) != 0) : (a != 0);
}
#else
u64int softfloat_shiftRightJam64( u64int a, u32int dist );
#endif
#endif

#ifndef softfloat_countLeadingZeros16
/*----------------------------------------------------------------------------
| Returns the number of leading 0 bits before the most-significant 1 bit of
| 'a'.  If 'a' is zero, 16 is returned.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE u8int softfloat_countLeadingZeros16( u16int a )
{
    u8int count = 8;
    if ( 0x100 <= a ) {
        count = 0;
        a >>= 8;
    }
    count += softfloat_countLeadingZeros8[a];
    return count;
}
#else
u8int softfloat_countLeadingZeros16( u16int a );
#endif
#endif

#ifndef softfloat_countLeadingZeros32
/*----------------------------------------------------------------------------
| Returns the number of leading 0 bits before the most-significant 1 bit of
| 'a'.  If 'a' is zero, 32 is returned.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (3 <= INLINE_LEVEL)
INLINE u8int softfloat_countLeadingZeros32( u32int a )
{
    u8int count = 0;
    if ( a < 0x10000 ) {
        count = 16;
        a <<= 16;
    }
    if ( a < 0x1000000 ) {
        count += 8;
        a <<= 8;
    }
    count += softfloat_countLeadingZeros8[a>>24];
    return count;
}
#else
u8int softfloat_countLeadingZeros32( u32int a );
#endif
#endif

#ifndef softfloat_countLeadingZeros64
/*----------------------------------------------------------------------------
| Returns the number of leading 0 bits before the most-significant 1 bit of
| 'a'.  If 'a' is zero, 64 is returned.
*----------------------------------------------------------------------------*/
u8int softfloat_countLeadingZeros64( u64int a );
#endif

extern const u16int softfloat_approxRecip_1k0s[16];
extern const u16int softfloat_approxRecip_1k1s[16];

#ifndef softfloat_approxRecip32_1
/*----------------------------------------------------------------------------
| Returns an approximation to the reciprocal of the number represented by 'a',
| where 'a' is interpreted as an unsigned fixed-point number with one integer
| bit and 31 fraction bits.  The 'a' input must be "normalized", meaning that
| its most-significant bit (bit 31) must be 1.  Thus, if A is the value of
| the fixed-point interpretation of 'a', then 1 <= A < 2.  The returned value
| is interpreted as a pure unsigned fraction, having no integer bits and 32
| fraction bits.  The approximation returned is never greater than the true
| reciprocal 1/A, and it differs from the true reciprocal by at most 2.006 ulp
| (units in the last place).
*----------------------------------------------------------------------------*/
#ifdef SOFTFLOAT_FAST_DIV64TO32
#define softfloat_approxRecip32_1( a ) ((u32int) (UINT64_C( 0x7FFFFFFFFFFFFFFF ) / (u32int) (a)))
#else
u32int softfloat_approxRecip32_1( u32int a );
#endif
#endif

extern const u16int softfloat_approxRecipSqrt_1k0s[16];
extern const u16int softfloat_approxRecipSqrt_1k1s[16];

#ifndef softfloat_approxRecipSqrt32_1
/*----------------------------------------------------------------------------
| Returns an approximation to the reciprocal of the square root of the number
| represented by 'a', where 'a' is interpreted as an unsigned fixed-point
| number either with one integer bit and 31 fraction bits or with two integer
| bits and 30 fraction bits.  The format of 'a' is determined by 'oddExpA',
| which must be either 0 or 1.  If 'oddExpA' is 1, 'a' is interpreted as
| having one integer bit, and if 'oddExpA' is 0, 'a' is interpreted as having
| two integer bits.  The 'a' input must be "normalized", meaning that its
| most-significant bit (bit 31) must be 1.  Thus, if A is the value of the
| fixed-point interpretation of 'a', it follows that 1 <= A < 2 when 'oddExpA'
| is 1, and 2 <= A < 4 when 'oddExpA' is 0.
|   The returned value is interpreted as a pure unsigned fraction, having
| no integer bits and 32 fraction bits.  The approximation returned is never
| greater than the true reciprocal 1/sqrt(A), and it differs from the true
| reciprocal by at most 2.06 ulp (units in the last place).  The approximation
| returned is also always within the range 0.5 to 1; thus, the most-
| significant bit of the result is always set.
*----------------------------------------------------------------------------*/
u32int softfloat_approxRecipSqrt32_1( unsigned int oddExpA, u32int a );
#endif

#ifdef SOFTFLOAT_FAST_INT64

/*----------------------------------------------------------------------------
| The following functions are needed only when 'SOFTFLOAT_FAST_INT64' is
| defined.
*----------------------------------------------------------------------------*/

#ifndef softfloat_eq128
/*----------------------------------------------------------------------------
| Returns true if the 128-bit unsigned integer formed by concatenating 'a64'
| and 'a0' is equal to the 128-bit unsigned integer formed by concatenating
| 'b64' and 'b0'.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (1 <= INLINE_LEVEL)
INLINE
bool softfloat_eq128( u64int a64, u64int a0, u64int b64, u64int b0 )
    { return (a64 == b64) && (a0 == b0); }
#else
bool softfloat_eq128( u64int a64, u64int a0, u64int b64, u64int b0 );
#endif
#endif

#ifndef softfloat_le128
/*----------------------------------------------------------------------------
| Returns true if the 128-bit unsigned integer formed by concatenating 'a64'
| and 'a0' is less than or equal to the 128-bit unsigned integer formed by
| concatenating 'b64' and 'b0'.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
bool softfloat_le128( u64int a64, u64int a0, u64int b64, u64int b0 )
    { return (a64 < b64) || ((a64 == b64) && (a0 <= b0)); }
#else
bool softfloat_le128( u64int a64, u64int a0, u64int b64, u64int b0 );
#endif
#endif

#ifndef softfloat_lt128
/*----------------------------------------------------------------------------
| Returns true if the 128-bit unsigned integer formed by concatenating 'a64'
| and 'a0' is less than the 128-bit unsigned integer formed by concatenating
| 'b64' and 'b0'.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
bool softfloat_lt128( u64int a64, u64int a0, u64int b64, u64int b0 )
    { return (a64 < b64) || ((a64 == b64) && (a0 < b0)); }
#else
bool softfloat_lt128( u64int a64, u64int a0, u64int b64, u64int b0 );
#endif
#endif

#ifndef softfloat_shortShiftLeft128
/*----------------------------------------------------------------------------
| Shifts the 128 bits formed by concatenating 'a64' and 'a0' left by the
| number of bits given in 'dist', which must be in the range 1 to 63.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
struct uint128
 softfloat_shortShiftLeft128( u64int a64, u64int a0, u8int dist )
{
    struct uint128 z;
    z.v64 = a64<<dist | a0>>(-dist & 63);
    z.v0 = a0<<dist;
    return z;
}
#else
struct uint128
 softfloat_shortShiftLeft128( u64int a64, u64int a0, u8int dist );
#endif
#endif

#ifndef softfloat_shortShiftRight128
/*----------------------------------------------------------------------------
| Shifts the 128 bits formed by concatenating 'a64' and 'a0' right by the
| number of bits given in 'dist', which must be in the range 1 to 63.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
struct uint128
 softfloat_shortShiftRight128( u64int a64, u64int a0, u8int dist )
{
    struct uint128 z;
    z.v64 = a64>>dist;
    z.v0 = a64<<(-dist & 63) | a0>>dist;
    return z;
}
#else
struct uint128
 softfloat_shortShiftRight128( u64int a64, u64int a0, u8int dist );
#endif
#endif

#ifndef softfloat_shortShiftRightJam64Extra
/*----------------------------------------------------------------------------
| This function is the same as 'softfloat_shiftRightJam64Extra' (below),
| except that 'dist' must be in the range 1 to 63.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
struct uint64_extra
 softfloat_shortShiftRightJam64Extra(
     u64int a, u64int extra, u8int dist )
{
    struct uint64_extra z;
    z.v = a>>dist;
    z.extra = a<<(-dist & 63) | (extra != 0);
    return z;
}
#else
struct uint64_extra
 softfloat_shortShiftRightJam64Extra(
     u64int a, u64int extra, u8int dist );
#endif
#endif

#ifndef softfloat_shortShiftRightJam128
/*----------------------------------------------------------------------------
| Shifts the 128 bits formed by concatenating 'a64' and 'a0' right by the
| number of bits given in 'dist', which must be in the range 1 to 63.  If any
| nonzero bits are shifted off, they are "jammed" into the least-significant
| bit of the shifted value by setting the least-significant bit to 1.  This
| shifted-and-jammed value is returned.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (3 <= INLINE_LEVEL)
INLINE
struct uint128
 softfloat_shortShiftRightJam128(
     u64int a64, u64int a0, u8int dist )
{
    u8int negDist = -dist;
    struct uint128 z;
    z.v64 = a64>>dist;
    z.v0 =
        a64<<(negDist & 63) | a0>>dist
            | ((u64int) (a0<<(negDist & 63)) != 0);
    return z;
}
#else
struct uint128
 softfloat_shortShiftRightJam128(
     u64int a64, u64int a0, u8int dist );
#endif
#endif

#ifndef softfloat_shortShiftRightJam128Extra
/*----------------------------------------------------------------------------
| This function is the same as 'softfloat_shiftRightJam128Extra' (below),
| except that 'dist' must be in the range 1 to 63.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (3 <= INLINE_LEVEL)
INLINE
struct uint128_extra
 softfloat_shortShiftRightJam128Extra(
     u64int a64, u64int a0, u64int extra, u8int dist )
{
    u8int negDist = -dist;
    struct uint128_extra z;
    z.v.v64 = a64>>dist;
    z.v.v0 = a64<<(negDist & 63) | a0>>dist;
    z.extra = a0<<(negDist & 63) | (extra != 0);
    return z;
}
#else
struct uint128_extra
 softfloat_shortShiftRightJam128Extra(
     u64int a64, u64int a0, u64int extra, u8int dist );
#endif
#endif

#ifndef softfloat_shiftRightJam64Extra
/*----------------------------------------------------------------------------
| Shifts the 128 bits formed by concatenating 'a' and 'extra' right by 64
| _plus_ the number of bits given in 'dist', which must not be zero.  This
| shifted value is at most 64 nonzero bits and is returned in the 'v' field
| of the 'struct uint64_extra' result.  The 64-bit 'extra' field of the result
| contains a value formed as follows from the bits that were shifted off:  The
| _last_ bit shifted off is the most-significant bit of the 'extra' field, and
| the other 63 bits of the 'extra' field are all zero if and only if _all_but_
| _the_last_ bits shifted off were all zero.
|   (This function makes more sense if 'a' and 'extra' are considered to form
| an unsigned fixed-point number with binary point between 'a' and 'extra'.
| This fixed-point value is shifted right by the number of bits given in
| 'dist', and the integer part of this shifted value is returned in the 'v'
| field of the result.  The fractional part of the shifted value is modified
| as described above and returned in the 'extra' field of the result.)
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (4 <= INLINE_LEVEL)
INLINE
struct uint64_extra
 softfloat_shiftRightJam64Extra(
     u64int a, u64int extra, u32int dist )
{
    struct uint64_extra z;
    if ( dist < 64 ) {
        z.v = a>>dist;
        z.extra = a<<(-dist & 63);
    } else {
        z.v = 0;
        z.extra = (dist == 64) ? a : (a != 0);
    }
    z.extra |= (extra != 0);
    return z;
}
#else
struct uint64_extra
 softfloat_shiftRightJam64Extra(
     u64int a, u64int extra, u32int dist );
#endif
#endif

#ifndef softfloat_shiftRightJam128
/*----------------------------------------------------------------------------
| Shifts the 128 bits formed by concatenating 'a64' and 'a0' right by the
| number of bits given in 'dist', which must not be zero.  If any nonzero bits
| are shifted off, they are "jammed" into the least-significant bit of the
| shifted value by setting the least-significant bit to 1.  This shifted-and-
| jammed value is returned.
|   The value of 'dist' can be arbitrarily large.  In particular, if 'dist' is
| greater than 128, the result will be either 0 or 1, depending on whether the
| original 128 bits are all zeros.
*----------------------------------------------------------------------------*/
struct uint128
 softfloat_shiftRightJam128( u64int a64, u64int a0, u32int dist );
#endif

#ifndef softfloat_shiftRightJam128Extra
/*----------------------------------------------------------------------------
| Shifts the 192 bits formed by concatenating 'a64', 'a0', and 'extra' right
| by 64 _plus_ the number of bits given in 'dist', which must not be zero.
| This shifted value is at most 128 nonzero bits and is returned in the 'v'
| field of the 'struct uint128_extra' result.  The 64-bit 'extra' field of the
| result contains a value formed as follows from the bits that were shifted
| off:  The _last_ bit shifted off is the most-significant bit of the 'extra'
| field, and the other 63 bits of the 'extra' field are all zero if and only
| if _all_but_the_last_ bits shifted off were all zero.
|   (This function makes more sense if 'a64', 'a0', and 'extra' are considered
| to form an unsigned fixed-point number with binary point between 'a0' and
| 'extra'.  This fixed-point value is shifted right by the number of bits
| given in 'dist', and the integer part of this shifted value is returned
| in the 'v' field of the result.  The fractional part of the shifted value
| is modified as described above and returned in the 'extra' field of the
| result.)
*----------------------------------------------------------------------------*/
struct uint128_extra
 softfloat_shiftRightJam128Extra(
     u64int a64, u64int a0, u64int extra, u32int dist );
#endif

#ifndef softfloat_shiftRightJam256M
/*----------------------------------------------------------------------------
| Shifts the 256-bit unsigned integer pointed to by 'aPtr' right by the number
| of bits given in 'dist', which must not be zero.  If any nonzero bits are
| shifted off, they are "jammed" into the least-significant bit of the shifted
| value by setting the least-significant bit to 1.  This shifted-and-jammed
| value is stored at the location pointed to by 'zPtr'.  Each of 'aPtr' and
| 'zPtr' points to an array of four 64-bit elements that concatenate in the
| platform's normal endian order to form a 256-bit integer.
|   The value of 'dist' can be arbitrarily large.  In particular, if 'dist'
| is greater than 256, the stored result will be either 0 or 1, depending on
| whether the original 256 bits are all zeros.
*----------------------------------------------------------------------------*/
void
 softfloat_shiftRightJam256M(
     const u64int *aPtr, u32int dist, u64int *zPtr );
#endif

#ifndef softfloat_add128
/*----------------------------------------------------------------------------
| Returns the sum of the 128-bit integer formed by concatenating 'a64' and
| 'a0' and the 128-bit integer formed by concatenating 'b64' and 'b0'.  The
| addition is modulo 2^128, so any carry out is lost.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
struct uint128
 softfloat_add128( u64int a64, u64int a0, u64int b64, u64int b0 )
{
    struct uint128 z;
    z.v0 = a0 + b0;
    z.v64 = a64 + b64 + (z.v0 < a0);
    return z;
}
#else
struct uint128
 softfloat_add128( u64int a64, u64int a0, u64int b64, u64int b0 );
#endif
#endif

#ifndef softfloat_add256M
/*----------------------------------------------------------------------------
| Adds the two 256-bit integers pointed to by 'aPtr' and 'bPtr'.  The addition
| is modulo 2^256, so any carry out is lost.  The sum is stored at the
| location pointed to by 'zPtr'.  Each of 'aPtr', 'bPtr', and 'zPtr' points to
| an array of four 64-bit elements that concatenate in the platform's normal
| endian order to form a 256-bit integer.
*----------------------------------------------------------------------------*/
void
 softfloat_add256M(
     const u64int *aPtr, const u64int *bPtr, u64int *zPtr );
#endif

#ifndef softfloat_sub128
/*----------------------------------------------------------------------------
| Returns the difference of the 128-bit integer formed by concatenating 'a64'
| and 'a0' and the 128-bit integer formed by concatenating 'b64' and 'b0'.
| The subtraction is modulo 2^128, so any borrow out (carry out) is lost.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
struct uint128
 softfloat_sub128( u64int a64, u64int a0, u64int b64, u64int b0 )
{
    struct uint128 z;
    z.v0 = a0 - b0;
    z.v64 = a64 - b64;
    z.v64 -= (a0 < b0);
    return z;
}
#else
struct uint128
 softfloat_sub128( u64int a64, u64int a0, u64int b64, u64int b0 );
#endif
#endif

#ifndef softfloat_sub256M
/*----------------------------------------------------------------------------
| Subtracts the 256-bit integer pointed to by 'bPtr' from the 256-bit integer
| pointed to by 'aPtr'.  The addition is modulo 2^256, so any borrow out
| (carry out) is lost.  The difference is stored at the location pointed to
| by 'zPtr'.  Each of 'aPtr', 'bPtr', and 'zPtr' points to an array of four
| 64-bit elements that concatenate in the platform's normal endian order to
| form a 256-bit integer.
*----------------------------------------------------------------------------*/
void
 softfloat_sub256M(
     const u64int *aPtr, const u64int *bPtr, u64int *zPtr );
#endif

#ifndef softfloat_mul64ByShifted32To128
/*----------------------------------------------------------------------------
| Returns the 128-bit product of 'a', 'b', and 2^32.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (3 <= INLINE_LEVEL)
INLINE struct uint128 softfloat_mul64ByShifted32To128( u64int a, u32int b )
{
    u64int mid;
    struct uint128 z;
    mid = (u64int) (u32int) a * b;
    z.v0 = mid<<32;
    z.v64 = (u64int) (u32int) (a>>32) * b + (mid>>32);
    return z;
}
#else
struct uint128 softfloat_mul64ByShifted32To128( u64int a, u32int b );
#endif
#endif

#ifndef softfloat_mul64To128
/*----------------------------------------------------------------------------
| Returns the 128-bit product of 'a' and 'b'.
*----------------------------------------------------------------------------*/
struct uint128 softfloat_mul64To128( u64int a, u64int b );
#endif

#ifndef softfloat_mul128By32
/*----------------------------------------------------------------------------
| Returns the product of the 128-bit integer formed by concatenating 'a64' and
| 'a0', multiplied by 'b'.  The multiplication is modulo 2^128; any overflow
| bits are discarded.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (4 <= INLINE_LEVEL)
INLINE
struct uint128 softfloat_mul128By32( u64int a64, u64int a0, u32int b )
{
    struct uint128 z;
    u64int mid;
    u32int carry;
    z.v0 = a0 * b;
    mid = (u64int) (u32int) (a0>>32) * b;
    carry = (u32int) ((u32int) (z.v0>>32) - (u32int) mid);
    z.v64 = a64 * b + (u32int) ((mid + carry)>>32);
    return z;
}
#else
struct uint128 softfloat_mul128By32( u64int a64, u64int a0, u32int b );
#endif
#endif

#ifndef softfloat_mul128To256M
/*----------------------------------------------------------------------------
| Multiplies the 128-bit unsigned integer formed by concatenating 'a64' and
| 'a0' by the 128-bit unsigned integer formed by concatenating 'b64' and
| 'b0'.  The 256-bit product is stored at the location pointed to by 'zPtr'.
| Argument 'zPtr' points to an array of four 64-bit elements that concatenate
| in the platform's normal endian order to form a 256-bit integer.
*----------------------------------------------------------------------------*/
void
 softfloat_mul128To256M(
     u64int a64, u64int a0, u64int b64, u64int b0, u64int *zPtr );
#endif

#else

/*----------------------------------------------------------------------------
| The following functions are needed only when 'SOFTFLOAT_FAST_INT64' is not
| defined.
*----------------------------------------------------------------------------*/

#ifndef softfloat_compare96M
/*----------------------------------------------------------------------------
| Compares the two 96-bit unsigned integers pointed to by 'aPtr' and 'bPtr'.
| Returns -1 if the first integer (A) is less than the second (B); returns 0
| if the two integers are equal; and returns +1 if the first integer (A)
| is greater than the second (B).  (The result is thus the signum of A - B.)
| Each of 'aPtr' and 'bPtr' points to an array of three 32-bit elements that
| concatenate in the platform's normal endian order to form a 96-bit integer.
*----------------------------------------------------------------------------*/
s8int softfloat_compare96M( const u32int *aPtr, const u32int *bPtr );
#endif

#ifndef softfloat_compare128M
/*----------------------------------------------------------------------------
| Compares the two 128-bit unsigned integers pointed to by 'aPtr' and 'bPtr'.
| Returns -1 if the first integer (A) is less than the second (B); returns 0
| if the two integers are equal; and returns +1 if the first integer (A)
| is greater than the second (B).  (The result is thus the signum of A - B.)
| Each of 'aPtr' and 'bPtr' points to an array of four 32-bit elements that
| concatenate in the platform's normal endian order to form a 128-bit integer.
*----------------------------------------------------------------------------*/
s8int
 softfloat_compare128M( const u32int *aPtr, const u32int *bPtr );
#endif

#ifndef softfloat_shortShiftLeft64To96M
/*----------------------------------------------------------------------------
| Extends 'a' to 96 bits and shifts the value left by the number of bits given
| in 'dist', which must be in the range 1 to 31.  The result is stored at the
| location pointed to by 'zPtr'.  Argument 'zPtr' points to an array of three
| 32-bit elements that concatenate in the platform's normal endian order to
| form a 96-bit integer.
*----------------------------------------------------------------------------*/
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
INLINE
void
 softfloat_shortShiftLeft64To96M(
     u64int a, u8int dist, u32int *zPtr )
{
    zPtr[indexWord( 3, 0 )] = (u32int) a<<dist;
    a >>= 32 - dist;
    zPtr[indexWord( 3, 2 )] = a>>32;
    zPtr[indexWord( 3, 1 )] = a;
}
#else
void
 softfloat_shortShiftLeft64To96M(
     u64int a, u8int dist, u32int *zPtr );
#endif
#endif

#ifndef softfloat_shortShiftLeftM
/*----------------------------------------------------------------------------
| Shifts the N-bit unsigned integer pointed to by 'aPtr' left by the number
| of bits given in 'dist', where N = 'size_words' * 32.  The value of 'dist'
| must be in the range 1 to 31.  Any nonzero bits shifted off are lost.  The
| shifted N-bit result is stored at the location pointed to by 'zPtr'.  Each
| of 'aPtr' and 'zPtr' points to a 'size_words'-long array of 32-bit elements
| that concatenate in the platform's normal endian order to form an N-bit
| integer.
*----------------------------------------------------------------------------*/
void
 softfloat_shortShiftLeftM(
     u8int size_words,
     const u32int *aPtr,
     u8int dist,
     u32int *zPtr
 );
#endif

#ifndef softfloat_shortShiftLeft96M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shortShiftLeftM' with
| 'size_words' = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_shortShiftLeft96M( aPtr, dist, zPtr ) softfloat_shortShiftLeftM( 3, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shortShiftLeft128M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shortShiftLeftM' with
| 'size_words' = 4 (N = 128).
*----------------------------------------------------------------------------*/
#define softfloat_shortShiftLeft128M( aPtr, dist, zPtr ) softfloat_shortShiftLeftM( 4, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shortShiftLeft160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shortShiftLeftM' with
| 'size_words' = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_shortShiftLeft160M( aPtr, dist, zPtr ) softfloat_shortShiftLeftM( 5, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shiftLeftM
/*----------------------------------------------------------------------------
| Shifts the N-bit unsigned integer pointed to by 'aPtr' left by the number
| of bits given in 'dist', where N = 'size_words' * 32.  The value of 'dist'
| must not be zero.  Any nonzero bits shifted off are lost.  The shifted
| N-bit result is stored at the location pointed to by 'zPtr'.  Each of 'aPtr'
| and 'zPtr' points to a 'size_words'-long array of 32-bit elements that
| concatenate in the platform's normal endian order to form an N-bit integer.
|   The value of 'dist' can be arbitrarily large.  In particular, if 'dist' is
| greater than N, the stored result will be 0.
*----------------------------------------------------------------------------*/
void
 softfloat_shiftLeftM(
     u8int size_words,
     const u32int *aPtr,
     u32int dist,
     u32int *zPtr
 );
#endif

#ifndef softfloat_shiftLeft96M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shiftLeftM' with
| 'size_words' = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_shiftLeft96M( aPtr, dist, zPtr ) softfloat_shiftLeftM( 3, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shiftLeft128M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shiftLeftM' with
| 'size_words' = 4 (N = 128).
*----------------------------------------------------------------------------*/
#define softfloat_shiftLeft128M( aPtr, dist, zPtr ) softfloat_shiftLeftM( 4, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shiftLeft160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shiftLeftM' with
| 'size_words' = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_shiftLeft160M( aPtr, dist, zPtr ) softfloat_shiftLeftM( 5, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shortShiftRightM
/*----------------------------------------------------------------------------
| Shifts the N-bit unsigned integer pointed to by 'aPtr' right by the number
| of bits given in 'dist', where N = 'size_words' * 32.  The value of 'dist'
| must be in the range 1 to 31.  Any nonzero bits shifted off are lost.  The
| shifted N-bit result is stored at the location pointed to by 'zPtr'.  Each
| of 'aPtr' and 'zPtr' points to a 'size_words'-long array of 32-bit elements
| that concatenate in the platform's normal endian order to form an N-bit
| integer.
*----------------------------------------------------------------------------*/
void
 softfloat_shortShiftRightM(
     u8int size_words,
     const u32int *aPtr,
     u8int dist,
     u32int *zPtr
 );
#endif

#ifndef softfloat_shortShiftRight128M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shortShiftRightM' with
| 'size_words' = 4 (N = 128).
*----------------------------------------------------------------------------*/
#define softfloat_shortShiftRight128M( aPtr, dist, zPtr ) softfloat_shortShiftRightM( 4, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shortShiftRight160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shortShiftRightM' with
| 'size_words' = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_shortShiftRight160M( aPtr, dist, zPtr ) softfloat_shortShiftRightM( 5, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shortShiftRightJamM
/*----------------------------------------------------------------------------
| Shifts the N-bit unsigned integer pointed to by 'aPtr' right by the number
| of bits given in 'dist', where N = 'size_words' * 32.  The value of 'dist'
| must be in the range 1 to 31.  If any nonzero bits are shifted off, they are
| "jammed" into the least-significant bit of the shifted value by setting the
| least-significant bit to 1.  This shifted-and-jammed N-bit result is stored
| at the location pointed to by 'zPtr'.  Each of 'aPtr' and 'zPtr' points
| to a 'size_words'-long array of 32-bit elements that concatenate in the
| platform's normal endian order to form an N-bit integer.
*----------------------------------------------------------------------------*/
void
 softfloat_shortShiftRightJamM(
     u8int, const u32int *, u8int, u32int * );
#endif

#ifndef softfloat_shortShiftRightJam160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shortShiftRightJamM' with
| 'size_words' = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_shortShiftRightJam160M( aPtr, dist, zPtr ) softfloat_shortShiftRightJamM( 5, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shiftRightM
/*----------------------------------------------------------------------------
| Shifts the N-bit unsigned integer pointed to by 'aPtr' right by the number
| of bits given in 'dist', where N = 'size_words' * 32.  The value of 'dist'
| must not be zero.  Any nonzero bits shifted off are lost.  The shifted
| N-bit result is stored at the location pointed to by 'zPtr'.  Each of 'aPtr'
| and 'zPtr' points to a 'size_words'-long array of 32-bit elements that
| concatenate in the platform's normal endian order to form an N-bit integer.
|   The value of 'dist' can be arbitrarily large.  In particular, if 'dist' is
| greater than N, the stored result will be 0.
*----------------------------------------------------------------------------*/
void
 softfloat_shiftRightM(
     u8int size_words,
     const u32int *aPtr,
     u32int dist,
     u32int *zPtr
 );
#endif

#ifndef softfloat_shiftRight96M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shiftRightM' with
| 'size_words' = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_shiftRight96M( aPtr, dist, zPtr ) softfloat_shiftRightM( 3, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shiftRightJamM
/*----------------------------------------------------------------------------
| Shifts the N-bit unsigned integer pointed to by 'aPtr' right by the number
| of bits given in 'dist', where N = 'size_words' * 32.  The value of 'dist'
| must not be zero.  If any nonzero bits are shifted off, they are "jammed"
| into the least-significant bit of the shifted value by setting the least-
| significant bit to 1.  This shifted-and-jammed N-bit result is stored
| at the location pointed to by 'zPtr'.  Each of 'aPtr' and 'zPtr' points
| to a 'size_words'-long array of 32-bit elements that concatenate in the
| platform's normal endian order to form an N-bit integer.
|   The value of 'dist' can be arbitrarily large.  In particular, if 'dist'
| is greater than N, the stored result will be either 0 or 1, depending on
| whether the original N bits are all zeros.
*----------------------------------------------------------------------------*/
void
 softfloat_shiftRightJamM(
     u8int size_words,
     const u32int *aPtr,
     u32int dist,
     u32int *zPtr
 );
#endif

#ifndef softfloat_shiftRightJam128M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shiftRightJamM' with
| 'size_words' = 4 (N = 128).
*----------------------------------------------------------------------------*/
#define softfloat_shiftRightJam128M( aPtr, dist, zPtr ) softfloat_shiftRightJamM( 4, aPtr, dist, zPtr )
#endif

#ifndef softfloat_shiftRightJam160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shiftRightJamM' with
| 'size_words' = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_shiftRightJam160M( aPtr, dist, zPtr ) softfloat_shiftRightJamM( 5, aPtr, dist, zPtr )
#endif

#ifndef softfloat_addM
/*----------------------------------------------------------------------------
| Adds the two N-bit integers pointed to by 'aPtr' and 'bPtr', where N =
| 'size_words' * 32.  The addition is modulo 2^N, so any carry out is lost.
| The N-bit sum is stored at the location pointed to by 'zPtr'.  Each of
| 'aPtr', 'bPtr', and 'zPtr' points to a 'size_words'-long array of 32-bit
| elements that concatenate in the platform's normal endian order to form an
| N-bit integer.
*----------------------------------------------------------------------------*/
void
 softfloat_addM(
     u8int size_words,
     const u32int *aPtr,
     const u32int *bPtr,
     u32int *zPtr
 );
#endif

#ifndef softfloat_add128M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_addM' with 'size_words'
| = 4 (N = 128).
*----------------------------------------------------------------------------*/
#define softfloat_add128M( aPtr, bPtr, zPtr ) softfloat_addM( 4, aPtr, bPtr, zPtr )
#endif

#ifndef softfloat_add160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_addM' with 'size_words'
| = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_add160M( aPtr, bPtr, zPtr ) softfloat_addM( 5, aPtr, bPtr, zPtr )
#endif

#ifndef softfloat_addCarryM
/*----------------------------------------------------------------------------
| Adds the two N-bit unsigned integers pointed to by 'aPtr' and 'bPtr', where
| N = 'size_words' * 32, plus 'carry', which must be either 0 or 1.  The N-bit
| sum (modulo 2^N) is stored at the location pointed to by 'zPtr', and any
| carry out is returned as the result.  Each of 'aPtr', 'bPtr', and 'zPtr'
| points to a 'size_words'-long array of 32-bit elements that concatenate in
| the platform's normal endian order to form an N-bit integer.
*----------------------------------------------------------------------------*/
u8int
 softfloat_addCarryM(
     u8int size_words,
     const u32int *aPtr,
     const u32int *bPtr,
     u8int carry,
     u32int *zPtr
 );
#endif

#ifndef softfloat_addComplCarryM
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_addCarryM', except that
| the value of the unsigned integer pointed to by 'bPtr' is bit-wise completed
| before the addition.
*----------------------------------------------------------------------------*/
u8int
 softfloat_addComplCarryM(
     u8int size_words,
     const u32int *aPtr,
     const u32int *bPtr,
     u8int carry,
     u32int *zPtr
 );
#endif

#ifndef softfloat_addComplCarry96M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_addComplCarryM' with
| 'size_words' = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_addComplCarry96M( aPtr, bPtr, carry, zPtr ) softfloat_addComplCarryM( 3, aPtr, bPtr, carry, zPtr )
#endif

#ifndef softfloat_negXM
/*----------------------------------------------------------------------------
| Replaces the N-bit unsigned integer pointed to by 'zPtr' by the
| 2s-complement of itself, where N = 'size_words' * 32.  Argument 'zPtr'
| points to a 'size_words'-long array of 32-bit elements that concatenate in
| the platform's normal endian order to form an N-bit integer.
*----------------------------------------------------------------------------*/
void softfloat_negXM( u8int size_words, u32int *zPtr );
#endif

#ifndef softfloat_negX96M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_negXM' with 'size_words'
| = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_negX96M( zPtr ) softfloat_negXM( 3, zPtr )
#endif

#ifndef softfloat_negX128M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_negXM' with 'size_words'
| = 4 (N = 128).
*----------------------------------------------------------------------------*/
#define softfloat_negX128M( zPtr ) softfloat_negXM( 4, zPtr )
#endif

#ifndef softfloat_negX160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_negXM' with 'size_words'
| = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_negX160M( zPtr ) softfloat_negXM( 5, zPtr )
#endif

#ifndef softfloat_negX256M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_negXM' with 'size_words'
| = 8 (N = 256).
*----------------------------------------------------------------------------*/
#define softfloat_negX256M( zPtr ) softfloat_negXM( 8, zPtr )
#endif

#ifndef softfloat_sub1XM
/*----------------------------------------------------------------------------
| Subtracts 1 from the N-bit integer pointed to by 'zPtr', where N =
| 'size_words' * 32.  The subtraction is modulo 2^N, so any borrow out (carry
| out) is lost.  Argument 'zPtr' points to a 'size_words'-long array of 32-bit
| elements that concatenate in the platform's normal endian order to form an
| N-bit integer.
*----------------------------------------------------------------------------*/
void softfloat_sub1XM( u8int size_words, u32int *zPtr );
#endif

#ifndef softfloat_sub1X96M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_sub1XM' with 'size_words'
| = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_sub1X96M( zPtr ) softfloat_sub1XM( 3, zPtr )
#endif

#ifndef softfloat_sub1X160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_sub1XM' with 'size_words'
| = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_sub1X160M( zPtr ) softfloat_sub1XM( 5, zPtr )
#endif

#ifndef softfloat_subM
/*----------------------------------------------------------------------------
| Subtracts the two N-bit integers pointed to by 'aPtr' and 'bPtr', where N =
| 'size_words' * 32.  The subtraction is modulo 2^N, so any borrow out (carry
| out) is lost.  The N-bit difference is stored at the location pointed to by
| 'zPtr'.  Each of 'aPtr', 'bPtr', and 'zPtr' points to a 'size_words'-long
| array of 32-bit elements that concatenate in the platform's normal endian
| order to form an N-bit integer.
*----------------------------------------------------------------------------*/
void
 softfloat_subM(
     u8int size_words,
     const u32int *aPtr,
     const u32int *bPtr,
     u32int *zPtr
 );
#endif

#ifndef softfloat_sub96M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_subM' with 'size_words'
| = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_sub96M( aPtr, bPtr, zPtr ) softfloat_subM( 3, aPtr, bPtr, zPtr )
#endif

#ifndef softfloat_sub128M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_subM' with 'size_words'
| = 4 (N = 128).
*----------------------------------------------------------------------------*/
#define softfloat_sub128M( aPtr, bPtr, zPtr ) softfloat_subM( 4, aPtr, bPtr, zPtr )
#endif

#ifndef softfloat_sub160M
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_subM' with 'size_words'
| = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_sub160M( aPtr, bPtr, zPtr ) softfloat_subM( 5, aPtr, bPtr, zPtr )
#endif

#ifndef softfloat_mul64To128M
/*----------------------------------------------------------------------------
| Multiplies 'a' and 'b' and stores the 128-bit product at the location
| pointed to by 'zPtr'.  Argument 'zPtr' points to an array of four 32-bit
| elements that concatenate in the platform's normal endian order to form a
| 128-bit integer.
*----------------------------------------------------------------------------*/
void softfloat_mul64To128M( u64int a, u64int b, u32int *zPtr );
#endif

#ifndef softfloat_mul128MTo256M
/*----------------------------------------------------------------------------
| Multiplies the two 128-bit unsigned integers pointed to by 'aPtr' and
| 'bPtr', and stores the 256-bit product at the location pointed to by 'zPtr'.
| Each of 'aPtr' and 'bPtr' points to an array of four 32-bit elements that
| concatenate in the platform's normal endian order to form a 128-bit integer.
| Argument 'zPtr' points to an array of eight 32-bit elements that concatenate
| to form a 256-bit integer.
*----------------------------------------------------------------------------*/
void
 softfloat_mul128MTo256M(
     const u32int *aPtr, const u32int *bPtr, u32int *zPtr );
#endif

#ifndef softfloat_remStepMBy32
/*----------------------------------------------------------------------------
| Performs a "remainder reduction step" as follows:  Arguments 'remPtr' and
| 'bPtr' both point to N-bit unsigned integers, where N = 'size_words' * 32.
| Defining R and B as the values of those integers, the expression (R<<'dist')
| - B * q is computed modulo 2^N, and the N-bit result is stored at the
| location pointed to by 'zPtr'.  Each of 'remPtr', 'bPtr', and 'zPtr' points
| to a 'size_words'-long array of 32-bit elements that concatenate in the
| platform's normal endian order to form an N-bit integer.
*----------------------------------------------------------------------------*/
void
 softfloat_remStepMBy32(
     u8int size_words,
     const u32int *remPtr,
     u8int dist,
     const u32int *bPtr,
     u32int q,
     u32int *zPtr
 );
#endif

#ifndef softfloat_remStep96MBy32
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_remStepMBy32' with
| 'size_words' = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_remStep96MBy32( remPtr, dist, bPtr, q, zPtr ) softfloat_remStepMBy32( 3, remPtr, dist, bPtr, q, zPtr )
#endif

#ifndef softfloat_remStep128MBy32
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_remStepMBy32' with
| 'size_words' = 4 (N = 128).
*----------------------------------------------------------------------------*/
#define softfloat_remStep128MBy32( remPtr, dist, bPtr, q, zPtr ) softfloat_remStepMBy32( 4, remPtr, dist, bPtr, q, zPtr )
#endif

#ifndef softfloat_remStep160MBy32
/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_remStepMBy32' with
| 'size_words' = 5 (N = 160).
*----------------------------------------------------------------------------*/
#define softfloat_remStep160MBy32( remPtr, dist, bPtr, q, zPtr ) softfloat_remStepMBy32( 5, remPtr, dist, bPtr, q, zPtr )
#endif

#endif

/*----------------------------------------------------------------------------
| Types used to pass 16-bit, 32-bit, 64-bit, and 128-bit floating-point
| arguments and results to/from functions.  These types must be exactly
| 16 bits, 32 bits, 64 bits, and 128 bits in size, respectively.  Where a
| platform has "native" support for IEEE-Standard floating-point formats,
| the types below may, if desired, be defined as aliases for the native types
| (typically 'float' and 'double', and possibly 'long double').
*----------------------------------------------------------------------------*/

typedef struct { u16int v; } float16_t;
typedef struct { u16int v; } bfloat16_t;
typedef struct { u32int v; } float32_t;
typedef struct { u64int v; } float64_t;
typedef struct { u64int v[2]; } float128_t;

/*----------------------------------------------------------------------------
| The format of an 80-bit extended floating-point number in memory.  This
| structure must contain a 16-bit field named 'signExp' and a 64-bit field
| named 'signif'.
*----------------------------------------------------------------------------*/
#ifdef LITTLEENDIAN
struct extFloat80M { u64int signif; u16int signExp; };
#else
struct extFloat80M { u16int signExp; u64int signif; };
#endif

/*----------------------------------------------------------------------------
| The type used to pass 80-bit extended floating-point arguments and
| results to/from functions.  This type must have size identical to
| 'struct extFloat80M'.  Type 'extFloat80_t' can be defined as an alias for
| 'struct extFloat80M'.  Alternatively, if a platform has "native" support
| for IEEE-Standard 80-bit extended floating-point, it may be possible,
| if desired, to define 'extFloat80_t' as an alias for the native type
| (presumably either 'long double' or a nonstandard compiler-intrinsic type).
| In that case, the 'signif' and 'signExp' fields of 'struct extFloat80M'
| must align exactly with the locations in memory of the sign, exponent, and
| significand of the native type.
*----------------------------------------------------------------------------*/
typedef struct extFloat80M extFloat80_t;

union ui16_f16 { u16int ui; float16_t f; };
union ui16_bf16 { u16int ui; bfloat16_t f; };
union ui32_f32 { u32int ui; float32_t f; };
union ui64_f64 { u64int ui; float64_t f; };

#ifdef SOFTFLOAT_FAST_INT64
union extF80M_extF80 { struct extFloat80M fM; extFloat80_t f; };
union ui128_f128 { struct uint128 ui; float128_t f; };
#endif

enum {
    softfloat_mulAdd_subC    = 1,
    softfloat_mulAdd_subProd = 2
};

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
u32int softfloat_roundToUI32( bool, u64int, u8int, bool );

#ifdef SOFTFLOAT_FAST_INT64
u64int
 softfloat_roundToUI64(
     bool, u64int, u64int, u8int, bool );
#else
u64int softfloat_roundMToUI64( bool, u32int *, u8int, bool );
#endif

s32int softfloat_roundToI32( bool, u64int, u8int, bool );

#ifdef SOFTFLOAT_FAST_INT64
s64int
 softfloat_roundToI64(
     bool, u64int, u64int, u8int, bool );
#else
s64int softfloat_roundMToI64( bool, u32int *, u8int, bool );
#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#define signF16UI( a ) ((bool) ((u16int) (a)>>15))
#define expF16UI( a ) ((s8int) ((a)>>10) & 0x1F)
#define fracF16UI( a ) ((a) & 0x03FF)
#define packToF16UI( sign, exp, sig ) (((u16int) (sign)<<15) + ((u16int) (exp)<<10) + (sig))

#define isNaNF16UI( a ) (((~(a) & 0x7C00) == 0) && ((a) & 0x03FF))

struct exp8_sig16 { s8int exp; u16int sig; };
struct exp8_sig16 softfloat_normSubnormalF16Sig( u16int );

float16_t softfloat_roundPackToF16( bool, s16int, u16int );
float16_t softfloat_normRoundPackToF16( bool, s16int, u16int );

float16_t softfloat_addMagsF16( u16int, u16int );
float16_t softfloat_subMagsF16( u16int, u16int );
float16_t
 softfloat_mulAddF16(
     u16int, u16int, u16int, u8int );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#define signBF16UI( a ) ((bool) ((u16int) (a)>>15))
#define expBF16UI( a ) ((s16int) ((a)>>7) & 0xFF)
#define fracBF16UI( a ) ((a) & 0x07F)
#define packToBF16UI( sign, exp, sig ) (((u16int) (sign)<<15) + ((u16int) (exp)<<7) + (sig))

#define isNaNBF16UI( a ) (((~(a) & 0x7FC0) == 0) && ((a) & 0x07F))

bfloat16_t softfloat_roundPackToBF16( bool, s16int, u16int );
struct exp8_sig16 softfloat_normSubnormalBF16Sig( u16int );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#define signF32UI( a ) ((bool) ((u32int) (a)>>31))
#define expF32UI( a ) ((s16int) ((a)>>23) & 0xFF)
#define fracF32UI( a ) ((a) & 0x007FFFFF)
#define packToF32UI( sign, exp, sig ) (((u32int) (sign)<<31) + ((u32int) (exp)<<23) + (sig))

#define isNaNF32UI( a ) (((~(a) & 0x7F800000) == 0) && ((a) & 0x007FFFFF))

struct exp16_sig32 { s16int exp; u32int sig; };
struct exp16_sig32 softfloat_normSubnormalF32Sig( u32int );

float32_t softfloat_roundPackToF32( bool, s16int, u32int );
float32_t softfloat_normRoundPackToF32( bool, s16int, u32int );

float32_t softfloat_addMagsF32( u32int, u32int );
float32_t softfloat_subMagsF32( u32int, u32int );
float32_t
 softfloat_mulAddF32(
     u32int, u32int, u32int, u8int );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#define signF64UI( a ) ((bool) ((u64int) (a)>>63))
#define expF64UI( a ) ((s16int) ((a)>>52) & 0x7FF)
#define fracF64UI( a ) ((a) & UINT64_C( 0x000FFFFFFFFFFFFF ))
#define packToF64UI( sign, exp, sig ) ((u64int) (((u64int) (sign)<<63) + ((u64int) (exp)<<52) + (sig)))

#define isNaNF64UI( a ) (((~(a) & UINT64_C( 0x7FF0000000000000 )) == 0) && ((a) & UINT64_C( 0x000FFFFFFFFFFFFF )))

struct exp16_sig64 { s16int exp; u64int sig; };
struct exp16_sig64 softfloat_normSubnormalF64Sig( u64int );

float64_t softfloat_roundPackToF64( bool, s16int, u64int );
float64_t softfloat_normRoundPackToF64( bool, s16int, u64int );

float64_t softfloat_addMagsF64( u64int, u64int, bool );
float64_t softfloat_subMagsF64( u64int, u64int, bool );
float64_t
 softfloat_mulAddF64(
     u64int, u64int, u64int, u8int );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#define signExtF80UI64( a64 ) ((bool) ((u16int) (a64)>>15))
#define expExtF80UI64( a64 ) ((a64) & 0x7FFF)
#define packToExtF80UI64( sign, exp ) ((u16int) (sign)<<15 | (exp))

#define isNaNExtF80UI( a64, a0 ) ((((a64) & 0x7FFF) == 0x7FFF) && ((a0) & UINT64_C( 0x7FFFFFFFFFFFFFFF )))

#ifdef SOFTFLOAT_FAST_INT64

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

struct exp32_sig64 { s32int exp; u64int sig; };
struct exp32_sig64 softfloat_normSubnormalExtF80Sig( u64int );

extFloat80_t
 softfloat_roundPackToExtF80(
     bool, s32int, u64int, u64int, u8int );
extFloat80_t
 softfloat_normRoundPackToExtF80(
     bool, s32int, u64int, u64int, u8int );

extFloat80_t
 softfloat_addMagsExtF80(
     u16int, u64int, u16int, u64int, bool );
extFloat80_t
 softfloat_subMagsExtF80(
     u16int, u64int, u16int, u64int, bool );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#define signF128UI64( a64 ) ((bool) ((u64int) (a64)>>63))
#define expF128UI64( a64 ) ((s32int) ((a64)>>48) & 0x7FFF)
#define fracF128UI64( a64 ) ((a64) & UINT64_C( 0x0000FFFFFFFFFFFF ))
#define packToF128UI64( sign, exp, sig64 ) (((u64int) (sign)<<63) + ((u64int) (exp)<<48) + (sig64))

#define isNaNF128UI( a64, a0 ) (((~(a64) & UINT64_C( 0x7FFF000000000000 )) == 0) && (a0 || ((a64) & UINT64_C( 0x0000FFFFFFFFFFFF ))))

struct exp32_sig128 { s32int exp; struct uint128 sig; };
struct exp32_sig128
 softfloat_normSubnormalF128Sig( u64int, u64int );

float128_t
 softfloat_roundPackToF128(
     bool, s32int, u64int, u64int, u64int );
float128_t
 softfloat_normRoundPackToF128(
     bool, s32int, u64int, u64int );

float128_t
 softfloat_addMagsF128(
     u64int, u64int, u64int, u64int, bool );
float128_t
 softfloat_subMagsF128(
     u64int, u64int, u64int, u64int, bool );
float128_t
 softfloat_mulAddF128(
     u64int,
     u64int,
     u64int,
     u64int,
     u64int,
     u64int,
     u8int
 );

#else

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

bool
 softfloat_tryPropagateNaNExtF80M(
     const struct extFloat80M *,
     const struct extFloat80M *,
     struct extFloat80M *
 );
void softfloat_invalidExtF80M( struct extFloat80M * );

int softfloat_normExtF80SigM( u64int * );

void
 softfloat_roundPackMToExtF80M(
     bool, s32int, u32int *, u8int, struct extFloat80M * );
void
 softfloat_normRoundPackMToExtF80M(
     bool, s32int, u32int *, u8int, struct extFloat80M * );

void
 softfloat_addExtF80M(
     const struct extFloat80M *,
     const struct extFloat80M *,
     struct extFloat80M *,
     bool
 );

int
 softfloat_compareNonnormExtF80M(
     const struct extFloat80M *, const struct extFloat80M * );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#define signF128UI96( a96 ) ((bool) ((u32int) (a96)>>31))
#define expF128UI96( a96 ) ((s32int) ((a96)>>16) & 0x7FFF)
#define fracF128UI96( a96 ) ((a96) & 0x0000FFFF)
#define packToF128UI96( sign, exp, sig96 ) (((u32int) (sign)<<31) + ((u32int) (exp)<<16) + (sig96))

bool softfloat_isNaNF128M( const u32int * );

bool
 softfloat_tryPropagateNaNF128M(
     const u32int *, const u32int *, u32int * );
void softfloat_invalidF128M( u32int * );

int softfloat_shiftNormSigF128M( const u32int *, u8int, u32int * );

void softfloat_roundPackMToF128M( bool, s32int, u32int *, u32int * );
void softfloat_normRoundPackMToF128M( bool, s32int, u32int *, u32int * );

void
 softfloat_addF128M( const u32int *, const u32int *, u32int *, bool );
void
 softfloat_mulAddF128M(
     const u32int *,
     const u32int *,
     const u32int *,
     u32int *,
     u8int
 );

#endif

#ifndef THREAD_LOCAL
#define THREAD_LOCAL
#endif

/*----------------------------------------------------------------------------
| Software floating-point underflow tininess-detection mode.
*----------------------------------------------------------------------------*/
extern THREAD_LOCAL u8int softfloat_detectTininess;
enum {
    softfloat_tininess_beforeRounding = 0,
    softfloat_tininess_afterRounding  = 1
};

/*----------------------------------------------------------------------------
| Software floating-point rounding mode.  (Mode "odd" is supported only if
| SoftFloat is compiled with macro 'SOFTFLOAT_ROUND_ODD' defined.)
*----------------------------------------------------------------------------*/
extern THREAD_LOCAL u8int softfloat_roundingMode;
enum {
    softfloat_round_near_even   = 0,
    softfloat_round_minMag      = 1,
    softfloat_round_min         = 2,
    softfloat_round_max         = 3,
    softfloat_round_near_maxMag = 4,
    softfloat_round_odd         = 6
};

/*----------------------------------------------------------------------------
| Software floating-point exception flags.
*----------------------------------------------------------------------------*/
extern THREAD_LOCAL u8int softfloat_exceptionFlags;
typedef enum {
    softfloat_flag_inexact   =  1,
    softfloat_flag_underflow =  2,
    softfloat_flag_overflow  =  4,
    softfloat_flag_infinite  =  8,
    softfloat_flag_invalid   = 16
} exceptionFlag_t;

/*----------------------------------------------------------------------------
| Routine to raise any or all of the software floating-point exception flags.
*----------------------------------------------------------------------------*/
void softfloat_raiseFlags( u8int );

/*----------------------------------------------------------------------------
| Integer-to-floating-point conversion routines.
*----------------------------------------------------------------------------*/
float16_t ui32_to_f16( u32int );
float32_t ui32_to_f32( u32int );
float64_t ui32_to_f64( u32int );
#ifdef SOFTFLOAT_FAST_INT64
extFloat80_t ui32_to_extF80( u32int );
float128_t ui32_to_f128( u32int );
#endif
void ui32_to_extF80M( u32int, extFloat80_t * );
void ui32_to_f128M( u32int, float128_t * );
float16_t ui64_to_f16( u64int );
float32_t ui64_to_f32( u64int );
float64_t ui64_to_f64( u64int );
#ifdef SOFTFLOAT_FAST_INT64
extFloat80_t ui64_to_extF80( u64int );
float128_t ui64_to_f128( u64int );
#endif
void ui64_to_extF80M( u64int, extFloat80_t * );
void ui64_to_f128M( u64int, float128_t * );
float16_t i32_to_f16( s32int );
float32_t i32_to_f32( s32int );
float64_t i32_to_f64( s32int );
#ifdef SOFTFLOAT_FAST_INT64
extFloat80_t i32_to_extF80( s32int );
float128_t i32_to_f128( s32int );
#endif
void i32_to_extF80M( s32int, extFloat80_t * );
void i32_to_f128M( s32int, float128_t * );
float16_t i64_to_f16( s64int );
float32_t i64_to_f32( s64int );
float64_t i64_to_f64( s64int );
#ifdef SOFTFLOAT_FAST_INT64
extFloat80_t i64_to_extF80( s64int );
float128_t i64_to_f128( s64int );
#endif
void i64_to_extF80M( s64int, extFloat80_t * );
void i64_to_f128M( s64int, float128_t * );

/*----------------------------------------------------------------------------
| 16-bit (half-precision) floating-point operations.
*----------------------------------------------------------------------------*/
u32int f16_to_ui32( float16_t, u8int, bool );
u64int f16_to_ui64( float16_t, u8int, bool );
s32int f16_to_i32( float16_t, u8int, bool );
s64int f16_to_i64( float16_t, u8int, bool );
u32int f16_to_ui32_r_minMag( float16_t, bool );
u64int f16_to_ui64_r_minMag( float16_t, bool );
s32int f16_to_i32_r_minMag( float16_t, bool );
s64int f16_to_i64_r_minMag( float16_t, bool );
float32_t f16_to_f32( float16_t );
float64_t f16_to_f64( float16_t );
#ifdef SOFTFLOAT_FAST_INT64
extFloat80_t f16_to_extF80( float16_t );
float128_t f16_to_f128( float16_t );
#endif
void f16_to_extF80M( float16_t, extFloat80_t * );
void f16_to_f128M( float16_t, float128_t * );
float16_t f16_roundToInt( float16_t, u8int, bool );
float16_t f16_add( float16_t, float16_t );
float16_t f16_sub( float16_t, float16_t );
float16_t f16_mul( float16_t, float16_t );
float16_t f16_mulAdd( float16_t, float16_t, float16_t );
float16_t f16_div( float16_t, float16_t );
float16_t f16_rem( float16_t, float16_t );
float16_t f16_sqrt( float16_t );
bool f16_eq( float16_t, float16_t );
bool f16_le( float16_t, float16_t );
bool f16_lt( float16_t, float16_t );
bool f16_eq_signaling( float16_t, float16_t );
bool f16_le_quiet( float16_t, float16_t );
bool f16_lt_quiet( float16_t, float16_t );
bool f16_isSignalingNaN( float16_t );

/*----------------------------------------------------------------------------
| 16-bit (brain float 16) floating-point operations.
*----------------------------------------------------------------------------*/
float32_t bf16_to_f32( bfloat16_t );
bfloat16_t f32_to_bf16( float32_t );
bool bf16_isSignalingNaN( bfloat16_t );

/*----------------------------------------------------------------------------
| 32-bit (single-precision) floating-point operations.
*----------------------------------------------------------------------------*/
u32int f32_to_ui32( float32_t, u8int, bool );
u64int f32_to_ui64( float32_t, u8int, bool );
s32int f32_to_i32( float32_t, u8int, bool );
s64int f32_to_i64( float32_t, u8int, bool );
u32int f32_to_ui32_r_minMag( float32_t, bool );
u64int f32_to_ui64_r_minMag( float32_t, bool );
s32int f32_to_i32_r_minMag( float32_t, bool );
s64int f32_to_i64_r_minMag( float32_t, bool );
float16_t f32_to_f16( float32_t );
float64_t f32_to_f64( float32_t );
#ifdef SOFTFLOAT_FAST_INT64
extFloat80_t f32_to_extF80( float32_t );
float128_t f32_to_f128( float32_t );
#endif
void f32_to_extF80M( float32_t, extFloat80_t * );
void f32_to_f128M( float32_t, float128_t * );
float32_t f32_roundToInt( float32_t, u8int, bool );
float32_t f32_add( float32_t, float32_t );
float32_t f32_sub( float32_t, float32_t );
float32_t f32_mul( float32_t, float32_t );
float32_t f32_mulAdd( float32_t, float32_t, float32_t );
float32_t f32_div( float32_t, float32_t );
float32_t f32_rem( float32_t, float32_t );
float32_t f32_sqrt( float32_t );
bool f32_eq( float32_t, float32_t );
bool f32_le( float32_t, float32_t );
bool f32_lt( float32_t, float32_t );
bool f32_eq_signaling( float32_t, float32_t );
bool f32_le_quiet( float32_t, float32_t );
bool f32_lt_quiet( float32_t, float32_t );
bool f32_isSignalingNaN( float32_t );

/*----------------------------------------------------------------------------
| 64-bit (double-precision) floating-point operations.
*----------------------------------------------------------------------------*/
u32int f64_to_ui32( float64_t, u8int, bool );
u64int f64_to_ui64( float64_t, u8int, bool );
s32int f64_to_i32( float64_t, u8int, bool );
s64int f64_to_i64( float64_t, u8int, bool );
u32int f64_to_ui32_r_minMag( float64_t, bool );
u64int f64_to_ui64_r_minMag( float64_t, bool );
s32int f64_to_i32_r_minMag( float64_t, bool );
s64int f64_to_i64_r_minMag( float64_t, bool );
float16_t f64_to_f16( float64_t );
float32_t f64_to_f32( float64_t );
#ifdef SOFTFLOAT_FAST_INT64
extFloat80_t f64_to_extF80( float64_t );
float128_t f64_to_f128( float64_t );
#endif
void f64_to_extF80M( float64_t, extFloat80_t * );
void f64_to_f128M( float64_t, float128_t * );
float64_t f64_roundToInt( float64_t, u8int, bool );
float64_t f64_add( float64_t, float64_t );
float64_t f64_sub( float64_t, float64_t );
float64_t f64_mul( float64_t, float64_t );
float64_t f64_mulAdd( float64_t, float64_t, float64_t );
float64_t f64_div( float64_t, float64_t );
float64_t f64_rem( float64_t, float64_t );
float64_t f64_sqrt( float64_t );
bool f64_eq( float64_t, float64_t );
bool f64_le( float64_t, float64_t );
bool f64_lt( float64_t, float64_t );
bool f64_eq_signaling( float64_t, float64_t );
bool f64_le_quiet( float64_t, float64_t );
bool f64_lt_quiet( float64_t, float64_t );
bool f64_isSignalingNaN( float64_t );

/*----------------------------------------------------------------------------
| Rounding precision for 80-bit extended double-precision floating-point.
| Valid values are 32, 64, and 80.
*----------------------------------------------------------------------------*/
extern THREAD_LOCAL u8int extF80_roundingPrecision;

/*----------------------------------------------------------------------------
| 80-bit extended double-precision floating-point operations.
*----------------------------------------------------------------------------*/
#ifdef SOFTFLOAT_FAST_INT64
u32int extF80_to_ui32( extFloat80_t, u8int, bool );
u64int extF80_to_ui64( extFloat80_t, u8int, bool );
s32int extF80_to_i32( extFloat80_t, u8int, bool );
s64int extF80_to_i64( extFloat80_t, u8int, bool );
u32int extF80_to_ui32_r_minMag( extFloat80_t, bool );
u64int extF80_to_ui64_r_minMag( extFloat80_t, bool );
s32int extF80_to_i32_r_minMag( extFloat80_t, bool );
s64int extF80_to_i64_r_minMag( extFloat80_t, bool );
float16_t extF80_to_f16( extFloat80_t );
float32_t extF80_to_f32( extFloat80_t );
float64_t extF80_to_f64( extFloat80_t );
float128_t extF80_to_f128( extFloat80_t );
extFloat80_t extF80_roundToInt( extFloat80_t, u8int, bool );
extFloat80_t extF80_add( extFloat80_t, extFloat80_t );
extFloat80_t extF80_sub( extFloat80_t, extFloat80_t );
extFloat80_t extF80_mul( extFloat80_t, extFloat80_t );
extFloat80_t extF80_div( extFloat80_t, extFloat80_t );
extFloat80_t extF80_rem( extFloat80_t, extFloat80_t );
extFloat80_t extF80_sqrt( extFloat80_t );
bool extF80_eq( extFloat80_t, extFloat80_t );
bool extF80_le( extFloat80_t, extFloat80_t );
bool extF80_lt( extFloat80_t, extFloat80_t );
bool extF80_eq_signaling( extFloat80_t, extFloat80_t );
bool extF80_le_quiet( extFloat80_t, extFloat80_t );
bool extF80_lt_quiet( extFloat80_t, extFloat80_t );
bool extF80_isSignalingNaN( extFloat80_t );
#endif
u32int extF80M_to_ui32( const extFloat80_t *, u8int, bool );
u64int extF80M_to_ui64( const extFloat80_t *, u8int, bool );
s32int extF80M_to_i32( const extFloat80_t *, u8int, bool );
s64int extF80M_to_i64( const extFloat80_t *, u8int, bool );
u32int extF80M_to_ui32_r_minMag( const extFloat80_t *, bool );
u64int extF80M_to_ui64_r_minMag( const extFloat80_t *, bool );
s32int extF80M_to_i32_r_minMag( const extFloat80_t *, bool );
s64int extF80M_to_i64_r_minMag( const extFloat80_t *, bool );
float16_t extF80M_to_f16( const extFloat80_t * );
float32_t extF80M_to_f32( const extFloat80_t * );
float64_t extF80M_to_f64( const extFloat80_t * );
void extF80M_to_f128M( const extFloat80_t *, float128_t * );
void
 extF80M_roundToInt(
     const extFloat80_t *, u8int, bool, extFloat80_t * );
void extF80M_add( const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void extF80M_sub( const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void extF80M_mul( const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void extF80M_div( const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void extF80M_rem( const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void extF80M_sqrt( const extFloat80_t *, extFloat80_t * );
bool extF80M_eq( const extFloat80_t *, const extFloat80_t * );
bool extF80M_le( const extFloat80_t *, const extFloat80_t * );
bool extF80M_lt( const extFloat80_t *, const extFloat80_t * );
bool extF80M_eq_signaling( const extFloat80_t *, const extFloat80_t * );
bool extF80M_le_quiet( const extFloat80_t *, const extFloat80_t * );
bool extF80M_lt_quiet( const extFloat80_t *, const extFloat80_t * );
bool extF80M_isSignalingNaN( const extFloat80_t * );

/*----------------------------------------------------------------------------
| 128-bit (quadruple-precision) floating-point operations.
*----------------------------------------------------------------------------*/
#ifdef SOFTFLOAT_FAST_INT64
u32int f128_to_ui32( float128_t, u8int, bool );
u64int f128_to_ui64( float128_t, u8int, bool );
s32int f128_to_i32( float128_t, u8int, bool );
s64int f128_to_i64( float128_t, u8int, bool );
u32int f128_to_ui32_r_minMag( float128_t, bool );
u64int f128_to_ui64_r_minMag( float128_t, bool );
s32int f128_to_i32_r_minMag( float128_t, bool );
s64int f128_to_i64_r_minMag( float128_t, bool );
float16_t f128_to_f16( float128_t );
float32_t f128_to_f32( float128_t );
float64_t f128_to_f64( float128_t );
extFloat80_t f128_to_extF80( float128_t );
float128_t f128_roundToInt( float128_t, u8int, bool );
float128_t f128_add( float128_t, float128_t );
float128_t f128_sub( float128_t, float128_t );
float128_t f128_mul( float128_t, float128_t );
float128_t f128_mulAdd( float128_t, float128_t, float128_t );
float128_t f128_div( float128_t, float128_t );
float128_t f128_rem( float128_t, float128_t );
float128_t f128_sqrt( float128_t );
bool f128_eq( float128_t, float128_t );
bool f128_le( float128_t, float128_t );
bool f128_lt( float128_t, float128_t );
bool f128_eq_signaling( float128_t, float128_t );
bool f128_le_quiet( float128_t, float128_t );
bool f128_lt_quiet( float128_t, float128_t );
bool f128_isSignalingNaN( float128_t );
#endif
u32int f128M_to_ui32( const float128_t *, u8int, bool );
u64int f128M_to_ui64( const float128_t *, u8int, bool );
s32int f128M_to_i32( const float128_t *, u8int, bool );
s64int f128M_to_i64( const float128_t *, u8int, bool );
u32int f128M_to_ui32_r_minMag( const float128_t *, bool );
u64int f128M_to_ui64_r_minMag( const float128_t *, bool );
s32int f128M_to_i32_r_minMag( const float128_t *, bool );
s64int f128M_to_i64_r_minMag( const float128_t *, bool );
float16_t f128M_to_f16( const float128_t * );
float32_t f128M_to_f32( const float128_t * );
float64_t f128M_to_f64( const float128_t * );
void f128M_to_extF80M( const float128_t *, extFloat80_t * );
void f128M_roundToInt( const float128_t *, u8int, bool, float128_t * );
void f128M_add( const float128_t *, const float128_t *, float128_t * );
void f128M_sub( const float128_t *, const float128_t *, float128_t * );
void f128M_mul( const float128_t *, const float128_t *, float128_t * );
void
 f128M_mulAdd(
     const float128_t *, const float128_t *, const float128_t *, float128_t *
 );
void f128M_div( const float128_t *, const float128_t *, float128_t * );
void f128M_rem( const float128_t *, const float128_t *, float128_t * );
void f128M_sqrt( const float128_t *, float128_t * );
bool f128M_eq( const float128_t *, const float128_t * );
bool f128M_le( const float128_t *, const float128_t * );
bool f128M_lt( const float128_t *, const float128_t * );
bool f128M_eq_signaling( const float128_t *, const float128_t * );
bool f128M_le_quiet( const float128_t *, const float128_t * );
bool f128M_lt_quiet( const float128_t *, const float128_t * );
bool f128M_isSignalingNaN( const float128_t * );

/*----------------------------------------------------------------------------
| Default value for 'softfloat_detectTininess'.
*----------------------------------------------------------------------------*/
#define init_detectTininess softfloat_tininess_afterRounding

/*----------------------------------------------------------------------------
| The values to return on conversions to 32-bit integer formats that raise an
| invalid exception.
*----------------------------------------------------------------------------*/
#define ui32_fromPosOverflow 0xFFFFFFFF
#define ui32_fromNegOverflow 0xFFFFFFFF
#define ui32_fromNaN         0xFFFFFFFF
#define i32_fromPosOverflow  (-0x7FFFFFFF - 1)
#define i32_fromNegOverflow  (-0x7FFFFFFF - 1)
#define i32_fromNaN          (-0x7FFFFFFF - 1)

/*----------------------------------------------------------------------------
| The values to return on conversions to 64-bit integer formats that raise an
| invalid exception.
*----------------------------------------------------------------------------*/
#define ui64_fromPosOverflow UINT64_C( 0xFFFFFFFFFFFFFFFF )
#define ui64_fromNegOverflow UINT64_C( 0xFFFFFFFFFFFFFFFF )
#define ui64_fromNaN         UINT64_C( 0xFFFFFFFFFFFFFFFF )
#define i64_fromPosOverflow  (-INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
#define i64_fromNegOverflow  (-INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
#define i64_fromNaN          (-INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)

/*----------------------------------------------------------------------------
| "Common NaN" structure, used to transfer NaN representations from one format
| to another.
*----------------------------------------------------------------------------*/
struct commonNaN {
    bool sign;
#ifdef LITTLEENDIAN
    u64int v0, v64;
#else
    u64int v64, v0;
#endif
};

/*----------------------------------------------------------------------------
| The bit pattern for a default generated 16-bit floating-point NaN.
*----------------------------------------------------------------------------*/
#define defaultNaNF16UI 0xFE00

/*----------------------------------------------------------------------------
| Returns true when 16-bit unsigned integer 'uiA' has the bit pattern of a
| 16-bit floating-point signaling NaN.
| Note:  This macro evaluates its argument more than once.
*----------------------------------------------------------------------------*/
#define softfloat_isSigNaNF16UI( uiA ) ((((uiA) & 0x7E00) == 0x7C00) && ((uiA) & 0x01FF))

/*----------------------------------------------------------------------------
| Assuming 'uiA' has the bit pattern of a 16-bit floating-point NaN, converts
| this NaN to the common NaN form, and stores the resulting common NaN at the
| location pointed to by 'zPtr'.  If the NaN is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/
void softfloat_f16UIToCommonNaN( u16int uiA, struct commonNaN *zPtr );

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by 'aPtr' into a 16-bit floating-point
| NaN, and returns the bit pattern of this value as an unsigned integer.
*----------------------------------------------------------------------------*/
u16int softfloat_commonNaNToF16UI( const struct commonNaN *aPtr );

/*----------------------------------------------------------------------------
| Interpreting 'uiA' and 'uiB' as the bit patterns of two 16-bit floating-
| point values, at least one of which is a NaN, returns the bit pattern of
| the combined NaN result.  If either 'uiA' or 'uiB' has the pattern of a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/
u16int
 softfloat_propagateNaNF16UI( u16int uiA, u16int uiB );

/*----------------------------------------------------------------------------
| The bit pattern for a default generated 32-bit floating-point NaN.
*----------------------------------------------------------------------------*/
#define defaultNaNF32UI 0xFFC00000

/*----------------------------------------------------------------------------
| Returns true when 32-bit unsigned integer 'uiA' has the bit pattern of a
| 32-bit floating-point signaling NaN.
| Note:  This macro evaluates its argument more than once.
*----------------------------------------------------------------------------*/
#define softfloat_isSigNaNF32UI( uiA ) ((((uiA) & 0x7FC00000) == 0x7F800000) && ((uiA) & 0x003FFFFF))

/*----------------------------------------------------------------------------
| Assuming 'uiA' has the bit pattern of a 32-bit floating-point NaN, converts
| this NaN to the common NaN form, and stores the resulting common NaN at the
| location pointed to by 'zPtr'.  If the NaN is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/
void softfloat_f32UIToCommonNaN( u32int uiA, struct commonNaN *zPtr );

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by 'aPtr' into a 32-bit floating-point
| NaN, and returns the bit pattern of this value as an unsigned integer.
*----------------------------------------------------------------------------*/
u32int softfloat_commonNaNToF32UI( const struct commonNaN *aPtr );

/*----------------------------------------------------------------------------
| Interpreting 'uiA' and 'uiB' as the bit patterns of two 32-bit floating-
| point values, at least one of which is a NaN, returns the bit pattern of
| the combined NaN result.  If either 'uiA' or 'uiB' has the pattern of a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/
u32int
 softfloat_propagateNaNF32UI( u32int uiA, u32int uiB );

/*----------------------------------------------------------------------------
| The bit pattern for a default generated 64-bit floating-point NaN.
*----------------------------------------------------------------------------*/
#define defaultNaNF64UI UINT64_C( 0xFFF8000000000000 )

/*----------------------------------------------------------------------------
| Returns true when 64-bit unsigned integer 'uiA' has the bit pattern of a
| 64-bit floating-point signaling NaN.
| Note:  This macro evaluates its argument more than once.
*----------------------------------------------------------------------------*/
#define softfloat_isSigNaNF64UI( uiA ) ((((uiA) & UINT64_C( 0x7FF8000000000000 )) == UINT64_C( 0x7FF0000000000000 )) && ((uiA) & UINT64_C( 0x0007FFFFFFFFFFFF )))

/*----------------------------------------------------------------------------
| Assuming 'uiA' has the bit pattern of a 64-bit floating-point NaN, converts
| this NaN to the common NaN form, and stores the resulting common NaN at the
| location pointed to by 'zPtr'.  If the NaN is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/
void softfloat_f64UIToCommonNaN( u64int uiA, struct commonNaN *zPtr );

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by 'aPtr' into a 64-bit floating-point
| NaN, and returns the bit pattern of this value as an unsigned integer.
*----------------------------------------------------------------------------*/
u64int softfloat_commonNaNToF64UI( const struct commonNaN *aPtr );

/*----------------------------------------------------------------------------
| Interpreting 'uiA' and 'uiB' as the bit patterns of two 64-bit floating-
| point values, at least one of which is a NaN, returns the bit pattern of
| the combined NaN result.  If either 'uiA' or 'uiB' has the pattern of a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/
u64int
 softfloat_propagateNaNF64UI( u64int uiA, u64int uiB );

/*----------------------------------------------------------------------------
| The bit pattern for a default generated 80-bit extended floating-point NaN.
*----------------------------------------------------------------------------*/
#define defaultNaNExtF80UI64 0xFFFF
#define defaultNaNExtF80UI0  UINT64_C( 0xC000000000000000 )

/*----------------------------------------------------------------------------
| Returns true when the 80-bit unsigned integer formed from concatenating
| 16-bit 'uiA64' and 64-bit 'uiA0' has the bit pattern of an 80-bit extended
| floating-point signaling NaN.
| Note:  This macro evaluates its arguments more than once.
*----------------------------------------------------------------------------*/
#define softfloat_isSigNaNExtF80UI( uiA64, uiA0 ) ((((uiA64) & 0x7FFF) == 0x7FFF) && ! ((uiA0) & UINT64_C( 0x4000000000000000 )) && ((uiA0) & UINT64_C( 0x3FFFFFFFFFFFFFFF )))

#ifdef SOFTFLOAT_FAST_INT64

/*----------------------------------------------------------------------------
| The following functions are needed only when 'SOFTFLOAT_FAST_INT64' is
| defined.
*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
| Assuming the unsigned integer formed from concatenating 'uiA64' and 'uiA0'
| has the bit pattern of an 80-bit extended floating-point NaN, converts
| this NaN to the common NaN form, and stores the resulting common NaN at the
| location pointed to by 'zPtr'.  If the NaN is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/
void
 softfloat_extF80UIToCommonNaN(
     u16int uiA64, u64int uiA0, struct commonNaN *zPtr );

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by 'aPtr' into an 80-bit extended
| floating-point NaN, and returns the bit pattern of this value as an unsigned
| integer.
*----------------------------------------------------------------------------*/
struct uint128 softfloat_commonNaNToExtF80UI( const struct commonNaN *aPtr );

/*----------------------------------------------------------------------------
| Interpreting the unsigned integer formed from concatenating 'uiA64' and
| 'uiA0' as an 80-bit extended floating-point value, and likewise interpreting
| the unsigned integer formed from concatenating 'uiB64' and 'uiB0' as another
| 80-bit extended floating-point value, and assuming at least on of these
| floating-point values is a NaN, returns the bit pattern of the combined NaN
| result.  If either original floating-point value is a signaling NaN, the
| invalid exception is raised.
*----------------------------------------------------------------------------*/
struct uint128
 softfloat_propagateNaNExtF80UI(
     u16int uiA64,
     u64int uiA0,
     u16int uiB64,
     u64int uiB0
 );

/*----------------------------------------------------------------------------
| The bit pattern for a default generated 128-bit floating-point NaN.
*----------------------------------------------------------------------------*/
#define defaultNaNF128UI64 UINT64_C( 0xFFFF800000000000 )
#define defaultNaNF128UI0  UINT64_C( 0 )

/*----------------------------------------------------------------------------
| Returns true when the 128-bit unsigned integer formed from concatenating
| 64-bit 'uiA64' and 64-bit 'uiA0' has the bit pattern of a 128-bit floating-
| point signaling NaN.
| Note:  This macro evaluates its arguments more than once.
*----------------------------------------------------------------------------*/
#define softfloat_isSigNaNF128UI( uiA64, uiA0 ) ((((uiA64) & UINT64_C( 0x7FFF800000000000 )) == UINT64_C( 0x7FFF000000000000 )) && ((uiA0) || ((uiA64) & UINT64_C( 0x00007FFFFFFFFFFF ))))

/*----------------------------------------------------------------------------
| Assuming the unsigned integer formed from concatenating 'uiA64' and 'uiA0'
| has the bit pattern of a 128-bit floating-point NaN, converts this NaN to
| the common NaN form, and stores the resulting common NaN at the location
| pointed to by 'zPtr'.  If the NaN is a signaling NaN, the invalid exception
| is raised.
*----------------------------------------------------------------------------*/
void
 softfloat_f128UIToCommonNaN(
     u64int uiA64, u64int uiA0, struct commonNaN *zPtr );

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by 'aPtr' into a 128-bit floating-point
| NaN, and returns the bit pattern of this value as an unsigned integer.
*----------------------------------------------------------------------------*/
struct uint128 softfloat_commonNaNToF128UI( const struct commonNaN * );

/*----------------------------------------------------------------------------
| Interpreting the unsigned integer formed from concatenating 'uiA64' and
| 'uiA0' as a 128-bit floating-point value, and likewise interpreting the
| unsigned integer formed from concatenating 'uiB64' and 'uiB0' as another
| 128-bit floating-point value, and assuming at least on of these floating-
| point values is a NaN, returns the bit pattern of the combined NaN result.
| If either original floating-point value is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/
struct uint128
 softfloat_propagateNaNF128UI(
     u64int uiA64,
     u64int uiA0,
     u64int uiB64,
     u64int uiB0
 );

#else

/*----------------------------------------------------------------------------
| The following functions are needed only when 'SOFTFLOAT_FAST_INT64' is not
| defined.
*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
| Assuming the 80-bit extended floating-point value pointed to by 'aSPtr' is
| a NaN, converts this NaN to the common NaN form, and stores the resulting
| common NaN at the location pointed to by 'zPtr'.  If the NaN is a signaling
| NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/
void
 softfloat_extF80MToCommonNaN(
     const struct extFloat80M *aSPtr, struct commonNaN *zPtr );

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by 'aPtr' into an 80-bit extended
| floating-point NaN, and stores this NaN at the location pointed to by
| 'zSPtr'.
*----------------------------------------------------------------------------*/
void
 softfloat_commonNaNToExtF80M(
     const struct commonNaN *aPtr, struct extFloat80M *zSPtr );

/*----------------------------------------------------------------------------
| Assuming at least one of the two 80-bit extended floating-point values
| pointed to by 'aSPtr' and 'bSPtr' is a NaN, stores the combined NaN result
| at the location pointed to by 'zSPtr'.  If either original floating-point
| value is a signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/
void
 softfloat_propagateNaNExtF80M(
     const struct extFloat80M *aSPtr,
     const struct extFloat80M *bSPtr,
     struct extFloat80M *zSPtr
 );

/*----------------------------------------------------------------------------
| The bit pattern for a default generated 128-bit floating-point NaN.
*----------------------------------------------------------------------------*/
#define defaultNaNF128UI96 0xFFFF8000
#define defaultNaNF128UI64 0
#define defaultNaNF128UI32 0
#define defaultNaNF128UI0  0

/*----------------------------------------------------------------------------
| Assuming the 128-bit floating-point value pointed to by 'aWPtr' is a NaN,
| converts this NaN to the common NaN form, and stores the resulting common
| NaN at the location pointed to by 'zPtr'.  If the NaN is a signaling NaN,
| the invalid exception is raised.  Argument 'aWPtr' points to an array of
| four 32-bit elements that concatenate in the platform's normal endian order
| to form a 128-bit floating-point value.
*----------------------------------------------------------------------------*/
void
 softfloat_f128MToCommonNaN( const u32int *aWPtr, struct commonNaN *zPtr );

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by 'aPtr' into a 128-bit floating-point
| NaN, and stores this NaN at the location pointed to by 'zWPtr'.  Argument
| 'zWPtr' points to an array of four 32-bit elements that concatenate in the
| platform's normal endian order to form a 128-bit floating-point value.
*----------------------------------------------------------------------------*/
void
 softfloat_commonNaNToF128M( const struct commonNaN *aPtr, u32int *zWPtr );

/*----------------------------------------------------------------------------
| Assuming at least one of the two 128-bit floating-point values pointed to by
| 'aWPtr' and 'bWPtr' is a NaN, stores the combined NaN result at the location
| pointed to by 'zWPtr'.  If either original floating-point value is a
| signaling NaN, the invalid exception is raised.  Each of 'aWPtr', 'bWPtr',
| and 'zWPtr' points to an array of four 32-bit elements that concatenate in
| the platform's normal endian order to form a 128-bit floating-point value.
*----------------------------------------------------------------------------*/
void
 softfloat_propagateNaNF128M(
     const u32int *aWPtr, const u32int *bWPtr, u32int *zWPtr );

#endif
