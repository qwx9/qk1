// fn.c: quintet bit popcount patricia tries, new version
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include "platform.h"
#include "tbl.h"
#include "fn.h"

bool
Tgetkv(Tbl *t, const char *key, int len, const char **pkey, void **pval)
{
	if(t == nil)
		return false;
	while(isbranch(t)){
		Tindex i = t->index;
		Tbitmap b = twigbit(i, key, len);
		if(!hastwig(i, b))
			return false;
		t = Tbranch_twigs(t) + twigoff(i, b);
	}
	if(strcmp(key, Tleaf_key(t)) != 0)
		return false;
	*pkey = Tleaf_key(t);
	*pval = Tleaf_val(t);
	return true;
}

static bool
next_rec(Trie *t, const char **pkey, int *plen, void **pval)
{
	Tindex i = t->index;
	if(Tindex_branch(i)){
		// Recurse to find either this leaf (*pkey != nil)
		// or the next one (*pkey == nil).
		Tbitmap b = twigbit(i, *pkey, *plen);
		u32int s, m; TWIGOFFMAX(s, m, i, b);
		for(; s < m; s++){
			if(next_rec(Tbranch_twigs(t)+s, pkey, plen, pval))
				return true;
		}
		return false;
	}
	// We have found the next leaf.
	if(*pkey == nil){
		*pkey = Tleaf_key(t);
		*plen = strlen(*pkey);
		*pval = Tleaf_val(t);
		return true;
	}
	// We have found this leaf, so start looking for the next one.
	if(strcmp(*pkey, Tleaf_key(t)) == 0){
		*pkey = nil;
		*plen = 0;
		return false;
	}
	// No match.
	return false;
}

bool
Tnextl(Tbl *tbl, const char **pkey, int *plen, void **pval)
{
	if(tbl == nil){
		*pkey = nil;
		*plen = 0;
		return false;
	}
	return next_rec(tbl, pkey, plen, pval);
}

Tbl *
Tdelkv(Tbl *tbl, const char *key, int len, const char **pkey, void **pval)
{
	if(tbl == nil)
		return nil;
	Trie *t = tbl, *p = nil;
	Tindex i = 0;
	Tbitmap b = 0;
	while(isbranch(t)){
		i = t->index;
		b = twigbit(i, key, len);
		if(!hastwig(i, b))
			return tbl;
		p = t; t = Tbranch_twigs(t) + twigoff(i, b);
	}
	if(strcmp(key, Tleaf_key(t)) != 0)
		return tbl;
	*pkey = Tleaf_key(t);
	*pval = Tleaf_val(t);
	if(p == nil){
		free(tbl);
		return nil;
	}
	Trie *twigs = Tbranch_twigs(p);
	u32int m = __builtin_popcount(Tindex_bitmap(i));
	assert(twigs <= t && t < twigs+m);
	if(m == 2){
		// Move the other twig to the parent branch.
		*p = twigs[twigs == t];
		free(twigs);
		return tbl;
	}
	memmove(t, t+1, ((twigs + m) - (t + 1)) * sizeof(Trie));
	p->index = Tbitmap_del(i, b);
	// We have now correctly removed the twig from the trie, so if
	// realloc() fails we can ignore it and continue to use the
	// slightly oversized twig array.
	twigs = realloc(twigs, sizeof(Trie) * (m - 1));
	if(twigs != nil)
		Tset_twigs(p, twigs);
	return tbl;
}

Tbl *
Tsetl(Tbl *tbl, const char *key, int len, void *val)
{
	assert(!Tindex_branch((Tindex)(uintptr)val) && len <= (int)Tmaxlen);
	if(val == nil)
		return Tdell(tbl, key, len);
	// First leaf in an empty tbl?
	if(tbl == nil){
		tbl = malloc(sizeof(*tbl));
		if(tbl == nil)
			return nil;
		Tset_key(tbl, key);
		Tset_val(tbl, val);
		return tbl;
	}
	Trie *t = tbl;
	// Find the most similar leaf node in the trie. We will compare
	// its key with our new key to find the first differing nibble,
	// which can be at a lower index than the point at which we
	// detect a difference.
	while(isbranch(t)){
		Tindex i = t->index;
		Tbitmap b = twigbit(i, key, len);
		// Even if our key is missing from this branch we need to
		// keep iterating down to a leaf. It doesn't matter which
		// twig we choose since the keys are all the same up to this
		// index. Note that blindly using twigoff(t, b) can cause
		// an out-of-bounds index if it equals twigmax(t).
		u32int s = hastwig(i, b) ? twigoff(i, b) : 0;
		t = Tbranch_twigs(t) + s;
	}
	// Do the keys differ, and if so, where?
	u32int off, xor, shf;
	const char *tkey = Tleaf_key(t);
	for(off = 0; off <= (u32int)len; off++){
		xor = (u8int)key[off] ^ (u8int)tkey[off];
		if(xor != 0)
			goto newkey;
	}
	Tset_val(t, val);
	return tbl;
newkey:; // We have the branch's byte index; what is its chunk index?
	u32int bit = off * 8 + qclz(xor) + 8 - sizeof(u32int) * 8;
	u32int qo = bit / 5;
	off = qo * 5 / 8;
	shf = qo * 5 % 8;
	// re-index keys with adjusted offset
	Tbitmap nb = 1U << knybble(key,off,shf);
	Tbitmap tb = 1U << knybble(tkey,off,shf);
	// Prepare the new leaf.
	Trie nt;
	Tset_key(&nt, key);
	Tset_val(&nt, val);
	// Find where to insert a branch or grow an existing branch.
	t = tbl;
	Tindex i;
	while(isbranch(t)){
		i = t->index;
		if(off == Tindex_offset(i) && shf == Tindex_shift(i))
			goto growbranch;
		if(off == Tindex_offset(i) && shf < Tindex_shift(i))
			goto newbranch;
		if(off < Tindex_offset(i))
			goto newbranch;
		Tbitmap b = twigbit(i, key, len);
		assert(hastwig(i, b));
		t = Tbranch_twigs(t) + twigoff(i, b);
	}
newbranch:;
	Trie *twigs = malloc(sizeof(Trie) * 2);
	if(twigs == nil)
		return nil;
	i = Tindex_new(shf, off, nb | tb);
	twigs[twigoff(i, nb)] = nt;
	twigs[twigoff(i, tb)] = *t;
	Tset_twigs(t, twigs);
	Tset_index(t, i);
	return tbl;
growbranch:
	assert(!hastwig(i, nb));
	u32int s, m; TWIGOFFMAX(s, m, i, nb);
	twigs = realloc(Tbranch_twigs(t), sizeof(Trie) * (m + 1));
	if(twigs == nil)
		return nil;
	memmove(twigs+s+1, twigs+s, sizeof(Trie) * (m - s));
	memmove(twigs+s, &nt, sizeof(Trie));
	Tset_twigs(t, twigs);
	Tset_index(t, Tbitmap_add(i, nb));
	return tbl;
}
