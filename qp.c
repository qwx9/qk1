#include "quakedef.h"
#include "qp.h"

typedef u16int Tbitmap;
typedef struct Tbranch Tbranch;
typedef struct Tleaf Tleaf;

struct Tleaf {
	char *k;
	void *v;
};

struct Tbranch {
	Trie *twigs;
	u64int x;
};

union Trie {
	Tleaf leaf;
	Tbranch branch;
};

#define x_flags(x) ((x)>>62)
#define x_index(x) ((x)>>16 & ((1ULL<<46)-1ULL))
#define x_bitmap(x) ((x) & 0xffff)

#define isbranch(t) (x_flags((t)->branch.x) != 0)
#define hastwig(t, b) (x_bitmap((t)->branch.x) & (b))
#define twigoff(t, b) popcount(x_bitmap((t)->branch.x) & ((b)-1))
#define twig(t, i) (&(t)->branch.twigs[(i)])
#define twigoffmax(off, max, t, b) do{ off = twigoff(t, b); max = popcount(x_bitmap((t)->branch.x)); }while(0)
#define nibbit(k, f) (1 << (((k) & (((f)-2) ^ 0x0f) & 0xff) >> ((2-(f))<<2)))
#define twigbit(t, k, len) \
	(x_index((t)->branch.x) >= (u64int)(len) ? \
	1 : \
	nibbit((u8int)(k)[x_index((t)->branch.x)], x_flags((t)->branch.x)))

/*
// need some speed? just say no.
#define POPCNT	BYTE $0xf3; BYTE $0x0f; BYTE $0xb8

TEXT popcount(SB),$0
	MOVL b+0(FP), AX
	POPCNT
	RET
*/
static u16int
popcount(u16int b)
{
	b -= b>>1 & 0x5555;
	b = (b & 0x3333) + ((b>>2) & 0x3333);
	b = (b + (b>>4)) & 0x0f0f;
	b = (b + (b>>8)) & 0x00ff;
	return b;
}

int
qpget(Trie *t, char *k, int len, char **pk, void **pv)
{
	Tbitmap b;

	assert(k != nil && pk != nil && pv != nil);

	if(len < 1)
		len = strlen(k);
	if(t == nil)
		return -1;
	for(; isbranch(t); t = twig(t, twigoff(t, b))){
		b = twigbit(t, k, len);
		if(!hastwig(t, b))
			return -1;
	}
	if(strncmp(k, t->leaf.k, len) != 0)
		return -1;
	*pk = t->leaf.k;
	*pv = t->leaf.v;

	return 0;
}

int
qpnext(Trie *t, char **pk, int *plen, void **pv)
{
	Tbitmap b;
	unsigned s, m;

	assert(pk != nil && plen != nil && pv != nil);

	if(isbranch(t)){
		b = twigbit(t, *pk, *plen);
		twigoffmax(s, m, t, b);
		for(; s < m; s++){
			if(qpnext(twig(t, s), pk, plen, pv) == 0)
				return 0;
		}
		return -1;
	}

	if(*pk == nil){
		*pk = t->leaf.k;
		*plen = strlen(*pk);
		*pv = t->leaf.v;
		return 0;
	}
	if(*pk == t->leaf.k || strcmp(*pk, t->leaf.k) == 0){
		*pk = nil;
		*plen = 0;
		*pv = nil;
		return -1;
	}

	return -1;
}

Trie *
qpdel(Trie *t, char *k, int len, char **pk, void **pv)
{
	Trie *p, *twigs;
	Tbitmap b;
	unsigned s, m;

	if(t == nil)
		return nil;
	assert(k != nil && pk != nil && pv != nil);
	if(len < 1)
		len = strlen(k);
	assert(len > 0);

	for(b = 0, p = nil; isbranch(t); p = t, t = twig(t, twigoff(t, b))){
		b = twigbit(t, k, len);
		if(!hastwig(t, b))
			return t;
	}
	
	if(strncmp(k, t->leaf.k, len) != 0)
		return t;
	*pk = t->leaf.k;
	*pv = t->leaf.v;

	if(p == nil){
		free(t);
		return nil;
	}
	t = p;

	twigoffmax(s, m, t, b);
	if(m == 2){
		twigs = t->branch.twigs;
		*t = *twig(t, !s);
		free(twigs);
		return t;
	}
	memmove(t->branch.twigs+s, t->branch.twigs+s+1, sizeof(*t)*(m-s-1));
	t->branch.x &= ~(u64int)b;

	t->branch.twigs = realloc(t->branch.twigs, sizeof(*t)*(m-1));
	return t;
}

Trie *
qpset(Trie *t, char *k, int len, void *v)
{
	Trie *t0, t1, t2;
	unsigned i, s, m;
	u8int f;
	Tbitmap b, b1, b2;
	u8int k2;

	assert(k != nil && v != nil);
	if(len < 1)
		len = strlen(k);
	assert(len > 0);

	if(t == nil){
		t = malloc(sizeof(*t));
		assert(t != nil);
		t->leaf.k = k;
		t->leaf.v = v;
		return t;
	}

	t0 = t;
	for(; isbranch(t); t = twig(t, i)){
		b = twigbit(t, k, len);
		i = hastwig(t, b) ? twigoff(t, b) : 0;
	}
	for(i = 0; i <= (unsigned)len && k[i] == t->leaf.k[i]; i++);
	if(i == (unsigned)len+1){
		t->leaf.v = v;
		return t0;
	}

	k2 = (u8int)t->leaf.k[i];
	f = (((u8int)k[i] ^ k2) & 0xf0) ? 1 : 2;
	b1 = nibbit(k[i], f);
	t1.leaf.k = k;
	t1.leaf.v = v;
	for(t = t0; isbranch(t); t = twig(t, twigoff(t, b))){
		if(i == x_index(t->branch.x)){
			if(f == x_flags(t->branch.x))
				goto growbranch;
			if(f < x_flags(t->branch.x))
				break;
		}else if(i < x_index(t->branch.x)){
			break;
		}
		b = twigbit(t, k, len);
		assert(hastwig(t, b));
	}

	t2 = *t;
	b2 = nibbit(k2, f);
	t->branch.twigs = malloc(sizeof(*t)*2);
	assert(t->branch.twigs != nil);
	t->branch.x = (u64int)f<<62 | (u64int)i<<16 | b1 | b2;
	*twig(t, twigoff(t, b1)) = t1;
	*twig(t, twigoff(t, b2)) = t2;

	return t0;

growbranch:
	assert(!hastwig(t, b1));
	twigoffmax(s, m, t, b1);
	t->branch.twigs = realloc(t->branch.twigs, sizeof(*t)*(m+1));
	memmove(t->branch.twigs+s+1, t->branch.twigs+s, sizeof(*t)*(m-s));
	memmove(t->branch.twigs+s, &t1, sizeof(t1));
	t->branch.x |= b1;

	return t0;
}
