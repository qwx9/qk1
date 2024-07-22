#include "quakedef.h"
#include "hsluv.h"

#define transform() \
	do { \
		if(sa != 1.0 || li != 1.0){ \
			rgb2hsluv(r, g, b, &h, &s, &l); \
			s *= sa; \
			if(s > 100.0) \
				s = 100.0; \
			l *= li; \
			if(l > 100.0) \
				l = 100.0; \
			hsluv2rgb(h, s, l, &r, &g, &b); \
		} \
		if(br != 0){ \
			r *= br; \
			g *= br; \
			b *= br; \
		} \
		if(r > 1) r = 1; else if(r < 0) r = 0; \
		if(g > 1) g = 1; else if(g < 0) g = 0; \
		if(b > 1) b = 1; else if(b < 0) b = 0; \
	}while(0)

void
LightTransform(u8int *p, int n)
{
	double h, s, l, r, g, b, sa, li, br;
	int i;

	sa = v_saturation.value;
	li = v_lightness.value;
	br = v_brightness.value;
	if(sa == 1.0 && li == 1.0 && br == 1.0)
		return;

	for(i = 0; i < n; i++, p += 3){
		r = (double)p[0]/255.0;
		g = (double)p[1]/255.0;
		b = (double)p[2]/255.0;
		transform();
		p[0] = r*255;
		p[1] = g*255;
		p[2] = b*255;
	}
}

void
PixTransform(pixel_t *pixels, int n)
{
	double h, s, l, r, g, b, sa, li, br;
	pixel_t p, in, out;
	int i;

	sa = v_saturation.value;
	li = v_lightness.value;
	br = v_brightness.value;
	if(sa == 1.0 && li == 1.0 && br == 1.0)
		return;

	in = out = 0;
	for(i = 0; i < n; i++){
		p = pixels[i];
		if(p == 0)
			continue;
		if(p == in && i > 0){
			pixels[i] = out;
			continue;
		}

		in = p;
		r = (double)((p>>16) & 0xff)/255.0;
		g = (double)((p>> 8) & 0xff)/255.0;
		b = (double)((p>> 0) & 0xff)/255.0;
		transform();
		out =
			(in & (0xff<<24)) |
			(pixel_t)(r * 255)<<16 |
			(pixel_t)(g * 255)<< 8 |
			(pixel_t)(b * 255)<< 0;
		pixels[i] = out;
	}
}
