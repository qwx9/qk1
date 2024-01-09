#define _GNU_SOURCE
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <limits.h>
#include "fast_barrier.h"
#include "quakedef.h"

float scale_for_mip;

// FIXME: should go away
void R_RotateBmodel(entity_t *entity, view_t *v);

static int
D_MipLevelForScale(float scale)
{
	int lmiplevel;

	if(scale >= d_scalemip[0])
		lmiplevel = 0;
	else if(scale >= d_scalemip[1])
		lmiplevel = 1;
	else if(scale >= d_scalemip[2])
		lmiplevel = 2;
	else
		lmiplevel = 3;

	return max(d_minmip, lmiplevel);
}

static void
D_DrawSolidSurface(surf_t *surf, pixel_t color, int first, int end)
{
	espan_t *span;
	pixel_t *pdest;
	uzint *pz;
	int u, u2;

	for(span = surf->spans; span; span=span->pnext){
		if(span->v < first || span->v >= end)
			continue;
		pdest = dvars.fb + span->v*dvars.w;
		pz = dvars.zb + span->v*dvars.w;
		memset(pz, 0xfe, span->count*sizeof(*pz));
		u2 = span->u + span->count - 1;
		for(u = span->u; u <= u2; u++)
			pdest[u] = color;
	}
}


static void
D_CalcGradients(int miplevel, msurface_t *pface, vec3_t transformed_modelorg, view_t *v, texvars_t *tv)
{
	vec3_t p_temp1, p_saxis, p_taxis;
	float mipscale;
	float t;

	mipscale = 1.0 / (float)(1 << miplevel);

	TransformVector(pface->texinfo->vecs[0], p_saxis, v);
	TransformVector(pface->texinfo->vecs[1], p_taxis, v);

	t = xscaleinv * mipscale;

	tv->s.divz.stepu = p_saxis[0] * t;
	tv->t.divz.stepu = p_taxis[0] * t;

	t = yscaleinv * mipscale;
	tv->s.divz.stepv = -p_saxis[1] * t;
	tv->t.divz.stepv = -p_taxis[1] * t;

	tv->s.divz.origin = p_saxis[2] * mipscale - xcenter * tv->s.divz.stepu - ycenter * tv->s.divz.stepv;
	tv->t.divz.origin = p_taxis[2] * mipscale - xcenter * tv->t.divz.stepu - ycenter * tv->t.divz.stepv;

	VectorScale(transformed_modelorg, mipscale, p_temp1);

	t = 0x10000*mipscale;
	tv->s.adjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
			((pface->texturemins[0] << 16) >> miplevel)
			+ pface->texinfo->vecs[0][3]*t;
	tv->t.adjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
			((pface->texturemins[1] << 16) >> miplevel)
			+ pface->texinfo->vecs[1][3]*t;

	// -1 (-epsilon) so we never wander off the edge of the texture
	tv->s.bbextent = ((pface->extents[0] << 16) >> miplevel) - 1;
	tv->t.bbextent = ((pface->extents[1] << 16) >> miplevel) - 1;
}

static fast_barrier_t spansgobrr, spansgohome;
static pthread_spinlock_t spancache;

typedef struct span_thread_t span_thread_t;

struct span_thread_t {
	pthread_t tid;
	int n;
	int first;
	int end;
};

static int nthreads = 8;
static bool spawned = false;
static view_t *v0;

static void
spancachelock(int n)
{
	(n ? pthread_spin_lock : pthread_spin_unlock)(&spancache);
}

static void
spannothread(view_t *v0, int first, int end)
{
	vec3_t local_modelorg, transformed_modelorg, world_transformed_modelorg;
	surfcache_t *pcurrentcache;
	drawsurf_t ds = {0};
	msurface_t *pface;
	int miplevel;
	entity_t *e;
	texvars_t t;
	byte alpha;
	bool blend;
	surf_t *s;
	view_t v;
	espan_t *sp;
	bool yes;

	///uvlong t0 = nanosec();

	memmove(&v, v0, sizeof(v));
	TransformVector(v.modelorg, transformed_modelorg, &v);
	VectorCopy(transformed_modelorg, world_transformed_modelorg);

	// TODO: could preset a lot of this at mode set time
	for(s = surfaces+1; s < surface_p; s++){
		e = s->entity;
		if(!s->spans || ((surfdrawflags(s->flags) | entdrawflags(e)) ^ r_drawflags))
			continue;

		yes = false;
		for(sp = s->spans; sp != nil; sp = sp->pnext){
			if(sp->v >= first && sp->v < end){
				yes = true;
				break;
			}
		}
		if(!yes)
			continue;

		alpha = 255;
		if(enthasalpha(e) && e->alpha != 255)
			alpha = e->alpha;
		else if(s->flags & SURF_TRANS)
			alpha *= alphafor(s->flags);
		if(alpha < 1)
			alpha = 255;

		t.z.stepu = s->d_zistepu;
		t.z.stepv = s->d_zistepv;
		t.z.origin = s->d_ziorigin;

		if(insubmodel(s)){
			VectorSubtract(v.org, e->origin, local_modelorg);
			TransformVector(local_modelorg, transformed_modelorg, &v);
			R_RotateBmodel(e, &v);
		}

		pface = s->data;
		if(s->flags & SURF_DRAWSKY){
			D_DrawSkyScans8(s->spans, first, end);
		}else if(s->flags & SURF_DRAWBACKGROUND){
			D_DrawSolidSurface(s, q1pal[(int)r_clearcolor.value & 0xFF], first, end);
		}else if(s->flags & SURF_DRAWTURB){
			t.p = pface->texinfo->texture->pixels;
			t.w = 64;
			D_CalcGradients(0, pface, transformed_modelorg, &v, &t);
			D_DrawSpans(s->spans, &t, alpha, SPAN_TURB, first, end);
		}else{
			miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip * pface->texinfo->mipadjust);
			if(s->flags & SURF_FENCE)
				miplevel = max(miplevel-1, 0);

			pcurrentcache = D_CacheSurface(s->entity, pface, &ds, miplevel, spancachelock);
			t.p = pcurrentcache->pixels;
			t.w = pcurrentcache->width;
			D_CalcGradients(miplevel, pface, transformed_modelorg, &v, &t);
			blend = (s->flags & SURF_FENCE) || (r_drawflags & DRAW_BLEND);
			D_DrawSpans(s->spans, &t, alpha,
				(alpha == 255 && (s->flags & SURF_FENCE))
					? SPAN_FENCE
					: (blend ? SPAN_BLEND : SPAN_SOLID),
				first,
				end
			);
		}

		if(insubmodel(s)){
			VectorCopy(world_transformed_modelorg, transformed_modelorg);
			memmove(&v, v0, sizeof(v));
		}
	}

	///uvlong t1 = nanosec();
	///if(first != 0 || end != vid.height)
	///	fprintf(stderr, "@%d %llu\n", 0, t1-t0);
}

static void *
spanthread(void *th_)
{
	vec3_t local_modelorg, transformed_modelorg, world_transformed_modelorg;
	surfcache_t *pcurrentcache;
	span_thread_t *th = th_;
	drawsurf_t ds = {0};
	msurface_t *pface;
	int miplevel, ns;
	entity_t *e;
	texvars_t t;
	byte alpha;
	bool blend;
	surf_t *s;
	espan_t *sp;
	bool yes;
	view_t v;

	for(;;){
		fast_barrier_wait(&spansgobrr);

		//uvlong t0 = nanosec();
		memmove(&v, v0, sizeof(v));
		TransformVector(v.modelorg, transformed_modelorg, &v);
		VectorCopy(transformed_modelorg, world_transformed_modelorg);
		ns = 0;

		// TODO: could preset a lot of this at mode set time
		for(s = surfaces+1; s < surface_p; s++){
			e = s->entity;
			if(!s->spans || ((surfdrawflags(s->flags) | entdrawflags(e)) ^ r_drawflags))
				continue;
			yes = false;
			for(sp = s->spans; sp != nil; sp = sp->pnext){
				if(sp->v >= th->first && sp->v < th->end){
					yes = true;
					break;
				}
			}
			if(!yes)
				continue;
			ns++;
			alpha = 255;
			if(enthasalpha(e) && e->alpha != 255)
				alpha = e->alpha;
			else if(s->flags & SURF_TRANS)
				alpha *= alphafor(s->flags);
			if(alpha < 1)
				alpha = 255;

			t.z.stepu = s->d_zistepu;
			t.z.stepv = s->d_zistepv;
			t.z.origin = s->d_ziorigin;

			if(insubmodel(s)){
				VectorSubtract(v.org, e->origin, local_modelorg);
				TransformVector(local_modelorg, transformed_modelorg, &v);
				R_RotateBmodel(e, &v);
			}

			pface = s->data;
			if(s->flags & SURF_DRAWSKY){
				D_DrawSkyScans8(s->spans, th->first, th->end);
			}else if(s->flags & SURF_DRAWBACKGROUND){
				D_DrawSolidSurface(s, q1pal[(int)r_clearcolor.value & 0xFF], th->first, th->end);
			}else if(s->flags & SURF_DRAWTURB){
				t.p = pface->texinfo->texture->pixels;
				t.w = 64;
				D_CalcGradients(0, pface, transformed_modelorg, &v, &t);
				D_DrawSpans(s->spans, &t, alpha, SPAN_TURB, th->first, th->end);
			}else{
				miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip * pface->texinfo->mipadjust);
				if(s->flags & SURF_FENCE)
					miplevel = max(miplevel-1, 0);

				pcurrentcache = D_CacheSurface(s->entity, pface, &ds, miplevel, spancachelock);
				t.p = pcurrentcache->pixels;
				t.w = pcurrentcache->width;
				D_CalcGradients(miplevel, pface, transformed_modelorg, &v, &t);
				blend = (s->flags & SURF_FENCE) || (r_drawflags & DRAW_BLEND);
				D_DrawSpans(s->spans, &t, alpha,
					(alpha == 255 && (s->flags & SURF_FENCE))
						? SPAN_FENCE
						: (blend ? SPAN_BLEND : SPAN_SOLID),
					th->first,
					th->end
				);
			}

			if(insubmodel(s)){
				VectorCopy(world_transformed_modelorg, transformed_modelorg);
				memmove(&v, v0, sizeof(v));
			}
		}

		///uvlong t1 = nanosec();
		///fprintf(stderr, "@%d %llu\n", th->n, t1-t0);
		//fprintf(stderr, "!%d %d\n", th->n, ns);
		fast_barrier_wait(&spansgohome);
	}

	return nil;
}

static span_thread_t *threads;

void
D_DrawSurfaces(view_t *v0_)
{
	static int lastheight = -1;
	span_thread_t *t;
	int i, split, dt, n, y;

	if(lastheight < 0)
		pthread_spin_init(&spancache, PTHREAD_PROCESS_PRIVATE);

	if(nthreads > 1 && threads == nil){
		pthread_barrierattr_t battr;
		cpu_set_t set;

		pthread_barrierattr_setpshared(&battr, PTHREAD_PROCESS_PRIVATE);
		fast_barrier_init(&spansgobrr, &battr, nthreads);
		fast_barrier_init(&spansgohome, &battr, nthreads);

		threads = calloc(1, sizeof(*threads) * nthreads);
		for(t = threads, i = 0; i < nthreads; i++, t++){
			t->n = i;
			CPU_ZERO(&set);
			CPU_SET(2*i, &set);
			if(i == 0){
				sched_setaffinity(getpid(), sizeof(set), &set);
			}else{
				pthread_create(&t->tid, nil, spanthread, t);
				pthread_setaffinity_np(t->tid, sizeof(set), &set);
			}
		}
		spawned = true;
	}
	if(threads != nil && lastheight != vid.height){
		lastheight = vid.height;
		split = (nthreads+2)*nthreads/8;
		dt = vid.height/2 / split;
		n = dt*nthreads/2;
		y = 0;
		for(t = threads, i = 0; i < nthreads; i++, t++){
			t->first = y;
			t->end = y = y + n;
			if((n -= dt) == 0){
				dt = -dt;
				n = -dt;
			}
			///fprintf(stderr, "# %d: %d...%d\n", i, t->first, t->end);
		}
		t[-1].end = vid.height;
	}

	v0 = v0_;
	if(nthreads < 2 || (r_drawflags & DRAW_BLEND) != 0){
		// overhead (lots of small objects + synchronization)
		// not worth it - run it all in the same thread
		spannothread(v0, 0, vid.height);
	}else{
		///uvlong t0 = nanosec();
		fast_barrier_wait(&spansgobrr);
		///uvlong t1 = nanosec();
		spannothread(v0, threads[0].first, threads[0].end);
		///uvlong t2 = nanosec();
		fast_barrier_wait(&spansgohome);
		///uvlong t3 = nanosec();
		///fprintf(stderr, "---------- total=%llu start_barrier=%llu end_barrier=%llu\n", t3-t0, t1-t0, t3-t2);
	}
}
