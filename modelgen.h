// modelgen.h: header file for model generation program

#define ALIAS_VERSION	6

#ifdef __plan9__
#pragma pack on
#endif

typedef enum {
	ALIAS_SINGLE,
	ALIAS_GROUP,
}aliasframetype_t;

typedef enum {
	ALIAS_SKIN_SINGLE,
	ALIAS_SKIN_GROUP,
}aliasskintype_t;

typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;
} mdl_t;

// TODO: could be shorts

typedef struct {
	int		onseam;
	int		s;
	int		t;
} stvert_t;

typedef struct dtriangle_s {
	int					facesfront;
	int					vertindex[3];
} dtriangle_t;

#define DT_FACES_FRONT				0x0010

// This mirrors trivert_t in trilib.h, is present so Quake knows how to
// load this data

typedef struct {
	byte	v[3];
	byte	lightnormalindex;
} trivertx_t;

typedef struct {
	trivertx_t	bboxmin;	// lightnormal isn't used
	trivertx_t	bboxmax;	// lightnormal isn't used
	char		name[16];	// frame name from grabbing
} daliasframe_t;

typedef struct {
	int			numframes;
	trivertx_t	bboxmin;	// lightnormal isn't used
	trivertx_t	bboxmax;	// lightnormal isn't used
} daliasgroup_t;

typedef struct {
	int			numskins;
} daliasskingroup_t;

typedef struct {
	float	interval;
} daliasinterval_t;

typedef struct {
	float	interval;
} daliasskininterval_t;

typedef struct {
	aliasframetype_t	type;
} daliasframetype_t;

typedef struct {
	aliasskintype_t	type;
} daliasskintype_t;

#define IDPOLYHEADER	(('O'<<24)+('P'<<16)+('D'<<8)+'I')
// little-endian "IDPO"

#ifdef __plan9__
#pragma pack off
#endif
