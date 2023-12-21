#include "quakedef.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Warray-bounds"
#include "stb_image_resize2.h"
#pragma GCC diagnostic pop

void
pixels_resize(pixel_t *in, pixel_t *out, int iw, int ih, int ow, int oh, bool premul, bool fence)
{
	stbir_resize(
		(byte*)in, iw, ih, iw*sizeof(pixel_t),
		(byte*)out, ow, oh, ow*sizeof(pixel_t),
		(premul || fence) ? STBIR_RGBA_PM : STBIR_RGBA,
		STBIR_TYPE_UINT8_SRGB,
		STBIR_EDGE_CLAMP,
		fence ? STBIR_FILTER_BOX : STBIR_FILTER_MITCHELL
	);
}
