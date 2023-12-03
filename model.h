typedef enum {ST_SYNC, ST_RAND} synctype_t;

#include "modelgen.h"
#include "spritegn.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vec3_t		position;
} mvertex_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


// plane_t structure
// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct mplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for texture axis selection and fast side tests
	byte	signbits;		// signx + signy<<1 + signz<<1
	byte	pad[2];
} mplane_t;

typedef struct texture_s
{
	char name[16];
	int width, height;
	int anim_total;				// total tenths in sequence ( 0 = no)
	int anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s *anim_next;		// in the animation sequence
	struct texture_s *alternate_anims;	// bmodels in frmae 1 use these
	int offsets[MIPLEVELS];		// four mip maps stored
	pixel_t data[];
} texture_t;

enum {
	SURF_PLANEBACK = 1<<1,
	SURF_DRAWSKY = 1<<2,
	SURF_DRAWSPRITE = 1<<3,
	SURF_DRAWTURB = 1<<4,
	SURF_DRAWTILED = 1<<5,
	SURF_DRAWBACKGROUND = 1<<6,
	SURF_TRANS = 1<<8,
	SURF_FENCE = 1<<9,
	SURF_LAVA = 1<<10,
	SURF_SLIME = 1<<11,
	SURF_TELE = 1<<12,
};

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	unsigned int	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct
{
	float		vecs[2][4];
	float		mipadjust;
	texture_t	*texture;
	int			flags;
} mtexinfo_t;

typedef struct msurface_s
{
	mplane_t	*plane;
	mtexinfo_t	*texinfo;
	byte		*samples;		// [numstyles*surfsize]

// surface generation data
	struct surfcache_s	*cachespots[MIPLEVELS];

	int		visframe;		// should be drawn when node is crossed
	int		dlightframe;
	int		dlightbits;
	int		flags;
	int		firstedge;	// look up in model->surfedges[], negative numbers
	int		numedges;	// are backwards edges
	int		texturemins[2];
	int		extents[2];

// lighting info
	byte		styles[MAXLIGHTMAPS];
} msurface_t;

typedef struct mnode_s
{
// common with leaf
	int			contents;		// 0, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];

	unsigned int		firstsurface;
	unsigned int		numsurfaces;
} mnode_t;



typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// leaf specific
	byte		*compressed_vis;
	efrag_t		*efrags;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
	int			key;			// BSP sequence number for leaf's contents
	byte		ambient_sound_level[Namb];
} mleaf_t;

typedef struct
{
	int planenum;
	int children[2];
}mclipnode_t;

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
	mclipnode_t	*clipnodes;
	mplane_t	*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s
{
	int		width;
	int		height;
	void	*pcachespot;			// remove?
	float	up, down, left, right;
	pixel_t	pixels[];
} mspriteframe_t;

typedef struct
{
	int				numframes;
	float			*intervals;
	mspriteframe_t	*frames[];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t	type;
	union {
		mspriteframe_t *frameptr;
		mspritegroup_t *framegrp;
	};
} mspriteframedesc_t;

typedef struct
{
	int					type;
	float				boundingradius;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	float				beamlength;		// remove?
	void				*cachespot;		// remove?
	mspriteframedesc_t	frames[];
} msprite_t;


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

typedef struct
{
	aliasframetype_t	type;
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct
{
	aliasskintype_t		type;
	void				*pcachespot;
	int					skin;
} maliasskindesc_t;

typedef struct
{
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
} maliasgroupframedesc_t;

typedef struct
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

typedef struct
{
	int					numskins;
	int					intervals;
	maliasskindesc_t	skindescs[1];
} maliasskingroup_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;

typedef struct {
	int					model;
	int					stverts;
	int					skindesc;
	int					triangles;
	maliasframedesc_t	frames[1];
} aliashdr_t;

//===================================================================

//
// Whole model
//

typedef enum {
	mod_brush,
	mod_sprite,
	mod_alias,
} modtype_t;

enum {
	EF_ROCKET = 1<<0, // leave a trail
	EF_GRENADE = 1<<1, // leave a trail
	EF_GIB = 1<<2, // leave a trail
	EF_ROTATE = 1<<3, // rotate (bonus items)
	EF_TRACER = 1<<4, // green split trail
	EF_ZOMGIB = 1<<5, // small blood trail
	EF_TRACER2 = 1<<6, // orange split trail + rotate
	EF_TRACER3 = 1<<7, // purple trail
};

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLS];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} submodel_t;

typedef struct model_s
{
	char		name[Npath];
	int	needload;		// bmodels and sprites don't cache normally
	bool	blend;

	int ver;
	int numwads;
	Wad **wads;

	modtype_t	type;
	int			numframes;
	synctype_t	synctype;

	int			flags;

//
// volume occupied by the model
//
	vec3_t		mins, maxs;
	float		radius;

//
// brush model
//
	int			firstmodelsurface, nummodelsurfaces;

	int			numsubmodels;
	submodel_t	*submodels;

	int			numplanes;
	mplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			numclipnodes;
	mclipnode_t	*clipnodes;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	int			numtextures;
	texture_t	**textures;

	byte		*visdata;
	byte		*lightdata;
	char		*entities;

//
// additional model data
//
	mem_user_t	cache;		// only access through Mod_Extradata

} model_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (char *name, bool crash);
void	*Mod_Extradata (model_t *mod);	// handles caching
void	Mod_TouchModel (char *name);

mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model);
byte	*Mod_LeafPVS (mleaf_t *leaf, model_t *model, int *sz);
void	Mod_Print(void);
