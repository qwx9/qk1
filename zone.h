typedef struct
{
	void *data;
}mem_user_t;

#define Z_Free(p) free(p)
#define Z_Malloc(sz) calloc(1, (sz))

int Hunk_From(void *p);
void Hunk_CacheFrom(mem_user_t *c, void *p);
void *Hunk_Alloc(int size);
void *Hunk_Double(void *p);
void *Hunk_Mark(void);
void Hunk_FreeToMark(void *mark);
void *Hunk_TempAlloc(int size);

void *Cache_Alloc(mem_user_t *c, int size);
void *Cache_Realloc(mem_user_t *c, int size);
void *Cache_Check(mem_user_t *c);
void Cache_Free(mem_user_t *c);

void Memory_Init(void);
