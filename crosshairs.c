#include "quakedef.h"

#define X 0xffffffffU
#define o 0

static const qpic_t c_cross = {
	.width = 5,
	.height = 5,
	.pixels = {
		o, o, X, o, o, //
		o, o, X, o, o, //
		X, X, X, X, X, //
		o, o, X, o, o, //
		o, o, X, o, o, //
	},
};

static const qpic_t c_dot = {
	.width = 5,
	.height = 5,
	.pixels = {
		o, o, o, o, o, //
		o, o, o, o, o, //
		o, o, X, o, o, //
		o, o, o, o, o, //
		o, o, o, o, o, //
	},
};

static const qpic_t c_x = {
	.width = 5,
	.height = 5,
	.pixels = {
		X, o, o, o, X, //
		o, X, o, X, o, //
		o, o, X, o, o, //
		o, X, o, X, o, //
		X, o, o, o, X, //
	},
};

const qpic_t *crosshairs[3] = {
	&c_cross,
	&c_dot,
	&c_x,
};
