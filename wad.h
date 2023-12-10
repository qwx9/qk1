typedef struct Wad Wad;
#pragma incomplete Wad

typedef struct
{
	int width;
	int height;
	pixel_t pixels[];
}qpic_t;

struct texture_s;

Wad *W_OpenWad(char *path);
qpic_t *W_ReadQpic(Wad *w, char *name, mem_user_t *c);
int W_ReadMipTex(Wad *wad, char *name, struct texture_s *t);
int W_ReadPixels(Wad *w, char *name, pixel_t *out, int num);
int W_ReadRaw(Wad *w, char *name, byte *out, int num);
