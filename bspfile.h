// upper design bounds
enum {
	MAX_MAP_HULLS = 4,
	MAX_MAP_MODELS = 256,
	MAX_MAP_BRUSHES = 4096,
	MAX_MAP_ENTSTRING = 65536,
	MAX_MAP_PLANES = 32767,
	MAX_MAP_NODES = 32767, // because negative shorts are contents
	MAX_MAP_CLIPNODES = 32767, // same here
	MAX_MAP_VERTS = 65535,
	MAX_MAP_FACES = 65535,
	MAX_MAP_MARKSURFACES = 65535,
	MAX_MAP_TEXINFO = 4096,
	MAX_MAP_EDGES = 256000,
	MAX_MAP_SURFEDGES = 512000,
	MAX_MAP_TEXTURES = 512,
	MAX_MAP_MIPTEX = 0x200000,
	MAX_MAP_LIGHTING = 0x100000,
	MAX_MAP_VISIBILITY = 0x100000,
	MAX_MAP_PORTALS = 65536,

	// key / value pair sizes
	MAX_KEY = 32,
	MAX_VALUE = 1024,

	MIPLEVELS = 4,
	MAXLIGHTMAPS = 4,
};

//=============================================================================

#define BSPVERSION	29
#define BSP2VERSION ('B'|'S'<<8|'P'<<16|'2'<<24)
#define	TOOLVERSION	2

#ifdef __plan9__
#pragma pack on
#else
#pragma pack(1)
#endif

typedef struct
{
	int		fileofs, filelen;
} lump_t;

enum {
	LUMP_ENTITIES,
	LUMP_PLANES,
	LUMP_TEXTURES,
	LUMP_VERTEXES,
	LUMP_VISIBILITY,
	LUMP_NODES,
	LUMP_TEXINFO,
	LUMP_FACES,
	LUMP_LIGHTING,
	LUMP_CLIPNODES,
	LUMP_LEAFS,
	LUMP_MARKSURFACES,
	LUMP_EDGES,
	LUMP_SURFEDGES,
	LUMP_MODELS,
	HEADER_LUMPS,
};

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLS];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} dmodel_t;

typedef struct
{
	int			version;
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	int			nummiptex;
	int			dataofs[4];		// [nummiptex]
} dmiptexlump_t;

typedef struct miptex_s
{
	char		name[16];
	unsigned	width, height;
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
} miptex_t;

typedef struct
{
	float	point[3];
} dvertex_t;

enum {
	// 0-2 are axial planes
	PLANE_X,
	PLANE_Y,
	PLANE_Z,
	// 3-5 are non-axial planes snapped to the nearest
	PLANE_ANYX,
	PLANE_ANYY,
	PLANE_ANYZ,
};

typedef struct
{
	float	normal[3];
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

enum {
	CONTENTS_CURRENT_DOWN = -14,
	CONTENTS_CURRENT_UP,
	CONTENTS_CURRENT_270,
	CONTENTS_CURRENT_180,
	CONTENTS_CURRENT_90,
	CONTENTS_CURRENT_0,
	CONTENTS_CLIP, // changed to contents_solid
	CONTENTS_ORIGIN, // removed at csg time
	CONTENTS_SKY,
	CONTENTS_LAVA,
	CONTENTS_SLIME,
	CONTENTS_WATER,
	CONTENTS_SOLID,
	CONTENTS_EMPTY,
};

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
	int			planenum;
	short		children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for sphere culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} dnode_t;

typedef struct
{
	int			planenum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	float		mins[3];		// for sphere culling
	float		maxs[3];
	unsigned int	firstface;
	unsigned int	numfaces;	// counting both sides
} bsp2_dnode_t;

typedef struct
{
	int			planenum;
	short		children[2];	// negative numbers are contents
} dclipnode_t;

typedef struct
{
	int		planenum;
	int		children[2];	// negative numbers are contents
} bsp2_dclipnode_t;

enum {
	TEX_SPECIAL = 1<<0, // sky or slime, no lightmap or 256 subdivision
};

typedef struct texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			miptex;
	int			flags;
} texinfo_t;

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
	unsigned short	v[2];		// vertex numbers
} dedge_t;

typedef struct
{
	unsigned int v[2];
} bsp2_dedge_t;

typedef struct
{
	short		planenum;
	short		side;

	int			firstedge;		// we must support > 64k edges
	short		numedges;
	short		texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} dface_t;

typedef struct
{
	int		planenum;
	int		side;

	int			firstedge;		// we must support > 64k edges
	int		numedges;
	int		texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} bsp2_dface_t;

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct
{
	int			contents;
	int			visofs;				// -1 = no visibility info

	short		mins[3];			// for frustum culling
	short		maxs[3];

	unsigned short		firstmarksurface;
	unsigned short		nummarksurfaces;

	byte		ambient_level[Namb];
} dleaf_t;

typedef struct
{
	int			contents;
	int			visofs;				// -1 = no visibility info

	float		mins[3];			// for frustum culling
	float		maxs[3];

	unsigned int		firstmarksurface;
	unsigned int		nummarksurfaces;

	byte		ambient_level[Namb];
} bsp2_dleaf_t;

#ifdef __plan9__
#pragma pack off
#else
#pragma pack(0)
#endif
