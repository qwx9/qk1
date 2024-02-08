#include "quakedef.h"

/*
===============
R_CheckVariables
===============
*/
static void
R_CheckVariables(void)
{
	static float	oldbright;

	if (r_fullbright.value != oldbright)
	{
		oldbright = r_fullbright.value;
		D_FlushCaches ();	// so all lighting changes
	}
}

/*
===================
R_TransformFrustum
===================
*/
void
R_TransformFrustum(view_t *view)
{
	int		i;
	vec3_t	v, v2;

	for (i=0 ; i<4 ; i++)
	{
		v[0] = screenedge[i].normal[2];
		v[1] = -screenedge[i].normal[0];
		v[2] = screenedge[i].normal[1];

		v2[0] = v[1]*view->right[0] + v[2]*view->up[0] + v[0]*view->pn[0];
		v2[1] = v[1]*view->right[1] + v[2]*view->up[1] + v[0]*view->pn[1];
		v2[2] = v[1]*view->right[2] + v[2]*view->up[2] + v[0]*view->pn[2];

		VectorCopy(v2, view->clipplanes[i].normal);

		view->clipplanes[i].dist = DotProduct(view->modelorg, v2);
	}
}

/*
===============
R_SetUpFrustumIndexes
===============
*/
static void
R_SetUpFrustumIndexes(view_t *v)
{
	int		i, j, *pindex;

	pindex = r_frustum_indexes;

	for (i=0 ; i<4 ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			if (v->clipplanes[i].normal[j] < 0)
			{
				pindex[j] = j;
				pindex[j+3] = j+3;
			}
			else
			{
				pindex[j] = j+3;
				pindex[j+3] = j;
			}
		}

		// FIXME: do just once at start
		pfrustum_indexes[i] = pindex;
		pindex += 6;
	}
}


/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int				edgecount;
	vrect_t			vrect;
	float			w, h;

	// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		setcvar ("r_fullbright", "0");
		setcvar ("r_ambient", "0");
	}

	if (r_numsurfs.value)
	{
		if ((surface_p - surfaces) > r_maxsurfsseen)
			r_maxsurfsseen = surface_p - surfaces;

		Con_Printf (va("Used %zd of %zd surfs; %d max\n", surface_p - surfaces,
				surf_max - surfaces, r_maxsurfsseen));
	}

	if (r_numedges.value)
	{
		edgecount = edge_p - r_edges;

		if (edgecount > r_maxedgesseen)
			r_maxedgesseen = edgecount;

		Con_Printf ("Used %d of %d edges; %d max\n", edgecount,
				r_numallocatededges, r_maxedgesseen);
	}

	r_refdef.ambientlight[0] = max(0, r_ambient.value);
	r_refdef.ambientlight[1] = max(0, r_ambient.value);
	r_refdef.ambientlight[2] = max(0, r_ambient.value);

	R_CheckVariables ();

	R_AnimateLight ();

	r_framecount++;

/* debugging
r_refdef.view.org[0]=  80;
r_refdef.view.org[1]=      64;
r_refdef.view.org[2]=      40;
r_refdef.view.angles[0]=    0;
r_refdef.view.angles[1]=    46.763641357;
r_refdef.view.angles[2]=    0;
*/

	// build the transformation matrix for the given view angles
	VectorCopy(r_refdef.view.org, r_refdef.view.modelorg);
	AngleVectors(r_refdef.view.angles, r_refdef.view.pn, r_refdef.view.right, r_refdef.view.up);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf(r_refdef.view.org, cl.worldmodel);

	r_dowarpold = r_dowarp;
	r_dowarp = r_waterwarp.value && (r_viewleaf->contents <= CONTENTS_WATER);

	if ((r_dowarp != r_dowarpold) || r_viewchanged || lcd_x.value)
	{
		if (r_dowarp)
		{
			if ((vid.width <= vid.maxwarpwidth) &&
				(vid.height <= vid.maxwarpheight))
			{
				vrect.x = 0;
				vrect.y = 0;
				vrect.width = vid.width;
				vrect.height = vid.height;

				R_ViewChanged (&vrect, sb_lines, vid.aspect);
			}
			else
			{
				w = vid.width;
				h = vid.height;

				if (w > vid.maxwarpwidth)
				{
					h *= (float)vid.maxwarpwidth / w;
					w = vid.maxwarpwidth;
				}

				if (h > vid.maxwarpheight)
				{
					h = vid.maxwarpheight;
					w *= (float)vid.maxwarpheight / h;
				}

				vrect.x = 0;
				vrect.y = 0;
				vrect.width = (int)w;
				vrect.height = (int)h;

				R_ViewChanged (&vrect,
							   (int)((float)sb_lines * (h/(float)vid.height)),
							   vid.aspect * (h / w) *
								 ((float)vid.width / (float)vid.height));
			}
		}
		else
		{
			vrect.x = 0;
			vrect.y = 0;
			vrect.width = vid.width;
			vrect.height = vid.height;

			R_ViewChanged (&vrect, sb_lines, vid.aspect);
		}

		r_viewchanged = false;
	}

	r_cache_thrash = false;
	R_TransformFrustum(&r_refdef.view); // start off with just the four screen edge clip planes
	R_SetSkyFrame();
	R_SetUpFrustumIndexes(&r_refdef.view);
	D_SetupFrame();
}
