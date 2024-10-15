#include "quakedef.h"

typedef struct mem_t mem_t;

struct mem_t
{
	mem_t *prev, *next;
	mem_user_t *user;
	int tag;
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
	assert(n != nil && (void*)(n+1) == p);
	data = Cache_Alloc(c, size);
	setmalloctag((mem_t*)data-1, getcallerpc(&c));

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

	assert(size >= 0);
	m = calloc(1, sizeof(*m) + size);
	if(m == nil)
		fatal("Hunk_Alloc: size=%d: %s", size, lerr());
	setmalloctag(m, getcallerpc(&size));
	m->size = size;
	m->next = hunk_head;
	if(hunk_head != nil)
		hunk_head->prev = m;
	hunk_head = m;

	return m+1;
}

void
Hunk_SetTag(void *p, int tag)
{
	mem_t *m;

	assert(p != nil);
	m = p;
	m--;
	m->tag = tag;
}

int
Hunk_GetTag(void *p)
{
	mem_t *m;

	assert(p != nil);
	m = p;
	m--;
	return m->tag;
}

void *
Hunk_Double(void *p)
{
	mem_t *m, *n;

	assert(p != nil);
	m = p;
	m--;
	n = realloc(m, sizeof(*m) + m->size*2);
	if(m == nil)
		fatal("Hunk_Double: %s", lerr());
	if(hunk_head == m)
		hunk_head = n;
	m = n;
	setrealloctag(m, getcallerpc(&p));
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

void
Hunk_Free(void *p)
{
	mem_t *m;

	if(p == nil)
		return;
	m = p;
	m--;

	if(m->prev != nil)
		m->prev->next = m->next;
	if(m->next != nil)
		m->next->prev = m->prev;
	if(m == hunk_head)
		hunk_head = m->next;
	free(m);
}

void *
Hunk_TempAlloc(int size)
{
	static void	*buf;
	static int bufsz = -1;

	assert(size >= 0);
	if(size > bufsz){
		buf = realloc(buf, size);
		if(buf == nil)
			fatal("Hunk_TempAlloc: %s", lerr());
		setmalloctag(buf, getcallerpc(&size));
		bufsz = size;
	}
	memset(buf, 0, size);

	return buf;
}

void *
Cache_Check(mem_user_t *c)
{
	return c->data;
}

static void
Cache_Flush(cmd_t *c)
{
	mem_t *s;

	USED(c);
	while(cache_head != nil){
		s = cache_head->next;
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

	cs = (mem_t *)c->data - 1;
	if(cs == cache_head)
		cache_head = cs->next;
	if(cs->prev != nil)
		cs->prev->next = cs->next;
	if(cs->next != nil)
		cs->next->prev = cs->prev;

	c->data = nil;
	free(cs);
}

void *
Cache_Alloc(mem_user_t *c, int size)
{
	mem_t *cs;

	if(c->data != nil)
		fatal("Cache_Alloc: already allocated");
	if(size <= 0)
		fatal("Cache_Alloc: size %d", size);

	cs = calloc(1, sizeof(*cs) + size);
	if(cs == nil)
		fatal("Cache_Alloc: %s", lerr());
	setmalloctag(cs, getcallerpc(&c));
	cs->size = size;
	cs->next = cache_head;
	if(cache_head != nil)
		cache_head->prev = cs;
	cache_head = cs;
	cs->user = c;
	c->data = cs+1;

	return c->data;
}

void *
Cache_Realloc(mem_user_t *c, int size)
{
	mem_t *cs, *o;

	if(c->data == nil)
		return Cache_Alloc(c, size);
	if(size <= 0)
		fatal("Cache_Alloc: size %d", size);

	cs = (mem_t *)c->data - 1;
	o = cs;
	cs = realloc(cs, sizeof(*cs) + size);
	if(cs == nil)
		fatal("Cache_Realloc: %s", lerr());
	if(cache_head == o)
		cache_head = cs;
	if(cs->prev != nil)
		cs->prev->next = cs;
	if(cs->next != nil)
		cs->next->prev = cs;
	setrealloctag(cs, getcallerpc(&c));
	if(size > cs->size)
		memset((byte*)(cs+1) + cs->size, 0, size - cs->size);
	cs->size = size;
	cs->user = c;
	c->data = cs+1;

	return c->data;
}

void
Memory_Init(void)
{
	Cmd_AddCommand("flush", Cache_Flush);
}
