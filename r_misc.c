#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

/*
===============
R_CheckVariables
===============
*/
void R_CheckVariables (void)
{
	static float	oldbright;

	if (r_fullbright.value != oldbright)
	{
		oldbright = r_fullbright.value;
		D_FlushCaches ();	// so all lighting changes
	}
}

void WarpPalette (void)
{
	int		i,j;
	byte	newpalette[768];
	int		basecolor[3];
	
	basecolor[0] = 130;
	basecolor[1] = 80;
	basecolor[2] = 50;

// pull the colors halfway to bright brown
	for (i=0 ; i<256 ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			newpalette[i*3+j] = (host_basepal[i*3+j] + basecolor[j])/2;
		}
	}
	
	setpal(newpalette);
}


/*
===================
R_TransformFrustum
===================
*/
void R_TransformFrustum (void)
{
	int		i;
	vec3_t	v, v2;
	
	for (i=0 ; i<4 ; i++)
	{
		v[0] = screenedge[i].normal[2];
		v[1] = -screenedge[i].normal[0];
		v[2] = screenedge[i].normal[1];

		v2[0] = v[1]*vright[0] + v[2]*vup[0] + v[0]*vpn[0];
		v2[1] = v[1]*vright[1] + v[2]*vup[1] + v[0]*vpn[1];
		v2[2] = v[1]*vright[2] + v[2]*vup[2] + v[0]*vpn[2];

		VectorCopy (v2, view_clipplanes[i].normal);

		view_clipplanes[i].dist = DotProduct (modelorg, v2);
	}
}


/*
================
TransformVector
================
*/
void TransformVector (vec3_t in, vec3_t out)
{
	out[0] = DotProduct(in,vright);
	out[1] = DotProduct(in,vup);
	out[2] = DotProduct(in,vpn);		
}


/*
================
R_TransformPlane
================
*/
void R_TransformPlane (mplane_t *p, float *normal, float *dist)
{
	float	d;
	
	d = DotProduct (r_origin, p->normal);
	*dist = p->dist - d;
// TODO: when we have rotating entities, this will need to use the view matrix
	TransformVector (p->normal, normal);
}


/*
===============
R_SetUpFrustumIndexes
===============
*/
void R_SetUpFrustumIndexes (void)
{
	int		i, j, *pindex;

	pindex = r_frustum_indexes;

	for (i=0 ; i<4 ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			if (view_clipplanes[i].normal[j] < 0)
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

	r_refdef.ambientlight = r_ambient.value;

	if (r_refdef.ambientlight < 0)
		r_refdef.ambientlight = 0;

	R_CheckVariables ();
	
	R_AnimateLight ();

	r_framecount++;

	numbtofpolys = 0;

/* debugging
r_refdef.vieworg[0]=  80;
r_refdef.vieworg[1]=      64;
r_refdef.vieworg[2]=      40;
r_refdef.viewangles[0]=    0;
r_refdef.viewangles[1]=    46.763641357;
r_refdef.viewangles[2]=    0;
*/

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, modelorg);
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

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

// start off with just the four screen edge clip planes
	R_TransformFrustum ();

// save base values
	VectorCopy (vpn, base_vpn);
	VectorCopy (vright, base_vright);
	VectorCopy (vup, base_vup);
	VectorCopy (modelorg, base_modelorg);

	R_SetSkyFrame ();

	R_SetUpFrustumIndexes ();

	r_cache_thrash = false;

// clear frame counts
	c_faceclip = 0;
	d_spanpixcount = 0;
	r_polycount = 0;
	r_drawnpolycount = 0;
	r_wholepolycount = 0;
	r_amodels_drawn = 0;

	D_SetupFrame ();
}

