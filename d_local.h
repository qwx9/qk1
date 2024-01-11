// d_local.h:  private rasterization driver defs

enum {
	DS_SPAN_LIST_END = -128,

	SURFCACHE_SIZE_AT_320X200 = 600*1024,
};

typedef struct {
	struct {
		float stepv;
		float stepu;
		float origin;
	}divz;
	fixed16_t adjust;
	fixed16_t bbextent;
}tavars_t;

typedef struct {
	struct {
		float origin;
		float stepv;
		float stepu;
	}z;
	unsigned w;
	pixel_t *p;
	tavars_t s, t;
}texvars_t;

typedef struct {
	pixel_t *fb;
	uzint *zb;
	unsigned w;
}dvars_t;

typedef struct {
	pixel_t *source[2];
	int w, h;
	int smask, tmask;
	int tshift;
	float speed, time;
}skyvars_t;

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	struct texture_s	*texture;	// checked for animating textures
	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;
	int					size;		// including header
	int					width;
	int					height;		// DEBUG only needed for debug
	float				mipscale;
	pixel_t				pixels[];	// width*height elements
} surfcache_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct sspan_s
{
	int				u, v, count;
} sspan_t;

extern cvar_t	d_subdiv16;

extern bool		d_roverwrapped;
extern surfcache_t	*sc_rover;
extern surfcache_t	*d_initial_rover;

extern dvars_t dvars;
extern skyvars_t skyvars;

enum {
	SPAN_BLEND,
	SPAN_FENCE,
	SPAN_SOLID,
	SPAN_TURB,
};

void D_DrawSpans(espan_t *pspan, texvars_t *t, byte alpha, int spanfunc);

void D_DrawSkyScans8 (espan_t *pspan);

surfcache_t	*D_CacheSurface(entity_t *e, msurface_t *ms, drawsurf_t *ds, int miplevel);

extern int	*d_pscantable;
extern int	d_scantable[MAXHEIGHT];

extern int	d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;

extern int d_pix_min, d_pix_max;
extern uzint *zspantable[MAXHEIGHT];
extern int d_minmip;
extern float d_scalemip[3];

static inline pixel_t
mulalpha(pixel_t ca, int alpha)
{
	pixel_t a, b;

	a = ((ca >> 0) & 0xff00ff)*alpha + 0x800080;
	a = (a + ((a >> 8) & 0xff00ff)) >> 8;
	a &= 0x00ff00ff;

	b = ((ca >> 8) & 0xff00ff)*alpha + 0x800080;
	b = (b + ((b >> 8) & 0xff00ff)) >> 0;
	b &= 0xff00ff00;

	return a | b;
}

pixel_t blendadditive(pixel_t ca, pixel_t cb, int alpha);
pixel_t blendalpha(pixel_t ca, pixel_t cb, int alpha);
float alphafor(int flags);
