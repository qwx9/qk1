// r_shared.h: general refresh-related stuff shared between the refresh and the
// driver

typedef s32int uzint;

enum {
	Enfog = 1<<0,
	Enskyfog = 1<<1,
};

typedef struct fogvars_t fogvars_t;

struct fogvars_t {
	pixel_t pix, skypix;
	float density;
	byte c0, c1, c2;
	bool allowed, enabled;
};

extern fogvars_t fogvars;

#define isfogged() (fogvars.pix != 0)

// FIXME: clean up and move into d_iface.h

#define	MAXVERTS	16					// max points in a surface polygon
#define MAXWORKINGVERTS	(MAXVERTS+4)	// max points in an intermediate
										//  polygon (while processing)
// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define	MAXWIDTH		3840
#define	MAXHEIGHT		2160
#define MAXDIMENSION	((MAXHEIGHT > MAXWIDTH) ? MAXHEIGHT : MAXWIDTH)

#define SIN_BUFFER_SIZE	(MAXDIMENSION+CYCLE)

//===================================================================

extern	float	pixelAspect;

extern int		r_drawnpolycount;

extern cvar_t	r_clearcolor;

extern int	sintable[SIN_BUFFER_SIZE];
extern int	intsintable[SIN_BUFFER_SIZE];

extern	entity_t		*currententity;

// NOTE: these are only initial values. limits are supposed to scale up dynamically
#define	MAXEDGES			2400
#define MAXSURFACES			800
#define	MAXSPANS			3000

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct espan_s
{
	struct espan_s *pnext;
	u16int v, u, count;
} espan_t;

typedef struct surf_s
{
	int			key;				// sorting key (BSP order)
	s16int		last_u;				// set during tracing
	s16int		spanstate;			// 0 = not in span
									// 1 = in span
									// -1 = in inverted span (end before
									//  start)
	struct surf_s	*next;			// active surface stack in r_edge.c
	struct surf_s	*prev;			// used in r_edge.c for active surf stack
	struct espan_s	*spans;			// pointer to linked list of spans to draw
	void		*data;				// associated data like msurface_t
	entity_t	*entity;
	int			flags;
	float		nearzi;				// nearest 1/z on surface, for mipmapping
	float		d_ziorigin;
	float		d_zistepu;
	float		d_zistepv;
} surf_t;

extern int r_numallocatedbasespans;
extern void *r_basespans;

extern	surf_t	*surfaces, *surface_p, *surf_max;

// surfaces are generated in back to front order by the bsp, so if a surf
// pointer is greater than another one, it should be drawn in front
// surfaces[1] is the background, and is used as the active surface stack.
// surfaces[0] is a dummy, because index 0 is used to indicate no surface
//  attached to an edge_t

//===================================================================

extern vec3_t	sxformaxis[4];	// s axis transformed into viewspace
extern vec3_t	txformaxis[4];	// t axis transformed into viewspac

extern	float	xcenter, ycenter;
extern	float	xscale, yscale;
extern	float	xscaleinv, yscaleinv;
extern	float	xscaleshrink, yscaleshrink;

extern	int d_lightstylevalue[256]; // 8.8 frac of base light value

extern void SetUpForLineScan(fixed8_t startvertu, fixed8_t startvertv,
	fixed8_t endvertu, fixed8_t endvertv);

// flags in finalvert_t.flags
#define ALIAS_LEFT_CLIP				0x0001
#define ALIAS_TOP_CLIP				0x0002
#define ALIAS_RIGHT_CLIP			0x0004
#define ALIAS_BOTTOM_CLIP			0x0008
#define ALIAS_Z_CLIP				0x0010
// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define ALIAS_ONSEAM				0x0020	// also defined in modelgen.h;
											//  must be kept in sync
#define ALIAS_XY_CLIP_MASK			0x000F

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct edge_s
{
	u16int	surfs[2];
	fixed16_t		u;
	fixed16_t		u_step;
	float			nearzi;
	struct edge_s	*prev, *next;
	struct edge_s	*nextremove;
	medge_t			*owner;
} edge_t;
