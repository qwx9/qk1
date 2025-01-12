// fn.h: quintet bit popcount patricia tries, new version
//
// This version uses somewhat different terminology than older
// variants. The location of a quintet in the key is now called its
// "offset", and the whole word containing the offset, bitmap, and tag
// bit is called the "index word" (by analogy with a database index).
// The precise quintet location is represented as a byte offset and a
// shift. Previously a flags field contained the isbranch tag and shift,
// but these are now separate.
//
// Instead of trying to use bit fields, this code uses accessor
// functions to split up a pair of words into their constituent parts.
// This should improve portability to machines with varying endianness
// and/or word size.
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#pragma once

typedef u32int Tbitmap;
typedef u64int Tindex;

#ifdef __plan9__
static inline u32int
__builtin_popcount(Tbitmap w)
{
	w -= (w >> 1) & 0x55555555U;
	w = (w & 0x33333333U) + ((w >> 2) & 0x33333333U);
	w = (w + (w >> 4)) & 0x0F0F0F0FU;
	w = (w * 0x01010101U) >> 24;
	return w;
}
#endif

typedef struct Tbl {
	Tindex index;
	void *ptr;
}Trie;

// accessor functions, except for the index word

#define Tset_field(cast, elem, type, field) \
	static inline void \
	Tset_##field(Trie *t, type field) \
	{ \
		t->elem = cast field; \
	} \
	struct dummy

Tset_field((void *),          ptr,   Trie *,       twigs);
Tset_field((Tindex),          index, Tindex,       index);
Tset_field((void *)(uintptr), ptr,   const char *, key);
Tset_field((Tindex)(uintptr), index, void *,       val);

static inline bool Tindex_branch(Tindex i);

static inline bool isbranch(Trie *t)
{
	return Tindex_branch(t->index);
}

#define Tbranch(t) assert(isbranch(t))
#define Tleaf(t)  assert(!isbranch(t))

#define Tcheck_get(type, tag, field, expr) \
	static inline type \
	tag##_##field(Trie *t) \
	{ \
		tag(t); \
		return expr; \
	} \
	struct dummy

Tcheck_get(Trie *,       Tbranch, twigs, t->ptr);
Tcheck_get(const char *, Tleaf,   key,   t->ptr);
Tcheck_get(void *,       Tleaf,   val,   (void*)(uintptr)t->index);

// index word layout

#define Tix_width_branch 1
#define Tix_width_shift  3
#define Tix_width_offset 28
#define Tix_width_bitmap 32

#define Tix_base_branch 0
#define Tix_base_shift  (Tix_base_branch + Tix_width_branch)
#define Tix_base_offset (Tix_base_shift  + Tix_width_shift)
#define Tix_base_bitmap (Tix_base_offset + Tix_width_offset)

#define Tix_place(field) ((Tindex)(field) << Tix_base_##field)

#define Tix_mask(field) ((1ULL << Tix_width_##field) - 1ULL)

#define Tunmask(field,index) \
	((u32int)(((index) >> Tix_base_##field) & Tix_mask(field)))

#define Tmaxlen Tix_mask(offset)

// index word accessor functions

#define Tindex_get(type, field) \
	static inline type \
	Tindex_##field(Tindex i) \
	{ \
		return Tunmask(field, i); \
	} \
	struct dummy

Tindex_get(bool, branch);
Tindex_get(u32int, shift);
Tindex_get(u32int, offset);
Tindex_get(Tbitmap, bitmap);

static inline Tindex
Tindex_new(u32int shift, u32int offset, Tbitmap bitmap)
{
	u32int branch = 1;
	return
		Tix_place(branch) |
		Tix_place(shift)  |
		Tix_place(offset) |
		Tix_place(bitmap) ;
}

static inline Tindex
Tbitmap_add(Tindex i, Tbitmap bitmap)
{
	return i | Tix_place(bitmap);
}

static inline Tindex
Tbitmap_del(Tindex i, Tbitmap bitmap)
{
	return i & ~Tix_place(bitmap);
}

#if 0
// sanity checks!
static_assert(Tix_base_bitmap + Tix_width_bitmap == 64,
	      "index fields must fill a 64 bit word");

static_assert(Tunmask(bitmap,0x1234567800000000ULL) == 0x12345678,
	      "extracting the bitmap works");

static_assert(Tunmask(offset,0x0420ULL) == 0x42,
	      "extracting the offset works");

static_assert(Tunmask(shift,0xFEDCBAULL) == 5,
	      "extracting the shift works");
#endif

//  ..key[o%5==0].. ..key[o%5==1].. ..key[o%5==2].. ..key[o%5==3].. ..key[o%5==4]..
// |               |               |               |               |               |
//  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
// |         |         |         |         |         |         |         |         |
//  shift=0   shift=5   shift=2   shift=7   shift=4   shift=1   shift=6   shift=3

static inline u8int
knybble(const char *key, u32int off, u32int shift)
{
	u32int word = (u8int)key[off]<<8;
	if(word)
		word |= key[off+1];
	u32int right = 16 - 5 - shift;
	return (word >> right) & 0x1FU;
}

static inline u8int
nibble(Tindex i, const char *key, int len)
{
	u32int off = Tindex_offset(i);
	if(off >= (u32int)len)
		return 0;
	return knybble(key, off, Tindex_shift(i));
}

static inline Tbitmap
twigbit(Tindex i, const char *key, int len)
{
	return 1U << nibble(i, key, len);
}

static inline bool
hastwig(Tindex i, Tbitmap bit)
{
	return Tindex_bitmap(i) & bit;
}

static inline u32int
twigoff(Tindex i, Tbitmap bit)
{
	return __builtin_popcount(Tindex_bitmap(i) & (bit-1));
}

#define TWIGOFFMAX(off, max, i, b) do{ \
		off = twigoff(i, b); \
		max = __builtin_popcount(Tindex_bitmap(i)); \
	}while(0)
