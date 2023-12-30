// d_local.h:  private rasterization driver defs

enum {
	R_SKY_SMASK = 0x007F0000,
	R_SKY_TMASK	= 0x007F0000,

	DS_SPAN_LIST_END = -128,

	SURFCACHE_SIZE_AT_320X200 = 600*1024,
};

typedef struct {
	pixel_t *cacheblock;
	pixel_t *viewbuffer;
	uzint *zbuffer;
	unsigned cachewidth;
	unsigned zwidth;
	fixed16_t sadjust, tadjust;
	fixed16_t bbextents, bbextentt;
	float sdivzstepu, tdivzstepu, zistepu;
	float sdivzstepv, tdivzstepv, zistepv;
	float sdivzorigin, tdivzorigin, ziorigin;
}dvars_t;

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;
	int					size;		// including header
	int					width;
	int					height;		// DEBUG only needed for debug
	float				mipscale;
	struct texture_s	*texture;	// checked for animating textures
	pixel_t				pixels[];	// width*height elements
} surfcache_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct sspan_s
{
	int				u, v, count;
} sspan_t;

extern cvar_t	d_subdiv16;

extern float	scale_for_mip;

extern bool		d_roverwrapped;
extern surfcache_t	*sc_rover;
extern surfcache_t	*d_initial_rover;

extern dvars_t dvars;

void D_DrawSpans16 (espan_t *pspans, bool blend, byte alpha);
void D_DrawZSpans (espan_t *pspans);
void Turbulent8 (espan_t *pspan, byte alpha);

void D_DrawSkyScans8 (espan_t *pspan);

void R_ShowSubDiv (void);
surfcache_t	*D_CacheSurface (msurface_t *surface, int miplevel);

extern int	*d_pscantable;
extern int	d_scantable[MAXHEIGHT];

extern int	d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;

extern int d_pix_min, d_pix_max;
extern uzint *zspantable[MAXHEIGHT];
extern int d_minmip;
extern float d_scalemip[3];

pixel_t blendalpha(pixel_t ca, pixel_t cb, int alpha, uzint izi);
float alphafor(int flags);
