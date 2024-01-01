static inline pixel_t
addlight(pixel_t x, int lr, int lg, int lb)
{
	int r, g, b;

	if(fullbright && (x & 0xff000000U) == 0)
		return x;

	if(additive)
		return x;

	r = (x>>16) & 0xff;
	g = (x>>8)  & 0xff;
	b = (x>>0)  & 0xff;

	r = (r * ((64<<8)-(lr & 0xffff))) >> (8+VID_CBITS);
	g = (g * ((64<<8)-(lg & 0xffff))) >> (8+VID_CBITS);
	b = (b * ((64<<8)-(lb & 0xffff))) >> (8+VID_CBITS);
	x = (x & 0xff000000) | r<<16 | g<<8 | b<<0;

	return x;
}
