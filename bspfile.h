#define BSPVERSION 29
#define BSP2VERSION ('B'|'S'<<8|'P'<<16|'2'<<24)

enum {
	// upper design bounds
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

	MIPLEVELS = 4,
	MAXLIGHTMAPS = 4,

	LUMP_ENTITIES = 0,
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

	// 0-2 are axial planes
	PLANE_X = 0,
	PLANE_Y,
	PLANE_Z,
	// 3-5 are non-axial planes snapped to the nearest
	PLANE_ANYX,
	PLANE_ANYY,
	PLANE_ANYZ,

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

	TEX_SPECIAL = 1<<0, // sky or slime, no lightmap or 256 subdivision
};
