#include "quakedef.h"

static int iskyspeed = 8;
static int iskyspeed2 = 2;
static float skyspeed2;

/* a lot of sky logic originally comes from super8/ToChriS */

static const int planes[6][2] = {
	{2, -128},
	{0, -128},
	{2, +128},
	{1, +128},
	{0, +128},
	{1, -128},
};

static mtexinfo_t r_skytexinfo[6];
static mplane_t r_skyplanes[6];
static mvertex_t *r_skyverts;
static msurface_t *r_skysurface;

bool r_skybox = false;

void
R_EmitSkyBox(view_t *v)
{
	static const vec3_t verts[8] = {
		{-128, -128, -128},
		{-128, +128, -128},
		{+128, +128, -128},
		{+128, -128, -128},
		{-128, -128, +128},
		{-128, +128, +128},
		{+128, -128, +128},
		{+128, +128, +128},
	};
	int i, oldkey;

	for(i = 0; i < nelem(verts); i++)
		VectorAdd(v->org, verts[i], r_skyverts[i].position);
	for(i = 0; i < nelem(planes); i++) {
		r_skyplanes[i].dist = v->org[planes[i][0]] + planes[i][1];
		r_skytexinfo[i].vecs[0][3] = -DotProduct(v->org, r_skytexinfo[i].vecs[0]);
		r_skytexinfo[i].vecs[1][3] = -DotProduct(v->org, r_skytexinfo[i].vecs[1]);
	}

	oldkey = r_currentkey;
	r_currentkey = Q_MAXINT - 0xf;
	for(i = 0; i < nelem(planes); i++)
		R_RenderFace(r_skysurface + i, v, ALIAS_XY_CLIP_MASK);
	r_currentkey = oldkey;
}

void
R_LoadSkyBox(const char *name, model_t *m)
{
	static const vec3_t vecs[6][2] = {
		{{0, -1, 0}, {-1, 0, 0}},
		{{0, 1, 0}, {0, 0, -1}},
		{{0, -1, 0}, {1, 0, 0}},
		{{1, 0, 0}, {0, 0, -1}},
		{{0, -1, 0}, {0, 0, -1}},
		{{-1, 0, 0}, {0, 0, -1}},
	};
	static const medge_t edges[12] = {
		{{1, 2}},
		{{2, 3}},
		{{3, 4}},
		{{4, 1}},
		{{1, 5}},
		{{5, 6}},
		{{6, 2}},
		{{7, 8}},
		{{8, 6}},
		{{5, 7}},
		{{8, 3}},
		{{7, 4}},
	};
	static const int surfedges[6 * 4] = {
		1, 2, 3, 4, -1, 5, 6, 7, 8, 9, -6, 10, -2, -7, -9, 11, 12, -3, -11, -8, -12, -10, -5, -4,
	};
	static const int flags[6] = {0, 0, SURF_PLANEBACK, SURF_PLANEBACK, SURF_PLANEBACK, 0};
	static const char *suffix[6] = {"dn", "lf", "up", "bk", "rt", "ft"};
	texture_t *t, *ts[6];
	msurface_t *s;
	medge_t *e;
	char *path;
	int i, *se;

	if(r_skybox) {
		for(i = 0; i < nelem(ts); i++) {
			if(r_skytexinfo[i].texture != r_notexture_mip)
				Hunk_Free(r_skytexinfo[i].texture);
		}
		m->numvertexes -= 8;
		m->numsurfedges -= nelem(surfedges);
		m->numsurfaces -= nelem(ts);
		m->numedges -= nelem(edges);
		r_skybox = false;
	}

	if(name != nil && name[0] == 0)
		return;
	if(name == nil && (name = Mod_FindInWorldspawn(m, "sky")) == nil &&
	   (name = Mod_FindInWorldspawn(m, "skyname")) == nil)
		return;

	r_skyverts = m->vertexes + m->numvertexes;
	r_skysurface = s = m->surfaces + m->numsurfaces;

	for(i = 0; i < nelem(ts); i++, s++) {
		path = va("gfx/env/%s%s", name, suffix[i]);
		if((t = Load_ExternalTexture(nil, path, true)) == nil) {
			Con_Printf("R_SetSkyBox: failed to load %s%s\n", name, suffix[i]);
			t = r_notexture_mip;
		}
		ts[i] = t;
		r_skytexinfo[i].texture = t;
		VectorScale(vecs[i][0], (float)t->width / 256.0f, r_skytexinfo[i].vecs[0]);
		VectorScale(vecs[i][1], (float)t->height / 256.0f, r_skytexinfo[i].vecs[1]);
		s->texinfo = &r_skytexinfo[i];
		s->plane = &r_skyplanes[i];
		s->numedges = 4;
		s->firstedge = m->numsurfedges + i * 4;
		s->flags = flags[i] | SURF_DRAWSKYBOX;
		s->extents[0] = t->width;
		s->extents[1] = t->height;
		s->texturemins[0] = -t->width / 2;
		s->texturemins[1] = -t->height / 2;
		r_skyplanes[i].normal[planes[i][0]] = 1;
	}

	se = m->surfedges + m->numsurfedges;
	for(i = 0; i < nelem(surfedges); i++, se++) {
		if(surfedges[i] > 0)
			*se = m->numedges - 1 + surfedges[i];
		else
			*se = -(m->numedges - 1 - surfedges[i]);
	}

	e = m->edges + m->numedges;
	for(i = 0; i < nelem(edges); i++, e++) {
		e->v[0] = m->numvertexes - 1 + edges[i].v[0];
		e->v[1] = m->numvertexes - 1 + edges[i].v[1];
		e->cacheoffset = 0;
	}

	m->numvertexes += 8;
	m->numsurfedges += nelem(surfedges);
	m->numsurfaces += nelem(ts);
	m->numedges += nelem(edges);
	r_skybox = true;
}

static void
skyrecalc(void)
{
	skyvars.smask = (skyvars.w - 1) << 16;
	skyvars.tmask = (skyvars.h - 1) << 16;
	skyvars.tshift = 16 - qctz(skyvars.w);
}

void
R_InitSky(texture_t *mt)
{
	int x, y, w, n;
	pixel_t *src, p;

	r_skybox = false;
	if(mt == nil) {
		skyvars.w = r_notexture_mip->width;
		skyvars.h = r_notexture_mip->height;
		skyvars.source[0] = skyvars.source[1] = r_notexture_mip->pixels;
	} else {
		src = mt->pixels + mt->offsets[0];
		w = mt->width;
		skyvars.h = mt->height;
		skyvars.w = w / 2;
		n = skyvars.w * skyvars.h;
		skyvars.source[0] = Hunk_Alloc(n * sizeof(pixel_t) * 2);
		skyvars.source[1] = skyvars.source[0] + n;
		for(y = 0; y < skyvars.h; y++) {
			for(x = 0; x < skyvars.w; x++) {
				skyvars.source[0][y * skyvars.w + x] = src[y * w + skyvars.w + x];
				if((p = src[y * w + x]) & 0xffffff)
					skyvars.source[1][y * skyvars.w + x] = p;
			}
		}
	}
	skyrecalc();
}

void
R_SetSkyFrame(void)
{
	int g, s1, s2;
	float temp;

	skyvars.speed = iskyspeed;
	skyspeed2 = iskyspeed2;

	g = GreatestCommonDivisor(iskyspeed, iskyspeed2);
	s1 = iskyspeed / g;
	s2 = iskyspeed2 / g;
	temp = skyvars.h * s1 * s2;

	skyvars.time = cl.time - ((int)(cl.time / temp) * temp);
}
