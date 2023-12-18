#include "quakedef.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_ASSERT(x) assert(x)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Warray-bounds"
#include "stb_image_resize2.h"
#pragma GCC diagnostic pop

void
pixels_resize(pixel_t *in, pixel_t *out, int iw, int ih, int ow, int oh, bool premulalpha)
{
	stbir_resize_uint8_srgb(
		(byte*)in, iw, ih, iw*sizeof(pixel_t),
		(byte*)out, ow, oh, ow*sizeof(pixel_t),
		premulalpha ? STBIR_RGBA_PM : STBIR_RGBA
	);
}
