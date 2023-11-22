// spritegn.h: header file for sprite generation program

enum {
	SPRITE_VERSION = 1,

	SPR_VP_PARALLEL_UPRIGHT = 0,
	SPR_FACING_UPRIGHT,
	SPR_VP_PARALLEL,
	SPR_ORIENTED,
	SPR_VP_PARALLEL_ORIENTED,
};

typedef enum {
	SPR_SINGLE,
	SPR_GROUP,
}spriteframetype_t;

// TODO: shorten these?
typedef struct {
	int			ident;
	int			version;
	int			type;
	float		boundingradius;
	int			width;
	int			height;
	int			numframes;
	float		beamlength;
	synctype_t	synctype;
} dsprite_t;

typedef struct {
	int			origin[2];
	int			width;
	int			height;
} dspriteframe_t;

typedef struct {
	int			numframes;
} dspritegroup_t;

typedef struct {
	float	interval;
} dspriteinterval_t;

typedef struct {
	spriteframetype_t	type;
} dspriteframetype_t;

#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I')
// little-endian "IDSP"
