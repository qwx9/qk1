#include "quakedef.h"

cvar_t chase_active = {"chase_active", "0"};

static cvar_t chase_back = {"chase_back", "100"};
static cvar_t chase_up = {"chase_up", "16"};
static cvar_t chase_right = {"chase_right", "0"};

static vec3_t chase_dest;

void Chase_Init (void)
{
	Cvar_RegisterVariable (&chase_back);
	Cvar_RegisterVariable (&chase_up);
	Cvar_RegisterVariable (&chase_right);
	Cvar_RegisterVariable (&chase_active);
}

static void
TraceLine(vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t	trace;

	memset(&trace, 0, sizeof trace);
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

	VectorCopy (trace.endpos, impact);
}

void Chase_Update (void)
{
	int		i;
	float	dist;
	vec3_t	forward, up, right;
	vec3_t	dest, stop;


	// if can't see player, reset
	AngleVectors (cl.viewangles, forward, right, up);

	// calc exact destination
	for (i=0 ; i<3 ; i++)
		chase_dest[i] = r_refdef.view.org[i]
		- forward[i]*chase_back.value
		- right[i]*chase_right.value;
	chase_dest[2] = r_refdef.view.org[2] + chase_up.value;

	// find the spot the player is looking at
	VectorMA (r_refdef.view.org, 4096, forward, dest);
	TraceLine (r_refdef.view.org, dest, stop);

	// calculate pitch to look at the same spot from camera
	VectorSubtract (stop, r_refdef.view.org, stop);
	dist = DotProduct (stop, forward);
	if (dist < 1)
		dist = 1;
	r_refdef.view.angles[PITCH] = -atanf(stop[2] / dist) / M_PI * 180;

	// move towards destination
	VectorCopy (chase_dest, r_refdef.view.org);
}

