int TGA_Encode(byte **out, char *id, pixel_t *pix, int w, int h);
qpic_t *TGA_Decode(byte *in, int sz, bool *premultalpha);
