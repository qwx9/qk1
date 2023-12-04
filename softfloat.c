/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of
California.  All rights reserved.

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

#include "quakedef.h"
#include "softfloat.h"

#ifndef UINT64_C
#define UINT64_C(x) x##ULL
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
struct uint128 { u64int v0, v64; };
struct uint64_extra { u64int extra, v; };
#define wordIncr 1
#define indexWord( total, n ) (n)
#define indexWordHi( total ) ((total) - 1)
#define indexWordLo( total ) 0
#define indexMultiword( total, m, n ) (n)
#define indexMultiwordHi( total, n ) ((total) - (n))
#define indexMultiwordLo( total, n ) 0
#define indexMultiwordHiBut( total, n ) (n)
#define indexMultiwordLoBut( total, n ) 0
#else
struct uint128 { u64int v64, v0; };
struct uint64_extra { u64int v, extra; };
#define wordIncr -1
#define indexWord( total, n ) ((total) - 1 - (n))
#define indexWordHi( total ) 0
#define indexWordLo( total ) ((total) - 1)
#define indexMultiword( total, m, n ) ((total) - 1 - (m))
#define indexMultiwordHi( total, n ) 0
#define indexMultiwordLo( total, n ) ((total) - (n))
#define indexMultiwordHiBut( total, n ) 0
#define indexMultiwordLoBut( total, n ) (n)
#endif

/*----------------------------------------------------------------------------
| Default value for 'softfloat_detectTininess'.
*----------------------------------------------------------------------------*/
#define init_detectTininess softfloat_tininess_afterRounding

/*----------------------------------------------------------------------------
| "Common NaN" structure, used to transfer NaN representations from one format
| to another.
*----------------------------------------------------------------------------*/
struct commonNaN {
    bool sign;
#if BYTE_ORDER == LITTLE_ENDIAN
    u64int v0, v64;
#else
    u64int v64, v0;
#endif
};

union ui32_f32 { u32int ui; float32_t f; };

enum {
    softfloat_mulAdd_subC    = 1,
    softfloat_mulAdd_subProd = 2
};


/*----------------------------------------------------------------------------
| Software floating-point underflow tininess-detection mode.
*----------------------------------------------------------------------------*/
enum {
    softfloat_tininess_beforeRounding = 0,
    softfloat_tininess_afterRounding  = 1
};

/*----------------------------------------------------------------------------
| Software floating-point rounding mode.  (Mode "odd" is supported only if
| SoftFloat is compiled with macro 'SOFTFLOAT_ROUND_ODD' defined.)
*----------------------------------------------------------------------------*/
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
typedef enum {
    softfloat_flag_inexact   =  1,
    softfloat_flag_underflow =  2,
    softfloat_flag_overflow  =  4,
    softfloat_flag_infinite  =  8,
    softfloat_flag_invalid   = 16
} exceptionFlag_t;

/*----------------------------------------------------------------------------
| Returns true when 32-bit unsigned integer 'uiA' has the bit pattern of a
| 32-bit floating-point signaling NaN.
| Note:  This macro evaluates its argument more than once.
*----------------------------------------------------------------------------*/
#define softfloat_isSigNaNF32UI( uiA ) ((((uiA) & 0x7FC00000) == 0x7F800000) && ((uiA) & 0x003FFFFF))

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

#define signF32UI( a ) ((bool) ((u32int) (a)>>31))
#define expF32UI( a ) ((s16int) ((a)>>23) & 0xFF)
#define fracF32UI( a ) ((a) & 0x007FFFFF)
#define packToF32UI( sign, exp, sig ) (((u32int) (sign)<<31) + ((u32int) (exp)<<23) + (sig))

#define isNaNF32UI( a ) (((~(a) & 0x7F800000) == 0) && ((a) & 0x007FFFFF))

struct exp16_sig32 { s16int exp; u32int sig; };

static struct exp16_sig32 softfloat_normSubnormalF32Sig( u32int );
static float32_t softfloat_roundPackToF32( bool, s16int, u32int );

#define signExtF80UI64( a64 ) ((bool) ((u16int) (a64)>>15))
#define expExtF80UI64( a64 ) ((a64) & 0x7FFF)
#define packToExtF80UI64( sign, exp ) ((u16int) (sign)<<15 | (exp))

#define isNaNExtF80UI( a64, a0 ) ((((a64) & 0x7FFF) == 0x7FFF) && ((a0) & UINT64_C( 0x7FFFFFFFFFFFFFFF )))

struct exp32_sig64 { s32int exp; u64int sig; };

#define signF128UI64( a64 ) ((bool) ((u64int) (a64)>>63))
#define expF128UI64( a64 ) ((s32int) ((a64)>>48) & 0x7FFF)
#define fracF128UI64( a64 ) ((a64) & UINT64_C( 0x0000FFFFFFFFFFFF ))
#define packToF128UI64( sign, exp, sig64 ) (((u64int) (sign)<<63) + ((u64int) (exp)<<48) + (sig64))

#define isNaNF128UI( a64, a0 ) (((~(a64) & UINT64_C( 0x7FFF000000000000 )) == 0) && (a0 || ((a64) & UINT64_C( 0x0000FFFFFFFFFFFF ))))

struct exp32_sig128 { s32int exp; struct uint128 sig; };

/*----------------------------------------------------------------------------
| Routine to raise any or all of the software floating-point exception flags.
*----------------------------------------------------------------------------*/
static void softfloat_raiseFlags( u8int );

u8int softfloat_roundingMode = softfloat_round_near_even;
u8int softfloat_detectTininess = init_detectTininess;
u8int softfloat_exceptionFlags = 0;
u8int extF80_roundingPrecision = 80;

static bool
extF80M_isSignalingNaN( const extFloat80_t *aPtr )
{
    const struct extFloat80M *aSPtr;
    u64int uiA0;

    aSPtr = (const struct extFloat80M *) aPtr;
    if ( (aSPtr->signExp & 0x7FFF) != 0x7FFF ) return false;
    uiA0 = aSPtr->signif;
    return
        ! (uiA0 & UINT64_C( 0x4000000000000000 ))
            && (uiA0 & UINT64_C( 0x3FFFFFFFFFFFFFFF));

}

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by `aPtr' into an 80-bit extended
| floating-point NaN, and stores this NaN at the location pointed to by
| `zSPtr'.
*----------------------------------------------------------------------------*/
static void
softfloat_commonNaNToExtF80M(const struct commonNaN *aPtr, struct extFloat80M *zSPtr )
{

    zSPtr->signExp = packToExtF80UI64( aPtr->sign, 0x7FFF );
    zSPtr->signif = UINT64_C( 0xC000000000000000 ) | aPtr->v64>>1;

}

/*----------------------------------------------------------------------------
| Converts the common NaN pointed to by `aPtr' into a 32-bit floating-point
| NaN, and returns the bit pattern of this value as an unsigned integer.
*----------------------------------------------------------------------------*/
static u32int
softfloat_commonNaNToF32UI( const struct commonNaN *aPtr )
{

    return (u32int) aPtr->sign<<31 | 0x7FC00000 | aPtr->v64>>41;

}

/*----------------------------------------------------------------------------
| Assuming the 80-bit extended floating-point value pointed to by `aSPtr' is
| a NaN, converts this NaN to the common NaN form, and stores the resulting
| common NaN at the location pointed to by `zPtr'.  If the NaN is a signaling
| NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/
static void
softfloat_extF80MToCommonNaN(const struct extFloat80M *aSPtr, struct commonNaN *zPtr )
{

    if ( extF80M_isSignalingNaN( (const extFloat80_t *) aSPtr ) ) {
        softfloat_raiseFlags( softfloat_flag_invalid );
    }
    zPtr->sign = signExtF80UI64( aSPtr->signExp );
    zPtr->v64 = aSPtr->signif<<1;
    zPtr->v0  = 0;

}

/*----------------------------------------------------------------------------
| Assuming `uiA' has the bit pattern of a 32-bit floating-point NaN, converts
| this NaN to the common NaN form, and stores the resulting common NaN at the
| location pointed to by `zPtr'.  If the NaN is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/
static void
softfloat_f32UIToCommonNaN( u32int uiA, struct commonNaN *zPtr )
{

    if ( softfloat_isSigNaNF32UI( uiA ) ) {
        softfloat_raiseFlags( softfloat_flag_invalid );
    }
    zPtr->sign = uiA>>31;
    zPtr->v64  = (u64int) uiA<<41;
    zPtr->v0   = 0;

}

/*----------------------------------------------------------------------------
| Assuming at least one of the two 80-bit extended floating-point values
| pointed to by `aSPtr' and `bSPtr' is a NaN, stores the combined NaN result
| at the location pointed to by `zSPtr'.  If either original floating-point
| value is a signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/
static void
softfloat_propagateNaNExtF80M(
     const struct extFloat80M *aSPtr,
     const struct extFloat80M *bSPtr,
     struct extFloat80M *zSPtr
 )
{
    bool isSigNaNA;
    const struct extFloat80M *sPtr;
    bool isSigNaNB;
    u16int uiB64;
    u64int uiB0;
    u16int uiA64;
    u64int uiA0;
    u16int uiMagA64, uiMagB64;

    isSigNaNA = extF80M_isSignalingNaN( (const extFloat80_t *) aSPtr );
    sPtr = aSPtr;
    if ( ! bSPtr ) {
        if ( isSigNaNA ) softfloat_raiseFlags( softfloat_flag_invalid );
        goto copy;
    }
    isSigNaNB = extF80M_isSignalingNaN( (const extFloat80_t *) bSPtr );
    if ( isSigNaNA | isSigNaNB ) {
        softfloat_raiseFlags( softfloat_flag_invalid );
        if ( isSigNaNA ) {
            uiB64 = bSPtr->signExp;
            if ( isSigNaNB ) goto returnLargerUIMag;
            uiB0 = bSPtr->signif;
            if ( isNaNExtF80UI( uiB64, uiB0 ) ) goto copyB;
            goto copy;
        } else {
            uiA64 = aSPtr->signExp;
            uiA0 = aSPtr->signif;
            if ( isNaNExtF80UI( uiA64, uiA0 ) ) goto copy;
            goto copyB;
        }
    }
    uiB64 = bSPtr->signExp;
 returnLargerUIMag:
    uiA64 = aSPtr->signExp;
    uiMagA64 = uiA64 & 0x7FFF;
    uiMagB64 = uiB64 & 0x7FFF;
    if ( uiMagA64 < uiMagB64 ) goto copyB;
    if ( uiMagB64 < uiMagA64 ) goto copy;
    uiA0 = aSPtr->signif;
    uiB0 = bSPtr->signif;
    if ( uiA0 < uiB0 ) goto copyB;
    if ( uiB0 < uiA0 ) goto copy;
    if ( uiA64 < uiB64 ) goto copy;
 copyB:
    sPtr = bSPtr;
 copy:
    zSPtr->signExp = sPtr->signExp;
    zSPtr->signif = sPtr->signif | UINT64_C( 0xC000000000000000 );

}

/*----------------------------------------------------------------------------
| Interpreting the unsigned integer formed from concatenating 'uiA64' and
| 'uiA0' as an 80-bit extended floating-point value, and likewise interpreting
| the unsigned integer formed from concatenating 'uiB64' and 'uiB0' as another
| 80-bit extended floating-point value, and assuming at least on of these
| floating-point values is a NaN, returns the bit pattern of the combined NaN
| result.  If either original floating-point value is a signaling NaN, the
| invalid exception is raised.
*----------------------------------------------------------------------------*/
static struct uint128
 softfloat_propagateNaNExtF80UI(
     u16int uiA64,
     u64int uiA0,
     u16int uiB64,
     u64int uiB0
 )
{
    bool isSigNaNA, isSigNaNB;
    u64int uiNonsigA0, uiNonsigB0;
    u16int uiMagA64, uiMagB64;
    struct uint128 uiZ;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    isSigNaNA = softfloat_isSigNaNExtF80UI( uiA64, uiA0 );
    isSigNaNB = softfloat_isSigNaNExtF80UI( uiB64, uiB0 );
    /*------------------------------------------------------------------------
    | Make NaNs non-signaling.
    *------------------------------------------------------------------------*/
    uiNonsigA0 = uiA0 | UINT64_C( 0xC000000000000000 );
    uiNonsigB0 = uiB0 | UINT64_C( 0xC000000000000000 );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( isSigNaNA | isSigNaNB ) {
        softfloat_raiseFlags( softfloat_flag_invalid );
        if ( isSigNaNA ) {
            if ( isSigNaNB ) goto returnLargerMag;
            if ( isNaNExtF80UI( uiB64, uiB0 ) ) goto returnB;
            goto returnA;
        } else {
            if ( isNaNExtF80UI( uiA64, uiA0 ) ) goto returnA;
            goto returnB;
        }
    }
 returnLargerMag:
    uiMagA64 = uiA64 & 0x7FFF;
    uiMagB64 = uiB64 & 0x7FFF;
    if ( uiMagA64 < uiMagB64 ) goto returnB;
    if ( uiMagB64 < uiMagA64 ) goto returnA;
    if ( uiA0 < uiB0 ) goto returnB;
    if ( uiB0 < uiA0 ) goto returnA;
    if ( uiA64 < uiB64 ) goto returnA;
 returnB:
    uiZ.v64 = uiB64;
    uiZ.v0  = uiNonsigB0;
    return uiZ;
 returnA:
    uiZ.v64 = uiA64;
    uiZ.v0  = uiNonsigA0;
    return uiZ;

}

/*----------------------------------------------------------------------------
| Raises the exceptions specified by `flags'.  Floating-point traps can be
| defined here if desired.  It is currently not possible for such a trap
| to substitute a result value.  If traps are not implemented, this routine
| should be simply `softfloat_exceptionFlags |= flags;'.
*----------------------------------------------------------------------------*/
static void
softfloat_raiseFlags( u8int flags )
{
    softfloat_exceptionFlags |= flags;
}

static u64int
softfloat_shortShiftRightJam64( u64int a, u8int dist )
{
    return a>>dist | ((a & (((u64int) 1<<dist) - 1)) != 0);
}

/*----------------------------------------------------------------------------
| A constant table that translates an 8-bit unsigned integer (the array index)
| into the number of leading 0 bits before the most-significant 1 of that
| integer.  For integer zero (index 0), the corresponding table element is 8.
*----------------------------------------------------------------------------*/
static const u8int softfloat_countLeadingZeros8[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u8int
softfloat_countLeadingZeros64( u64int a )
{
    u8int count;
    u32int a32;

    count = 0;
    a32 = a>>32;
    if ( ! a32 ) {
        count = 32;
        a32 = a;
    }
    /*------------------------------------------------------------------------
    | From here, result is current count + count leading zeros of `a32'.
    *------------------------------------------------------------------------*/
    if ( a32 < 0x10000 ) {
        count += 16;
        a32 <<= 16;
    }
    if ( a32 < 0x1000000 ) {
        count += 8;
        a32 <<= 8;
    }
    count += softfloat_countLeadingZeros8[a32>>24];
    return count;

}

static int
softfloat_normExtF80SigM( u64int *sigPtr )
{
    u64int sig;
    s8int shiftDist;

    sig = *sigPtr;
    shiftDist = softfloat_countLeadingZeros64( sig );
    *sigPtr = sig<<shiftDist;
    return -shiftDist;

}

static u8int
softfloat_countLeadingZeros32( u32int a )
{
    u8int count;

    count = 0;
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

static struct exp16_sig32
softfloat_normSubnormalF32Sig( u32int sig )
{
    s8int shiftDist;
    struct exp16_sig32 z;

    shiftDist = softfloat_countLeadingZeros32( sig ) - 8;
    z.exp = 1 - shiftDist;
    z.sig = sig<<shiftDist;
    return z;

}

static u32int
softfloat_shiftRightJam32( u32int a, u16int dist )
{

    return
        (dist < 31) ? a>>dist | ((u32int) (a<<(-dist & 31)) != 0) : (a != 0);

}

static float32_t
 softfloat_roundPackToF32( bool sign, s16int exp, u32int sig )
{
    u8int roundingMode;
    bool roundNearEven;
    u8int roundIncrement, roundBits;
    bool isTiny;
    u32int uiZ;
    union ui32_f32 uZ;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    roundingMode = softfloat_roundingMode;
    roundNearEven = (roundingMode == softfloat_round_near_even);
    roundIncrement = 0x40;
    if ( ! roundNearEven && (roundingMode != softfloat_round_near_maxMag) ) {
        roundIncrement =
            (roundingMode
                 == (sign ? softfloat_round_min : softfloat_round_max))
                ? 0x7F
                : 0;
    }
    roundBits = sig & 0x7F;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( 0xFD <= (unsigned int) exp ) {
        if ( exp < 0 ) {
            /*----------------------------------------------------------------
            *----------------------------------------------------------------*/
            isTiny =
                (softfloat_detectTininess == softfloat_tininess_beforeRounding)
                    || (exp < -1) || (sig + roundIncrement < 0x80000000);
            sig = softfloat_shiftRightJam32( sig, -exp );
            exp = 0;
            roundBits = sig & 0x7F;
            if ( isTiny && roundBits ) {
                softfloat_raiseFlags( softfloat_flag_underflow );
            }
        } else if ( (0xFD < exp) || (0x80000000 <= sig + roundIncrement) ) {
            /*----------------------------------------------------------------
            *----------------------------------------------------------------*/
            softfloat_raiseFlags(
                softfloat_flag_overflow | softfloat_flag_inexact );
            uiZ = packToF32UI( sign, 0xFF, 0 ) - ! roundIncrement;
            goto uiZ;
        }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    sig = (sig + roundIncrement)>>7;
    if ( roundBits ) {
        softfloat_exceptionFlags |= softfloat_flag_inexact;
    }
    sig &= ~(u32int) (! (roundBits ^ 0x40) & roundNearEven);
    if ( ! sig ) exp = 0;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uiZ = packToF32UI( sign, exp, sig );
 uiZ:
    uZ.ui = uiZ;
    return uZ.f;

}

static struct uint64_extra
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

static struct uint128
 softfloat_shiftRightJam128( u64int a64, u64int a0, u32int dist )
{
    u8int u8NegDist;
    struct uint128 z;

    if ( dist < 64 ) {
        u8NegDist = -dist;
        z.v64 = a64>>dist;
        z.v0 =
            a64<<(u8NegDist & 63) | a0>>dist
                | ((u64int) (a0<<(u8NegDist & 63)) != 0);
    } else {
        z.v64 = 0;
        z.v0 =
            (dist < 127)
                ? a64>>(dist & 63)
                      | (((a64 & (((u64int) 1<<(dist & 63)) - 1)) | a0)
                             != 0)
                : ((a64 | a0) != 0);
    }
    return z;

}

static struct uint64_extra
 softfloat_shortShiftRightJam64Extra(
     u64int a, u64int extra, u8int dist )
{
    struct uint64_extra z;

    z.v = a>>dist;
    z.extra = a<<(-dist & 63) | (extra != 0);
    return z;

}

static struct exp32_sig64
softfloat_normSubnormalExtF80Sig( u64int sig )
{
    s8int shiftDist;
    struct exp32_sig64 z;

    shiftDist = softfloat_countLeadingZeros64( sig );
    z.exp = -shiftDist;
    z.sig = sig<<shiftDist;
    return z;

}

static struct uint128
 softfloat_sub128( u64int a64, u64int a0, u64int b64, u64int b0 )
{
    struct uint128 z;

    z.v0 = a0 - b0;
    z.v64 = a64 - b64 - (a0 < b0);
    return z;

}

static u64int
softfloat_shiftRightJam64( u64int a, u32int dist )
{
    return
        (dist < 63) ? a>>dist | ((u64int) (a<<(-dist & 63)) != 0) : (a != 0);

}

static struct uint128
 softfloat_shortShiftLeft128( u64int a64, u64int a0, u8int dist )
{
    struct uint128 z;

    z.v64 = a64<<dist | a0>>(-dist & 63);
    z.v0 = a0<<dist;
    return z;

}

static extFloat80_t
 softfloat_roundPackToExtF80(
     bool sign,
     s32int exp,
     u64int sig,
     u64int sigExtra,
     u8int roundingPrecision
 )
{
    u8int roundingMode;
    bool roundNearEven;
    u64int roundIncrement, roundMask, roundBits;
    bool isTiny, doIncrement;
    struct uint64_extra sig64Extra;
    union { struct extFloat80M s; extFloat80_t f; } uZ;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    roundingMode = softfloat_roundingMode;
    roundNearEven = (roundingMode == softfloat_round_near_even);
    if ( roundingPrecision == 80 ) goto precision80;
    if ( roundingPrecision == 64 ) {
        roundIncrement = UINT64_C( 0x0000000000000400 );
        roundMask = UINT64_C( 0x00000000000007FF );
    } else if ( roundingPrecision == 32 ) {
        roundIncrement = UINT64_C( 0x0000008000000000 );
        roundMask = UINT64_C( 0x000000FFFFFFFFFF );
    } else {
        goto precision80;
    }
    sig |= (sigExtra != 0);
    if ( ! roundNearEven && (roundingMode != softfloat_round_near_maxMag) ) {
        roundIncrement =
            (roundingMode
                 == (sign ? softfloat_round_min : softfloat_round_max))
                ? roundMask
                : 0;
    }
    roundBits = sig & roundMask;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( 0x7FFD <= (u32int) (exp - 1) ) {
        if ( exp <= 0 ) {
            /*----------------------------------------------------------------
            *----------------------------------------------------------------*/
            isTiny =
                   (softfloat_detectTininess
                        == softfloat_tininess_beforeRounding)
                || (exp < 0)
                || (sig <= (u64int) (sig + roundIncrement));
            sig = softfloat_shiftRightJam64( sig, 1 - exp );
            roundBits = sig & roundMask;
            if ( roundBits ) {
                if ( isTiny ) softfloat_raiseFlags( softfloat_flag_underflow );
                softfloat_exceptionFlags |= softfloat_flag_inexact;
            }
            sig += roundIncrement;
            exp = ((sig & UINT64_C( 0x8000000000000000 )) != 0);
            roundIncrement = roundMask + 1;
            if ( roundNearEven && (roundBits<<1 == roundIncrement) ) {
                roundMask |= roundIncrement;
            }
            sig &= ~roundMask;
            goto packReturn;
        }
        if (
               (0x7FFE < exp)
            || ((exp == 0x7FFE) && ((u64int) (sig + roundIncrement) < sig))
        ) {
            goto overflow;
        }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( roundBits ) {
        softfloat_exceptionFlags |= softfloat_flag_inexact;
    }
    sig = (u64int) (sig + roundIncrement);
    if ( sig < roundIncrement ) {
        ++exp;
        sig = UINT64_C( 0x8000000000000000 );
    }
    roundIncrement = roundMask + 1;
    if ( roundNearEven && (roundBits<<1 == roundIncrement) ) {
        roundMask |= roundIncrement;
    }
    sig &= ~roundMask;
    goto packReturn;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 precision80:
    doIncrement = (UINT64_C( 0x8000000000000000 ) <= sigExtra);
    if ( ! roundNearEven && (roundingMode != softfloat_round_near_maxMag) ) {
        doIncrement =
            (roundingMode
                 == (sign ? softfloat_round_min : softfloat_round_max))
                && sigExtra;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( 0x7FFD <= (u32int) (exp - 1) ) {
        if ( exp <= 0 ) {
            /*----------------------------------------------------------------
            *----------------------------------------------------------------*/
            isTiny =
                   (softfloat_detectTininess
                        == softfloat_tininess_beforeRounding)
                || (exp < 0)
                || ! doIncrement
                || (sig < UINT64_C( 0xFFFFFFFFFFFFFFFF ));
            sig64Extra =
                softfloat_shiftRightJam64Extra( sig, sigExtra, 1 - exp );
            exp = 0;
            sig = sig64Extra.v;
            sigExtra = sig64Extra.extra;
            if ( sigExtra ) {
                if ( isTiny ) softfloat_raiseFlags( softfloat_flag_underflow );
                softfloat_exceptionFlags |= softfloat_flag_inexact;
            }
            doIncrement = (UINT64_C( 0x8000000000000000 ) <= sigExtra);
            if (
                ! roundNearEven
                    && (roundingMode != softfloat_round_near_maxMag)
            ) {
                doIncrement =
                    (roundingMode
                         == (sign ? softfloat_round_min : softfloat_round_max))
                        && sigExtra;
            }
            if ( doIncrement ) {
                ++sig;
                sig &=
                    ~(u64int)
                         (! (sigExtra & UINT64_C( 0x7FFFFFFFFFFFFFFF ))
                              & roundNearEven);
                exp = ((sig & UINT64_C( 0x8000000000000000 )) != 0);
            }
            goto packReturn;
        }
        if (
               (0x7FFE < exp)
            || ((exp == 0x7FFE) && (sig == UINT64_C( 0xFFFFFFFFFFFFFFFF ))
                    && doIncrement)
        ) {
            /*----------------------------------------------------------------
            *----------------------------------------------------------------*/
            roundMask = 0;
 overflow:
            softfloat_raiseFlags(
                softfloat_flag_overflow | softfloat_flag_inexact );
            if (
                   roundNearEven
                || (roundingMode == softfloat_round_near_maxMag)
                || (roundingMode
                        == (sign ? softfloat_round_min : softfloat_round_max))
            ) {
                exp = 0x7FFF;
                sig = UINT64_C( 0x8000000000000000 );
            } else {
                exp = 0x7FFE;
                sig = ~roundMask;
            }
            goto packReturn;
        }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( sigExtra ) {
        softfloat_exceptionFlags |= softfloat_flag_inexact;
    }
    if ( doIncrement ) {
        ++sig;
        if ( ! sig ) {
            ++exp;
            sig = UINT64_C( 0x8000000000000000 );
        } else {
            sig &=
                ~(u64int)
                     (! (sigExtra & UINT64_C( 0x7FFFFFFFFFFFFFFF ))
                          & roundNearEven);
        }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 packReturn:
    uZ.s.signExp = packToExtF80UI64( sign, exp );
    uZ.s.signif = sig;
    return uZ.f;

}

static extFloat80_t
 softfloat_normRoundPackToExtF80(
     bool sign,
     s32int exp,
     u64int sig,
     u64int sigExtra,
     u8int roundingPrecision
 )
{
    s8int shiftDist;
    struct uint128 sig128;

    if ( ! sig ) {
        exp -= 64;
        sig = sigExtra;
        sigExtra = 0;
    }
    shiftDist = softfloat_countLeadingZeros64( sig );
    exp -= shiftDist;
    if ( shiftDist ) {
        sig128 = softfloat_shortShiftLeft128( sig, sigExtra, shiftDist );
        sig = sig128.v64;
        sigExtra = sig128.v0;
    }
    return
        softfloat_roundPackToExtF80(
            sign, exp, sig, sigExtra, roundingPrecision );

}

static extFloat80_t
 softfloat_subMagsExtF80(
     u16int uiA64,
     u64int uiA0,
     u16int uiB64,
     u64int uiB0,
     bool signZ
 )
{
    s32int expA;
    u64int sigA;
    s32int expB;
    u64int sigB;
    s32int expDiff;
    u16int uiZ64;
    u64int uiZ0;
    s32int expZ;
    u64int sigExtra;
    struct uint128 sig128, uiZ;
    union { struct extFloat80M s; extFloat80_t f; } uZ;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    expA = expExtF80UI64( uiA64 );
    sigA = uiA0;
    expB = expExtF80UI64( uiB64 );
    sigB = uiB0;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    expDiff = expA - expB;
    if ( 0 < expDiff ) goto expABigger;
    if ( expDiff < 0 ) goto expBBigger;
    if ( expA == 0x7FFF ) {
        if ( (sigA | sigB) & UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) {
            goto propagateNaN;
        }
        softfloat_raiseFlags( softfloat_flag_invalid );
        uiZ64 = defaultNaNExtF80UI64;
        uiZ0  = defaultNaNExtF80UI0;
        goto uiZ;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    expZ = expA;
    if ( ! expZ ) expZ = 1;
    sigExtra = 0;
    if ( sigB < sigA ) goto aBigger;
    if ( sigA < sigB ) goto bBigger;
    uiZ64 =
        packToExtF80UI64( (softfloat_roundingMode == softfloat_round_min), 0 );
    uiZ0 = 0;
    goto uiZ;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 expBBigger:
    if ( expB == 0x7FFF ) {
        if ( sigB & UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) goto propagateNaN;
        uiZ64 = packToExtF80UI64( signZ ^ 1, 0x7FFF );
        uiZ0  = UINT64_C( 0x8000000000000000 );
        goto uiZ;
    }
    if ( ! expA ) {
        ++expDiff;
        sigExtra = 0;
        if ( ! expDiff ) goto newlyAlignedBBigger;
    }
    sig128 = softfloat_shiftRightJam128( sigA, 0, -expDiff );
    sigA = sig128.v64;
    sigExtra = sig128.v0;
 newlyAlignedBBigger:
    expZ = expB;
 bBigger:
    signZ = ! signZ;
    sig128 = softfloat_sub128( sigB, 0, sigA, sigExtra );
    goto normRoundPack;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 expABigger:
    if ( expA == 0x7FFF ) {
        if ( sigA & UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) goto propagateNaN;
        uiZ64 = uiA64;
        uiZ0  = uiA0;
        goto uiZ;
    }
    if ( ! expB ) {
        --expDiff;
        sigExtra = 0;
        if ( ! expDiff ) goto newlyAlignedABigger;
    }
    sig128 = softfloat_shiftRightJam128( sigB, 0, expDiff );
    sigB = sig128.v64;
    sigExtra = sig128.v0;
 newlyAlignedABigger:
    expZ = expA;
 aBigger:
    sig128 = softfloat_sub128( sigA, 0, sigB, sigExtra );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 normRoundPack:
    return
        softfloat_normRoundPackToExtF80(
            signZ, expZ, sig128.v64, sig128.v0, extF80_roundingPrecision );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 propagateNaN:
    uiZ = softfloat_propagateNaNExtF80UI( uiA64, uiA0, uiB64, uiB0 );
    uiZ64 = uiZ.v64;
    uiZ0  = uiZ.v0;
 uiZ:
    uZ.s.signExp = uiZ64;
    uZ.s.signif  = uiZ0;
    return uZ.f;

}

static extFloat80_t
 softfloat_addMagsExtF80(
     u16int uiA64,
     u64int uiA0,
     u16int uiB64,
     u64int uiB0,
     bool signZ
 )
{
    s32int expA;
    u64int sigA;
    s32int expB;
    u64int sigB;
    s32int expDiff;
    u16int uiZ64;
    u64int uiZ0, sigZ, sigZExtra;
    struct exp32_sig64 normExpSig;
    s32int expZ;
    struct uint64_extra sig64Extra;
    struct uint128 uiZ;
    union { struct extFloat80M s; extFloat80_t f; } uZ;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    expA = expExtF80UI64( uiA64 );
    sigA = uiA0;
    expB = expExtF80UI64( uiB64 );
    sigB = uiB0;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    expDiff = expA - expB;
    if ( ! expDiff ) {
        if ( expA == 0x7FFF ) {
            if ( (sigA | sigB) & UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) {
                goto propagateNaN;
            }
            uiZ64 = uiA64;
            uiZ0  = uiA0;
            goto uiZ;
        }
        sigZ = sigA + sigB;
        sigZExtra = 0;
        if ( ! expA ) {
            normExpSig = softfloat_normSubnormalExtF80Sig( sigZ );
            expZ = normExpSig.exp + 1;
            sigZ = normExpSig.sig;
            goto roundAndPack;
        }
        expZ = expA;
        goto shiftRight1;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( expDiff < 0 ) {
        if ( expB == 0x7FFF ) {
            if ( sigB & UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) goto propagateNaN;
            uiZ64 = packToExtF80UI64( signZ, 0x7FFF );
            uiZ0  = uiB0;
            goto uiZ;
        }
        expZ = expB;
        if ( ! expA ) {
            ++expDiff;
            sigZExtra = 0;
            if ( ! expDiff ) goto newlyAligned;
        }
        sig64Extra = softfloat_shiftRightJam64Extra( sigA, 0, -expDiff );
        sigA = sig64Extra.v;
        sigZExtra = sig64Extra.extra;
    } else {
        if ( expA == 0x7FFF ) {
            if ( sigA & UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) goto propagateNaN;
            uiZ64 = uiA64;
            uiZ0  = uiA0;
            goto uiZ;
        }
        expZ = expA;
        if ( ! expB ) {
            --expDiff;
            sigZExtra = 0;
            if ( ! expDiff ) goto newlyAligned;
        }
        sig64Extra = softfloat_shiftRightJam64Extra( sigB, 0, expDiff );
        sigB = sig64Extra.v;
        sigZExtra = sig64Extra.extra;
    }
 newlyAligned:
    sigZ = sigA + sigB;
    if ( sigZ & UINT64_C( 0x8000000000000000 ) ) goto roundAndPack;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 shiftRight1:
    sig64Extra = softfloat_shortShiftRightJam64Extra( sigZ, sigZExtra, 1 );
    sigZ = sig64Extra.v | UINT64_C( 0x8000000000000000 );
    sigZExtra = sig64Extra.extra;
    ++expZ;
 roundAndPack:
    return
        softfloat_roundPackToExtF80(
            signZ, expZ, sigZ, sigZExtra, extF80_roundingPrecision );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 propagateNaN:
    uiZ = softfloat_propagateNaNExtF80UI( uiA64, uiA0, uiB64, uiB0 );
    uiZ64 = uiZ.v64;
    uiZ0  = uiZ.v0;
 uiZ:
    uZ.s.signExp = uiZ64;
    uZ.s.signif  = uiZ0;
    return uZ.f;

}

static void
 softfloat_addM(
     u8int size_words,
     const u32int *aPtr,
     const u32int *bPtr,
     u32int *zPtr
 )
{
    unsigned int index, lastIndex;
    u8int carry;
    u32int wordA, wordZ;

    index = indexWordLo( size_words );
    lastIndex = indexWordHi( size_words );
    carry = 0;
    for (;;) {
        wordA = aPtr[index];
        wordZ = wordA + bPtr[index] + carry;
        zPtr[index] = wordZ;
        if ( index == lastIndex ) break;
        if ( wordZ != wordA ) carry = (wordZ < wordA);
        index += wordIncr;
    }

}

/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_addM' with 'size_words'
| = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_add96M( aPtr, bPtr, zPtr ) softfloat_addM( 3, aPtr, bPtr, zPtr )

static void
softfloat_mul64To128M( u64int a, u64int b, u32int *zPtr )
{
    u32int a32, a0, b32, b0;
    u64int z0, mid1, z64, mid;

    a32 = a>>32;
    a0 = a;
    b32 = b>>32;
    b0 = b;
    z0 = (u64int) a0 * b0;
    mid1 = (u64int) a32 * b0;
    mid = mid1 + (u64int) a0 * b32;
    z64 = (u64int) a32 * b32;
    z64 += (u64int) (mid < mid1)<<32 | mid>>32;
    mid <<= 32;
    z0 += mid;
    zPtr[indexWord( 4, 1 )] = z0>>32;
    zPtr[indexWord( 4, 0 )] = z0;
    z64 += (z0 < mid);
    zPtr[indexWord( 4, 3 )] = z64>>32;
    zPtr[indexWord( 4, 2 )] = z64;

}

static void
softfloat_invalidExtF80M( struct extFloat80M *zSPtr )
{

    softfloat_raiseFlags( softfloat_flag_invalid );
    zSPtr->signExp = defaultNaNExtF80UI64;
    zSPtr->signif  = defaultNaNExtF80UI0;

}

static bool
 softfloat_tryPropagateNaNExtF80M(
     const struct extFloat80M *aSPtr,
     const struct extFloat80M *bSPtr,
     struct extFloat80M *zSPtr
 )
{
    u16int ui64;
    u64int ui0;

    ui64 = aSPtr->signExp;
    ui0  = aSPtr->signif;
    if ( isNaNExtF80UI( ui64, ui0 ) ) goto propagateNaN;
    ui64 = bSPtr->signExp;
    ui0  = bSPtr->signif;
    if ( isNaNExtF80UI( ui64, ui0 ) ) goto propagateNaN;
    return false;
 propagateNaN:
    softfloat_propagateNaNExtF80M( aSPtr, bSPtr, zSPtr );
    return true;

}

static void
 softfloat_shortShiftRightJamM(
     u8int size_words,
     const u32int *aPtr,
     u8int dist,
     u32int *zPtr
 )
{
    u8int uNegDist;
    unsigned int index, lastIndex;
    u32int partWordZ, wordA;

    uNegDist = -dist;
    index = indexWordLo( size_words );
    lastIndex = indexWordHi( size_words );
    wordA = aPtr[index];
    partWordZ = wordA>>dist;
    if ( partWordZ<<dist != wordA ) partWordZ |= 1;
    while ( index != lastIndex ) {
        wordA = aPtr[index + wordIncr];
        zPtr[index] = wordA<<(uNegDist & 31) | partWordZ;
        index += wordIncr;
        partWordZ = wordA>>dist;
    }
    zPtr[index] = partWordZ;

}

/*----------------------------------------------------------------------------
| This function or macro is the same as 'softfloat_shiftRightJamM' with
| 'size_words' = 3 (N = 96).
*----------------------------------------------------------------------------*/
#define softfloat_shiftRightJam96M( aPtr, dist, zPtr ) softfloat_shiftRightJamM( 3, aPtr, dist, zPtr )

static void
 softfloat_shiftRightJamM(
     u8int size_words,
     const u32int *aPtr,
     u32int dist,
     u32int *zPtr
 )
{
    u32int wordJam, wordDist, *ptr;
    u8int i, innerDist;

    wordJam = 0;
    wordDist = dist>>5;
    ptr = nil;
    if ( wordDist ) {
        if ( size_words < wordDist ) wordDist = size_words;
        ptr = (u32int *) (aPtr + indexMultiwordLo( size_words, wordDist ));
        i = wordDist;
        do {
            wordJam = *ptr++;
            if ( wordJam ) break;
            --i;
        } while ( i );
        ptr = zPtr;
    }
    if ( wordDist < size_words ) {
        aPtr += indexMultiwordHiBut( size_words, wordDist );
        innerDist = dist & 31;
        if ( innerDist ) {
            softfloat_shortShiftRightJamM(
                size_words - wordDist,
                aPtr,
                innerDist,
                zPtr + indexMultiwordLoBut( size_words, wordDist )
            );
            if ( ! wordDist ) goto wordJam;
        } else {
            aPtr += indexWordLo( size_words - wordDist );
            ptr = zPtr + indexWordLo( size_words );
            for ( i = size_words - wordDist; i; --i ) {
                *ptr = *aPtr;
                aPtr += wordIncr;
                ptr += wordIncr;
            }
        }
        ptr = zPtr + indexMultiwordHi( size_words, wordDist );
    }
    do {
        *ptr++ = 0;
        --wordDist;
    } while ( wordDist );
 wordJam:
    if ( wordJam ) zPtr[indexWordLo( size_words )] |= 1;

}

static void
 softfloat_roundPackMToExtF80M(
     bool sign,
     s32int exp,
     u32int *extSigPtr,
     u8int roundingPrecision,
     struct extFloat80M *zSPtr
 )
{
    u8int roundingMode;
    bool roundNearEven;
    u64int sig, roundIncrement, roundMask, roundBits;
    bool isTiny;
    u32int sigExtra;
    bool doIncrement;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    roundingMode = softfloat_roundingMode;
    roundNearEven = (roundingMode == softfloat_round_near_even);
    sig =
        (u64int) extSigPtr[indexWord( 3, 2 )]<<32
            | extSigPtr[indexWord( 3, 1 )];
    if ( roundingPrecision == 80 ) goto precision80;
    if ( roundingPrecision == 64 ) {
        roundIncrement = UINT64_C( 0x0000000000000400 );
        roundMask = UINT64_C( 0x00000000000007FF );
    } else if ( roundingPrecision == 32 ) {
        roundIncrement = UINT64_C( 0x0000008000000000 );
        roundMask = UINT64_C( 0x000000FFFFFFFFFF );
    } else {
        goto precision80;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( extSigPtr[indexWordLo( 3 )] ) sig |= 1;
    if ( ! roundNearEven && (roundingMode != softfloat_round_near_maxMag) ) {
        roundIncrement =
            (roundingMode
                 == (sign ? softfloat_round_min : softfloat_round_max))
                ? roundMask
                : 0;
    }
    roundBits = sig & roundMask;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( 0x7FFD <= (u32int) (exp - 1) ) {
        if ( exp <= 0 ) {
            /*----------------------------------------------------------------
            *----------------------------------------------------------------*/
            isTiny =
                   (softfloat_detectTininess
                        == softfloat_tininess_beforeRounding)
                || (exp < 0)
                || (sig <= (u64int) (sig + roundIncrement));
            sig = softfloat_shiftRightJam64( sig, 1 - exp );
            roundBits = sig & roundMask;
            if ( roundBits ) {
                if ( isTiny ) softfloat_raiseFlags( softfloat_flag_underflow );
                softfloat_exceptionFlags |= softfloat_flag_inexact;
            }
            sig += roundIncrement;
            exp = ((sig & UINT64_C( 0x8000000000000000 )) != 0);
            roundIncrement = roundMask + 1;
            if ( roundNearEven && (roundBits<<1 == roundIncrement) ) {
                roundMask |= roundIncrement;
            }
            sig &= ~roundMask;
            goto packReturn;
        }
        if (
               (0x7FFE < exp)
            || ((exp == 0x7FFE) && ((u64int) (sig + roundIncrement) < sig))
        ) {
            goto overflow;
        }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( roundBits ) {
        softfloat_exceptionFlags |= softfloat_flag_inexact;
    }
    sig += roundIncrement;
    if ( sig < roundIncrement ) {
        ++exp;
        sig = UINT64_C( 0x8000000000000000 );
    }
    roundIncrement = roundMask + 1;
    if ( roundNearEven && (roundBits<<1 == roundIncrement) ) {
        roundMask |= roundIncrement;
    }
    sig &= ~roundMask;
    goto packReturn;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 precision80:
    sigExtra = extSigPtr[indexWordLo( 3 )];
    doIncrement = (0x80000000 <= sigExtra);
    if ( ! roundNearEven && (roundingMode != softfloat_round_near_maxMag) ) {
        doIncrement =
            (roundingMode
                 == (sign ? softfloat_round_min : softfloat_round_max))
                && sigExtra;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( 0x7FFD <= (u32int) (exp - 1) ) {
        if ( exp <= 0 ) {
            /*----------------------------------------------------------------
            *----------------------------------------------------------------*/
            isTiny =
                   (softfloat_detectTininess
                        == softfloat_tininess_beforeRounding)
                || (exp < 0)
                || ! doIncrement
                || (sig < UINT64_C( 0xFFFFFFFFFFFFFFFF ));
            softfloat_shiftRightJam96M( extSigPtr, 1 - exp, extSigPtr );
            exp = 0;
            sig =
                (u64int) extSigPtr[indexWord( 3, 2 )]<<32
                    | extSigPtr[indexWord( 3, 1 )];
            sigExtra = extSigPtr[indexWordLo( 3 )];
            if ( sigExtra ) {
                if ( isTiny ) softfloat_raiseFlags( softfloat_flag_underflow );
                softfloat_exceptionFlags |= softfloat_flag_inexact;
            }
            doIncrement = (0x80000000 <= sigExtra);
            if (
                ! roundNearEven
                    && (roundingMode != softfloat_round_near_maxMag)
            ) {
                doIncrement =
                    (roundingMode
                         == (sign ? softfloat_round_min : softfloat_round_max))
                        && sigExtra;
            }
            if ( doIncrement ) {
                ++sig;
                sig &= ~(u64int) (! (sigExtra & 0x7FFFFFFF) & roundNearEven);
                exp = ((sig & UINT64_C( 0x8000000000000000 )) != 0);
            }
            goto packReturn;
        }
        if (
               (0x7FFE < exp)
            || ((exp == 0x7FFE) && (sig == UINT64_C( 0xFFFFFFFFFFFFFFFF ))
                    && doIncrement)
        ) {
            /*----------------------------------------------------------------
            *----------------------------------------------------------------*/
            roundMask = 0;
 overflow:
            softfloat_raiseFlags(
                softfloat_flag_overflow | softfloat_flag_inexact );
            if (
                   roundNearEven
                || (roundingMode == softfloat_round_near_maxMag)
                || (roundingMode
                        == (sign ? softfloat_round_min : softfloat_round_max))
            ) {
                exp = 0x7FFF;
                sig = UINT64_C( 0x8000000000000000 );
            } else {
                exp = 0x7FFE;
                sig = ~roundMask;
            }
            goto packReturn;
        }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( sigExtra ) {
        softfloat_exceptionFlags |= softfloat_flag_inexact;
    }
    if ( doIncrement ) {
        ++sig;
        if ( ! sig ) {
            ++exp;
            sig = UINT64_C( 0x8000000000000000 );
        } else {
            sig &= ~(u64int) (! (sigExtra & 0x7FFFFFFF) & roundNearEven);
        }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 packReturn:
    zSPtr->signExp = packToExtF80UI64( sign, exp );
    zSPtr->signif = sig;

}

void
f32_to_extF80M( float32_t a, extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    union ui32_f32 uA;
    u32int uiA;
    bool sign;
    s16int exp;
    u32int frac;
    struct commonNaN commonNaN;
    u16int uiZ64;
    u32int uiZ32;
    struct exp16_sig32 normExpSig;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    zSPtr = (struct extFloat80M *) zPtr;
    uA.f = a;
    uiA = uA.ui;
    sign = signF32UI( uiA );
    exp  = expF32UI( uiA );
    frac = fracF32UI( uiA );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( exp == 0xFF ) {
        if ( frac ) {
            softfloat_f32UIToCommonNaN( uiA, &commonNaN );
            softfloat_commonNaNToExtF80M( &commonNaN, zSPtr );
            return;
        }
        uiZ64 = packToExtF80UI64( sign, 0x7FFF );
        uiZ32 = 0x80000000;
        goto uiZ;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( ! exp ) {
        if ( ! frac ) {
            uiZ64 = packToExtF80UI64( sign, 0 );
            uiZ32 = 0;
            goto uiZ;
        }
        normExpSig = softfloat_normSubnormalF32Sig( frac );
        exp = normExpSig.exp;
        frac = normExpSig.sig;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uiZ64 = packToExtF80UI64( sign, exp + 0x3F80 );
    uiZ32 = 0x80000000 | (u32int) frac<<8;
 uiZ:
    zSPtr->signExp = uiZ64;
    zSPtr->signif = (u64int) uiZ32<<32;

}

float32_t
extF80M_to_f32( const extFloat80_t *aPtr )
{
    const struct extFloat80M *aSPtr;
    u16int uiA64;
    bool sign;
    s32int exp;
    u64int sig;
    struct commonNaN commonNaN;
    u32int uiZ, sig32;
    union ui32_f32 uZ;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    aSPtr = (const struct extFloat80M *) aPtr;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uiA64 = aSPtr->signExp;
    sign = signExtF80UI64( uiA64 );
    exp  = expExtF80UI64( uiA64 );
    sig = aSPtr->signif;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( exp == 0x7FFF ) {
        if ( sig & UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) {
            softfloat_extF80MToCommonNaN( aSPtr, &commonNaN );
            uiZ = softfloat_commonNaNToF32UI( &commonNaN );
        } else {
            uiZ = packToF32UI( sign, 0xFF, 0 );
        }
        goto uiZ;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( ! (sig & UINT64_C( 0x8000000000000000 )) ) {
        if ( ! sig ) {
            uiZ = packToF32UI( sign, 0, 0 );
            goto uiZ;
        }
        exp += softfloat_normExtF80SigM( &sig );
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    sig32 = softfloat_shortShiftRightJam64( sig, 33 );
    exp -= 0x3F81;
    if ( sizeof (s16int) < sizeof (s32int) ) {
        if ( exp < -0x1000 ) exp = -0x1000;
    }
    return softfloat_roundPackToF32( sign, exp, sig32 );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 uiZ:
    uZ.ui = uiZ;
    return uZ.f;

}

void
 extF80M_add(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{
    const struct extFloat80M *aSPtr, *bSPtr;
    u16int uiA64;
    u64int uiA0;
    bool signA;
    u16int uiB64;
    u64int uiB0;
    bool signB;

    aSPtr = (const struct extFloat80M *) aPtr;
    bSPtr = (const struct extFloat80M *) bPtr;
    uiA64 = aSPtr->signExp;
    uiA0  = aSPtr->signif;
    signA = signExtF80UI64( uiA64 );
    uiB64 = bSPtr->signExp;
    uiB0  = bSPtr->signif;
    signB = signExtF80UI64( uiB64 );
    if ( signA == signB ) {
        *zPtr = softfloat_addMagsExtF80( uiA64, uiA0, uiB64, uiB0, signA );
    } else {
        *zPtr = softfloat_subMagsExtF80( uiA64, uiA0, uiB64, uiB0, signA );
    }
}

void
 extF80M_mul(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{
    const struct extFloat80M *aSPtr, *bSPtr;
    struct extFloat80M *zSPtr;
    u16int uiA64;
    s32int expA;
    u16int uiB64;
    s32int expB;
    bool signZ;
    u16int uiZ64;
    u64int uiZ0, sigA, sigB;
    s32int expZ;
    u32int sigProd[4], *extSigZPtr;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    aSPtr = (const struct extFloat80M *) aPtr;
    bSPtr = (const struct extFloat80M *) bPtr;
    zSPtr = (struct extFloat80M *) zPtr;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uiA64 = aSPtr->signExp;
    expA = expExtF80UI64( uiA64 );
    uiB64 = bSPtr->signExp;
    expB = expExtF80UI64( uiB64 );
    signZ = signExtF80UI64( uiA64 ) ^ signExtF80UI64( uiB64 );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( (expA == 0x7FFF) || (expB == 0x7FFF) ) {
        if ( softfloat_tryPropagateNaNExtF80M( aSPtr, bSPtr, zSPtr ) ) return;
        if (
               (! aSPtr->signif && (expA != 0x7FFF))
            || (! bSPtr->signif && (expB != 0x7FFF))
        ) {
            softfloat_invalidExtF80M( zSPtr );
            return;
        }
        uiZ64 = packToExtF80UI64( signZ, 0x7FFF );
        uiZ0  = UINT64_C( 0x8000000000000000 );
        goto uiZ;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( ! expA ) expA = 1;
    sigA = aSPtr->signif;
    if ( ! (sigA & UINT64_C( 0x8000000000000000 )) ) {
        if ( ! sigA ) goto zero;
        expA += softfloat_normExtF80SigM( &sigA );
    }
    if ( ! expB ) expB = 1;
    sigB = bSPtr->signif;
    if ( ! (sigB & UINT64_C( 0x8000000000000000 )) ) {
        if ( ! sigB ) goto zero;
        expB += softfloat_normExtF80SigM( &sigB );
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    expZ = expA + expB - 0x3FFE;
    softfloat_mul64To128M( sigA, sigB, sigProd );
    if ( sigProd[indexWordLo( 4 )] ) sigProd[indexWord( 4, 1 )] |= 1;
    extSigZPtr = &sigProd[indexMultiwordHi( 4, 3 )];
    if ( sigProd[indexWordHi( 4 )] < 0x80000000 ) {
        --expZ;
        softfloat_add96M( extSigZPtr, extSigZPtr, extSigZPtr );
    }
    softfloat_roundPackMToExtF80M(
        signZ, expZ, extSigZPtr, extF80_roundingPrecision, zSPtr );
    return;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 zero:
    uiZ64 = packToExtF80UI64( signZ, 0 );
    uiZ0  = 0;
 uiZ:
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;

}
