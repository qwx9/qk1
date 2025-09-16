#pragma once

// public interface to refresh functions

enum {
	MAXCLIPPLANES = 11,

	TOP_RANGE = 16, // soldier uniform colors
	BOTTOM_RANGE = 96,
};

typedef struct efrag_s {
	struct mleaf_s *leaf;
	struct efrag_s *leafnext;
	struct entity_s *entity;
	struct efrag_s *entnext;
} efrag_t;

typedef struct entity_s {
	struct model_s *model; // NULL = no model
	struct efrag_s *efrag; // linked list of efrags
	pixel_t *colormap;
	struct entity_s *last_reject;

	// for bmodels, first world node that splits bmodel, or NULL if not split
	struct mnode_s *topnode;

	double msgtime; // time of last update
	int frame;
	float syncbase; // for client-side animations
	int effects; // light, particals, etc
	int skinnum; // for Alias models
	int visframe; // last frame this entity was found in an active leaf

	int dlightframe; // dynamic lighting
	int dlightbits;

	int update_type;
	int trivial_accept;
	byte alpha;
	bool forcelink; // model changed

	entity_state_t baseline; // to fill in defaults in updates
	vec3_t msg_origins[2]; // last two updates (0 is newest)
	vec3_t origin;
	vec3_t msg_angles[2]; // last two updates (0 is newest)
	vec3_t angles;
} entity_t;

typedef struct {
	vrect_t vrect; // subwindow in video for refresh
	// FIXME: not need vrect next field here?
	vrect_t aliasvrect; // scaled Alias version
	int vrectright, vrectbottom; // right & bottom screen coords
	int aliasvrectright, aliasvrectbottom; // scaled Alias versions
	float fvrectx, fvrecty; // for floating-point compares
	float fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int vrect_x_adj_shift20; // (vrect.x + 0.5 - epsilon) << 20
	int vrectright_adj_shift20; // (vrectright + 0.5 - epsilon) << 20
	float fvrectright_adj, fvrectbottom_adj; // right and bottom edges, for clamping
	float fvrectright; // rightmost edge, for Alias clamping
	float fvrectbottom; // bottommost edge, for Alias clamping
	float horizontalFieldOfView; // at Z = 1.0, this many X is visible; 2.0 = 90 degrees
	float xOrigin; // should probably always be 0.5
	float yOrigin; // between be around 0.3 to 0.5

	view_t view;
	float fov_x, fov_y;

	int ambientlight[3];
} refdef_t;

// refresh

extern refdef_t r_refdef;

extern struct texture_s *r_notexture_mip;

void R_MissingTexture(struct texture_s *t);
void R_Init(void);
void R_InitTextures(void);
void R_InitEfrags(void);
void R_RenderView(void); // must set r_refdef first
void R_ViewChanged(vrect_t *pvrect, float pixelAspect); // called whenever r_refdef or vid change

void R_InitSky(struct texture_s *mt); // called at level load
void R_LoadSkyBox(const char *name, struct model_s *m);
void R_EmitSkyBox(view_t *v);

void R_AddEfrags(entity_t *ent);
void R_RemoveEfrags(entity_t *ent);

void R_NewMap(void);

void R_ParseParticleEffect(void);
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail(vec3_t start, vec3_t end, int type);
void R_EntityParticles(entity_t *ent);
void R_BlobExplosion(vec3_t org);
void R_ParticleExplosion(vec3_t org);
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength);
void R_LavaSplash(vec3_t org);
void R_TeleportSplash(vec3_t org);

void R_PushDlights(void);

// surface cache related

int D_SurfaceCacheForRes(int width, int height);
void D_FlushCaches(void);
void D_DeleteSurfaceCache(void);
void D_InitCaches(void *buffer, int size);
