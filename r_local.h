// r_local.h -- private refresh defs

#include "r_shared.h"

enum {
	DRAW_BLEND = 1<<0,
	DRAW_NO = 1<<1,
};

int surfdrawflags(int flags);
int entdrawflags(entity_t *e);
#define enthasalpha(e) ((e) && !defalpha((e)->alpha))

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle

#define BMODEL_FULLY_CLIPPED	0x10 // value returned by R_BmodelCheckBBox ()
									 //  if bbox is trivially rejected

//===========================================================================
// viewmodel lighting

typedef struct {
	int			ambientlight[3];
	int			shadelight[3];
	float		*plightvec;
} alight_t;

//===========================================================================
// clipped bmodel edges

typedef struct bedge_s
{
	mvertex_t		*v[2];
	struct bedge_s	*pnext;
} bedge_t;

typedef struct {
	float	fv[3];		// viewspace x, y
} auxvert_t;

//===========================================================================

extern cvar_t	r_clearcolor;
extern cvar_t	r_waterwarp;
extern cvar_t	r_fullbright;
extern cvar_t	r_ambient;
extern cvar_t	r_reportsurfout;
extern cvar_t	r_numsurfs;
extern cvar_t	r_reportedgeout;
extern cvar_t	r_numedges;
extern cvar_t	r_part_scale;
extern cvar_t	r_wateralpha;
extern cvar_t	r_lavaalpha;
extern cvar_t	r_slimealpha;

#define XCENTERING	(1.0 / 2.0)
#define YCENTERING	(1.0 / 2.0)

#define CLIP_EPSILON		0.001

#define BACKFACE_EPSILON	0.01

//===========================================================================

#define	DIST_NOT_SET	98765

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct clipplane_s
{
	vec3_t		normal;
	float		dist;
	struct		clipplane_s	*next;
	byte		leftedge;
	byte		rightedge;
	byte		reserved[2];
} clipplane_t;

extern	clipplane_t	view_clipplanes[4];

//=============================================================================

void R_RenderWorld (void);
void R_RenderWorldRejects(void);

//=============================================================================

extern	mplane_t	screenedge[4];

extern	vec3_t	r_origin;

extern	vec3_t	r_entorigin;

extern	float	screenAspect;
extern	float	xOrigin, yOrigin;

extern	int		r_visframecount;

//=============================================================================

extern int r_drawflags;

void R_DrawFog(void);
void R_InitFog(void);

//
// current entity info
//
extern	bool		insubmodel;

void R_DrawSprite (void);
int R_RenderFace (msurface_t *fa, int clipflags);
void R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf);
void R_TransformFrustum (void);
void R_SetSkyFrame (void);
texture_t *R_TextureAnimation (texture_t *base);

pixel_t addlight(pixel_t x, int lr, int lg, int lb);

void R_DrawSubmodelPolygons (model_t *pmodel, int clipflags);
void R_DrawSolidClippedSubmodelPolygons (model_t *pmodel);

void R_AddPolygonEdges (emitpoint_t *pverts, int numverts, int miplevel);
surf_t *R_GetSurf (void);
void R_AliasDrawModel (alight_t *plighting);
void R_BeginEdgeFrame (void);
void R_ScanEdges (void);
void D_DrawSurfaces (void);
void R_InsertNewEdges (edge_t *edgestoadd, edge_t *edgelist);
void R_StepActiveU (edge_t *pedge);
void R_RemoveEdges (edge_t *pedge);

extern void R_Surf8Start (void);
extern void R_Surf8End (void);
extern void R_Surf16Start (void);
extern void R_Surf16End (void);
extern void R_EdgeCodeStart (void);
extern void R_EdgeCodeEnd (void);

extern void R_RotateBmodel (void);

extern int	c_faceclip;
extern int	r_polycount;

extern	model_t		*cl_worldmodel;

extern int		*pfrustum_indexes[4];

// !!! if this is changed, it must be changed in asm_draw.h too !!!
#define	NEAR_CLIP	0.01

extern fixed16_t	sadjust, tadjust;
extern fixed16_t	bbextents, bbextentt;

#define MAXBVERTINDEXES	1000	// new clipped vertices when clipping bmodels
								//  to the world BSP
extern mvertex_t	*r_ptverts, *r_ptvertsmax;

extern int		r_currentkey;
extern int		r_currentbkey;

void	R_InitTurb (void);

//=========================================================
// Alias models
//=========================================================

#define MAXALIASVERTS		2000	// TODO: tune this
#define ALIAS_Z_CLIP_PLANE	5

#define NUMVERTEXNORMALS	162
extern const float r_avertexnormals[NUMVERTEXNORMALS][3];

extern finalvert_t		*pfinalverts;

bool R_AliasCheckBBox (void);

//=========================================================
// turbulence stuff

#define	AMP		8*0x10000
#define	AMP2	3
#define	SPEED	20

//=========================================================
// particle stuff

void R_DrawParticles (void);
void R_InitParticles (void);
void R_ClearParticles (void);

extern edge_t	*auxedges;
extern int		r_numallocatededges;
extern edge_t	*r_edges, *edge_p, *edge_max;

extern	edge_t	*newedges[MAXHEIGHT];
extern	edge_t	*removeedges[MAXHEIGHT];

extern	int	screenwidth;

// FIXME: make stack vars when debugging done
extern int		r_bmodelactive;

extern float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;
extern float		r_aliastransition, r_resfudge;

extern int		r_outofsurfaces;
extern int		r_outofedges;
extern int		r_outofspans;

extern mvertex_t	*r_pcurrentvertbase;
extern int			r_maxvalidedgeoffset;

void R_AliasClipTriangle (mtriangle_t *ptri, auxvert_t *auxverts);

extern int		r_frustum_indexes[4*6];
extern int		r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
extern bool	r_surfsonstack;
extern bool	r_dowarpold, r_viewchanged;

extern mleaf_t	*r_viewleaf, *r_oldviewleaf;

extern vec3_t	r_emins, r_emaxs;
extern mnode_t	*r_pefragtopnode;
extern int		r_clipflags;
extern int		r_dlightframecount;

void R_StoreEfrags (efrag_t **ppefrag);
void R_AnimateLight (void);
void R_LightPoint (vec3_t p, int *r);
void R_SetupFrame (void);
void R_cshift_f (void);
void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1);
void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip);
void R_SplitEntityOnNode2 (mnode_t *node);
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);
