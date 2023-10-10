#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

typedef struct mem_t mem_t;

struct mem_t
{
	mem_t *prev, *next;
	mem_user_t *user;
	int size; // NOT including this header
};

static mem_t *hunk_head;
static mem_t *cache_head;

int
Hunk_From(void *p)
{
	mem_t *m;
	int off;

	for(off = 0, m = hunk_head; m != nil; m = m->next){
		off += m->size;
		if((void*)(m+1) == p)
			break;
	}
	assert(m != nil || p == nil);
	return off;
}

void
Hunk_CacheFrom(mem_user_t *c, void *p)
{
	int size;
	mem_t *n, *prev;
	byte *data;

	for(size = 0, n = hunk_head; n != nil; n = n->next){
		size += n->size;
		if((void*)(n+1) == p)
			break;
	}
	assert((void*)(n+1) == p);
	data = Cache_Alloc(c, size);

	hunk_head = n->next;
	if(hunk_head != nil)
		hunk_head->prev = nil;
	for(size = 0; n != nil; n = prev){
		memmove(data+size, n+1, n->size);
		size += n->size;
		prev = n->prev;
		free(n);
	}
}

void *
Hunk_Alloc(int size)
{
	mem_t *m;

	size = (size+15)&~15;
	m = mallocz(sizeof(*m) + size, 1);
	if(m == nil)
		sysfatal("Hunk_Alloc: %r");
	m->size = size;
	m->next = hunk_head;
	if(hunk_head != nil)
		hunk_head->prev = m;
	hunk_head = m;

	return m+1;
}

void *
Hunk_Double(void *p)
{
	mem_t *m, *n;
	ulong t;

	m = p;
	m--;
	t = getmalloctag(m);
	n = realloc(m, sizeof(*m) + m->size*2);
	if(m == nil)
		sysfatal("Hunk_Double: %r");
	if(hunk_head == m)
		hunk_head = n;
	m = n;
	setmalloctag(m, t);
	memset((byte*)(m+1) + m->size, 0, m->size);
	m->size *= 2;
	if(m->prev != nil)
		m->prev->next = m;
	if(m->next != nil)
		m->next->prev = m;
	return m+1;
}

void *
Hunk_Mark(void)
{
	return hunk_head;
}

void
Hunk_FreeToMark(void *mark)
{
	mem_t *m;

	while(hunk_head != mark){
		m = hunk_head->next;
		free(hunk_head);
		hunk_head = m;
	}
}

void *
Hunk_TempAlloc(int size)
{
	static void	*buf;
	static int bufsz;

	if(bufsz > size)
		return buf;
	buf = realloc(buf, size);
	if(buf == nil)
		sysfatal("Hunk_TempAlloc: %r");
	bufsz = size;

	return buf;
}

void *
Cache_Check(mem_user_t *c)
{
	return c->data;
}

static void
Cache_Flush(void)
{
	mem_t *s;
		
	while(cache_head != nil){
		s = cache_head->next;
		cache_head->user->data = nil;
		free(cache_head);
		cache_head = s;
	}
}

void
Cache_Free(mem_user_t *c)
{
	mem_t *cs;

	if(!c->data)
		fatal("Cache_Free: not allocated");

	cs = ((mem_t *)c->data) - 1;
	if(cs == cache_head)
		cache_head = cs->next;
	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = nil;

	c->data = nil;
	free(cs);
}

void *
Cache_Alloc(mem_user_t *c, int size)
{
	mem_t *cs;

	if(c->data)
		fatal("Cache_Alloc: allready allocated");
	if(size <= 0)
		fatal("Cache_Alloc: size %d", size);

	cs = mallocz(sizeof(*cs) + size, 1);
	if(cs == nil)
		sysfatal("Cache_Alloc: %r");
	cs->size = size;
	cs->next = cache_head;
	if(cache_head != nil)
		cache_head->prev = cs;
	cache_head = cs;
	c->data = cs+1;
	cs->user = c;
	
	return c->data;
}

void
Memory_Init(void)
{
	Cmd_AddCommand("flush", Cache_Flush);
}
