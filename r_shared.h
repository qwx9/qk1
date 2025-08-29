#pragma once

// general refresh-related stuff shared between the refresh and the
// driver

typedef s32int uzint;

enum {
	Enfog = 1 << 0,
	Enskyfog = 1 << 1,
};

typedef struct fogvars_t fogvars_t;

struct fogvars_t {
	pixel_t pix, fbpix, skypix;
	float density;
	byte c0, c1, c2;
	bool allowed, enabled;
};

extern fogvars_t fogvars;

#define isfogged() (fogvars.pix != 0)

// FIXME: clean up and move into d_iface.h

enum {
	MAXVERTS = 16, // max points in a surface polygon
	MAXWORKINGVERTS = (MAXVERTS + 4), // max points in an intermediate polygon (while processing)
	MAXWIDTH = 3840,
	MAXHEIGHT = 2160,
	MAXDIMENSION = ((MAXHEIGHT > MAXWIDTH) ? MAXHEIGHT : MAXWIDTH),
	SIN_BUFFER_SIZE = (MAXDIMENSION + TURB_CYCLE),

	// NOTE: these are only initial values. limits are supposed to scale up dynamically
	MAXEDGES = 2400,
	MAXSURFACES = 800,
	MAXSPANS = 3000,
};

extern float pixelAspect;
extern int r_drawnpolycount;
extern cvar_t r_clearcolor;
extern int sintable[SIN_BUFFER_SIZE];
extern int intsintable[SIN_BUFFER_SIZE];
extern entity_t *currententity;

typedef struct espan_s {
	struct espan_s *pnext;
	u16int v, u, count;
} espan_t;

typedef struct surf_s {
	int key; // sorting key (BSP order)
	s16int last_u; // set during tracing

	// 0 = not in span
	// 1 = in span
	// -1 = in inverted span (end before start)
	s16int spanstate;

	struct surf_s *next; // active surface stack in r_edge.c
	struct surf_s *prev; // used in r_edge.c for active surf stack
	struct espan_s *spans; // pointer to linked list of spans to draw
	void *data; // associated data like msurface_t
	entity_t *entity;
	u16int flags;
	s16int miplevel;
	float d_ziorigin;
	float d_zistepu;
	float d_zistepv;
} surf_t;

extern int r_numallocatedbasespans;
extern void *r_basespans;
extern surf_t *surfaces, *surface_p, *surf_max;

// surfaces are generated in back to front order by the bsp, so if a surf
// pointer is greater than another one, it should be drawn in front
// surfaces[1] is the background, and is used as the active surface stack.
// surfaces[0] is a dummy, because index 0 is used to indicate no surface
// attached to an edge_t

extern vec3_t sxformaxis[4]; // s axis transformed into viewspace
extern vec3_t txformaxis[4]; // t axis transformed into viewspac

extern float xcenter, ycenter;
extern float xscale, yscale;
extern float xscaleinv, yscaleinv;
extern float xscaleshrink, yscaleshrink;

extern int d_lightstylevalue[256]; // 8.8 frac of base light value

// flags in finalvert_t.flags
enum {
	ALIAS_LEFT_CLIP = 0x0001,
	ALIAS_TOP_CLIP = 0x0002,
	ALIAS_RIGHT_CLIP = 0x0004,
	ALIAS_BOTTOM_CLIP = 0x0008,
	ALIAS_Z_CLIP = 0x0010,
	ALIAS_ONSEAM = 0x0020, // also defined in modelgen.h - must be kept in sync
	ALIAS_XY_CLIP_MASK = 0x000F,
};

typedef struct edge_s {
	u16int surfs[2];
	fixed16_t u;
	fixed16_t u_step;
	float nearzi;
	struct edge_s *prev, *next;
	struct edge_s *nextremove;
	medge_t *owner;
} edge_t;
