#pragma once

typedef struct Wad Wad;
#pragma incomplete Wad

struct texture_s;

Wad *W_OpenWad(const char *path);
qpic_t *W_ReadQpic(Wad *w, const char *name, mem_user_t *c);
int W_ReadMipTex(Wad *w, const char *name, struct texture_s *t);
int W_ReadPixels(Wad *w, const char *name, pixel_t *out, int num);
int W_ReadRaw(Wad *w, const char *name, byte *out, int num);
