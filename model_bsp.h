void BSP_MakeHull0(model_t *mod);
void BSP_SetParent(mnode_t *node, mnode_t *parent);
int BSP_CalcSurfaceExtents(model_t *mod, msurface_t *s);

int BSP_LoadClipnodes(model_t *mod, byte *in, int sz);
int BSP_LoadEdges(model_t *mod, byte *in, int sz);
int BSP_LoadEntities(model_t *mod, byte *in, int sz);
int BSP_LoadFaces(model_t *mod, byte *in, int sz);
int BSP_LoadLeafs(model_t *mod, byte *in, int sz);
int BSP_LoadLighting(model_t *mod, byte *in, int sz);
int BSP_LoadMarksurfaces(model_t *mod, byte *in, int sz);
int BSP_LoadNodes(model_t *mod, byte *in, int sz);
int BSP_LoadPlanes(model_t *mod, byte *in, int sz);
int BSP_LoadSubmodels(model_t *mod, byte *in, int sz);
int BSP_LoadSurfedges(model_t *mod, byte *in, int sz);
int BSP_LoadTexinfo(model_t *mod, byte *in, int sz);
int BSP_LoadTextures(model_t *mod, byte *in, int sz);
int BSP_LoadVertexes(model_t *mod, byte *in, int sz);
int BSP_LoadVisibility(model_t *mod, byte *in, int sz);

int BSP2_LoadClipnodes(model_t *mod, byte *in, int sz);
int BSP2_LoadEdges(model_t *mod, byte *in, int sz);
int BSP2_LoadFaces(model_t *mod, byte *in, int sz);
int BSP2_LoadLeafs(model_t *mod, byte *in, int sz);
int BSP2_LoadMarksurfaces(model_t *mod, byte *in, int sz);
int BSP2_LoadNodes(model_t *mod, byte *in, int sz);

int BSP30_LoadEntities(model_t *mod, byte *in, int sz);
int BSP30_LoadLighting(model_t *mod, byte *in, int sz);
int BSP30_LoadFaces(model_t *mod, byte *in, int sz);
int BSP30_LoadTextures(model_t *mod, byte *in, int sz);
