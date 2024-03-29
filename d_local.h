// d_local.h:  private rasterization driver defs

//
// TODO: fine-tune this; it's based on providing some overage even if there
// is a 2k-wide scan, with subdivision every 8, for 256 spans of 12 bytes each
//
#define SCANBUFFERPAD		0x1000

#define R_SKY_SMASK	0x007F0000
#define R_SKY_TMASK	0x007F0000

#define DS_SPAN_LIST_END	-128

#define SURFCACHE_SIZE_AT_320X200	600*1024

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;
	int					size;		// including header
	unsigned			width;
	unsigned			height;		// DEBUG only needed for debug
	float				mipscale;
	struct texture_s	*texture;	// checked for animating textures
	byte				data[4];	// width*height elements
} surfcache_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct sspan_s
{
	int				u, v, count;
} sspan_t;

typedef u16int uzint;

extern cvar_t	d_subdiv16;

extern float	scale_for_mip;

extern bool		d_roverwrapped;
extern surfcache_t	*sc_rover;
extern surfcache_t	*d_initial_rover;

extern float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
extern float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
extern float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

extern fixed16_t	sadjust, tadjust;
extern fixed16_t	bbextents, bbextentt;

void D_DrawSpans16 (espan_t *pspans, int forceblend, byte alpha);
void D_DrawZSpans (espan_t *pspans);
void Turbulent8 (espan_t *pspan, byte alpha);

void D_DrawSkyScans8 (espan_t *pspan);

void R_ShowSubDiv (void);
surfcache_t	*D_CacheSurface (msurface_t *surface, int miplevel);

extern uzint *d_pzbuffer;
extern unsigned int d_zrowbytes, d_zwidth;

extern int	*d_pscantable;
extern int	d_scantable[MAXHEIGHT];

extern int	d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;

extern int d_pix_min, d_pix_max;
extern pixel_t *d_viewbuffer;
extern uzint *zspantable[MAXHEIGHT];
extern int d_minmip;
extern float d_scalemip[3];

enum {
	// perhaps a bit too much, but looks ok
	AlphaStep = 2,
};

extern byte *alphamap[256>>AlphaStep];

#define blendalpha(a, b, alpha) \
	alphamap[alpha>>AlphaStep][(u16int)((a)<<8 | (b))]

void initalpha(void);
float alphafor(int flags);
