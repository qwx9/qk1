#include "quakedef.h"
#include "colormatrix.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t v_scale = {"v_scale", "1", true};

static cvar_t v_cshiftpercent = {"v_cshiftpercent", "30", true};

static cvar_t scr_ofsx = {"scr_ofsx","0", false};
static cvar_t scr_ofsy = {"scr_ofsy","0", false};
static cvar_t scr_ofsz = {"scr_ofsz","0", false};

static cvar_t cl_rollspeed = {"cl_rollspeed", "200"};
static cvar_t cl_rollangle = {"cl_rollangle", "2.0"};

static cvar_t cl_bob = {"cl_bob","0.02", false};
static cvar_t cl_bobcycle = {"cl_bobcycle","0.6", false};
static cvar_t cl_bobup = {"cl_bobup","0.5", false};

static cvar_t v_kicktime = {"v_kicktime", "0.5", false};
static cvar_t v_kickroll = {"v_kickroll", "0.6", false};
static cvar_t v_kickpitch = {"v_kickpitch", "0.6", false};

static cvar_t v_iyaw_cycle = {"v_iyaw_cycle", "2", false};
static cvar_t v_iroll_cycle = {"v_iroll_cycle", "0.5", false};
static cvar_t v_ipitch_cycle = {"v_ipitch_cycle", "1", false};
static cvar_t v_iyaw_level = {"v_iyaw_level", "0.3", false};
static cvar_t v_iroll_level = {"v_iroll_level", "0.1", false};
static cvar_t v_ipitch_level = {"v_ipitch_level", "0.3", false};

static cvar_t v_idlescale = {"v_idlescale", "0", false};

static cvar_t crosshair = {"crosshair", "1", true};
static cvar_t crosshair_shape = {"crosshair_shape", "0", true};
static cvar_t cl_crossx = {"cl_crossx", "0", false};
static cvar_t cl_crossy = {"cl_crossy", "0", false};

static double v_dmg_time, v_dmg_roll, v_dmg_pitch;
static double v_step_time, v_step_z, v_step_oldz;

extern	int			in_forward, in_forward2, in_back;


/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
vec3_t	forward, right, up;

float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	float	sign;
	float	side;
	float	value;

	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);

	value = cl_rollangle.value;
	//if (cl.inwater)
	//	value *= 6;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;

	return side*sign;

}


/*
===============
V_CalcBob

===============
*/
static float
V_CalcBob(void)
{
	float	bob;
	float	cycle;

	cycle = cl.time - (int)(cl.time/cl_bobcycle.value)*cl_bobcycle.value;
	cycle /= cl_bobcycle.value;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

	// bob is proportional to velocity in the xy plane
	// (don't count Z, or jumping messes it up)

	bob = sqrtf(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
	//Con_Printf ("speed: %5.1f\n", Length(cl.velocity));
	bob = bob*0.3 + bob*0.7*sinf(cycle);
	if (bob > 4)
		bob = 4;
	else if (bob < -7)
		bob = -7;
	return bob;

}


//=============================================================================


static cvar_t v_centermove = {"v_centermove", "0.15", false};
static cvar_t v_centerspeed = {"v_centerspeed","500"};


void
V_StartPitchDrift(cmd_t *c)
{
	USED(c);
	if(cl.laststop == cl.time)
		return;		// something else is keeping it from drifting
	if(cl.nodrift || !cl.pitchvel){
		cl.pitchvel = v_centerspeed.value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void
V_StopPitchDrift(void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when
===============
*/
static void
V_DriftPitch(void)
{
	float		delta, move;

	if (noclip_anglehack || !cl.onground || cls.demoplayback )
	{
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

	// don't count small mouse motion
	if (cl.nodrift)
	{
		if ( fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
			cl.driftmove = 0;
		else
			cl.driftmove += host_frametime;

		if ( cl.driftmove > v_centermove.value)
			V_StartPitchDrift(nil);
		return;
	}

	delta = cl.idealpitch - cl.viewangles[PITCH];

	if (!delta)
	{
		cl.pitchvel = 0;
		return;
	}

	move = host_frametime * cl.pitchvel;
	cl.pitchvel += host_frametime * v_centerspeed.value;

	//Con_Printf ("move: %f (%f)\n", move, host_frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}





/*
==============================================================================

						PALETTE FLASHES

==============================================================================
*/


static cshift_t cshift_empty = { {130,80,50}, 0 };
static cshift_t cshift_water = { {130,80,50}, 128 };
static cshift_t cshift_slime = { {0,25,5}, 150 };
static cshift_t cshift_lava = { {255,80,0}, 150 };

/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
	int		armor, blood;
	vec3_t	from;
	vec3_t	forward, right, up;
	entity_t	*ent;
	float	side;
	float	count;

	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	MSG_ReadVec(cl.protocol, from);

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;		// but sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

	// calculate view angle kicks
	ent = &cl_entities[cl.viewentity];

	VectorSubtract (from, ent->origin, from);
	VectorNormalize (from);

	AngleVectors (ent->angles, forward, right, up);

	side = DotProduct (from, right);
	v_dmg_roll = count*side*v_kickroll.value;

	side = DotProduct (from, forward);
	v_dmg_pitch = count*side*v_kickpitch.value;

	v_dmg_time = v_kicktime.value;
}


/*
==================
V_cshift_f
==================
*/
static void
V_cshift_f(cmd_t *c)
{
	USED(c);
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
static void
V_BonusFlash_f(cmd_t *c)
{
	USED(c);
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
	switch (contents)
	{
	case CONTENTS_EMPTY:
	case CONTENTS_SOLID:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	case CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;
	case CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;
	default:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
static void
V_CalcPowerupCshift(void)
{
	if (cl.items & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else if (cl.items & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20;
	}
	else if (cl.items & IT_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100;
	}
	else if (cl.items & IT_INVULNERABILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
}

static void
V_Blend(void)
{
	float c[4], a;
	int i;

	c[0] = c[1] = c[2] = c[3] = 0;
	for(i = 0; i < NUM_CSHIFTS; i++){
		a = ((cl.cshifts[i].percent * v_cshiftpercent.value)/100.0)/255.0;
		if(a > 0){
			c[3] += a*(1 - c[3]);
			a = a/c[3];
			c[0] = c[0]*(1-a) + cl.cshifts[i].destcolor[0]*a;
			c[1] = c[1]*(1-a) + cl.cshifts[i].destcolor[1]*a;
			c[2] = c[2]*(1-a) + cl.cshifts[i].destcolor[2]*a;
		}
	}
	c[3] = clamp(c[3], 0, 1.0);
	c[2] *= c[3]/255.0;
	c[1] *= c[3]/255.0;
	c[0] *= c[3]/255.0;
	cmsetvblend(c);
}

/*
=============
V_ApplyShifts
=============
*/
void V_ApplyShifts (void)
{
	int i, j;
	bool update;

	V_CalcPowerupCshift();

	update = false;
	for(i = 0; i < NUM_CSHIFTS; i++){
		if(cl.cshifts[i].percent != cl.prev_cshifts[i].percent){
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
			update = true;
		}
		for(j = 0; j < 3; j++){
			if(cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j]){
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
				update = true;
			}
		}
	}

	// drop the damage value
	if(cl.cshifts[CSHIFT_DAMAGE].percent > 0){
		cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime*150;
		if(cl.cshifts[CSHIFT_DAMAGE].percent < 0)
			cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	}

	// drop the bonus value
	if(cl.cshifts[CSHIFT_BONUS].percent > 0){
		cl.cshifts[CSHIFT_BONUS].percent -= host_frametime*100;
		if(cl.cshifts[CSHIFT_BONUS].percent < 0)
			cl.cshifts[CSHIFT_BONUS].percent = 0;
	}

	if(update)
		V_Blend();
}


/*
==============================================================================

						VIEW RENDERING

==============================================================================
*/

static float
angledelta(float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}

/*
==================
CalcGunAngle
==================
*/
static void
CalcGunAngle(void)
{
	float	yaw, pitch, move;
	static float oldyaw = 0;
	static float oldpitch = 0;

	yaw = r_refdef.view.angles[YAW];
	pitch = -r_refdef.view.angles[PITCH];

	yaw = angledelta(yaw - r_refdef.view.angles[YAW]) * 0.4;
	if (yaw > 10)
		yaw = 10;
	if (yaw < -10)
		yaw = -10;
	pitch = angledelta(-pitch - r_refdef.view.angles[PITCH]) * 0.4;
	if (pitch > 10)
		pitch = 10;
	if (pitch < -10)
		pitch = -10;
	move = host_frametime*20;
	if (yaw > oldyaw)
	{
		if (oldyaw + move < yaw)
			yaw = oldyaw + move;
	}
	else
	{
		if (oldyaw - move > yaw)
			yaw = oldyaw - move;
	}

	if (pitch > oldpitch)
	{
		if (oldpitch + move < pitch)
			pitch = oldpitch + move;
	}
	else
	{
		if (oldpitch - move > pitch)
			pitch = oldpitch - move;
	}

	oldyaw = yaw;
	oldpitch = pitch;

	cl.viewent.angles[YAW] = r_refdef.view.angles[YAW] + yaw;
	cl.viewent.angles[PITCH] = - (r_refdef.view.angles[PITCH] + pitch);

	cl.viewent.angles[ROLL] -= v_idlescale.value * sinf(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	cl.viewent.angles[PITCH] -= v_idlescale.value * sinf(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	cl.viewent.angles[YAW] -= v_idlescale.value * sinf(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}

/*
==============
V_BoundOffsets
==============
*/
static void
V_BoundOffsets(void)
{
	entity_t	*ent;

	ent = &cl_entities[cl.viewentity];

	// absolutely bound refresh reletive to entity clipping hull
	// so the view can never be inside a solid wall

	if (r_refdef.view.org[0] < ent->origin[0] - 14)
		r_refdef.view.org[0] = ent->origin[0] - 14;
	else if (r_refdef.view.org[0] > ent->origin[0] + 14)
		r_refdef.view.org[0] = ent->origin[0] + 14;
	if (r_refdef.view.org[1] < ent->origin[1] - 14)
		r_refdef.view.org[1] = ent->origin[1] - 14;
	else if (r_refdef.view.org[1] > ent->origin[1] + 14)
		r_refdef.view.org[1] = ent->origin[1] + 14;
	if (r_refdef.view.org[2] < ent->origin[2] - 22)
		r_refdef.view.org[2] = ent->origin[2] - 22;
	else if (r_refdef.view.org[2] > ent->origin[2] + 30)
		r_refdef.view.org[2] = ent->origin[2] + 30;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
static void
V_AddIdle(void)
{
	r_refdef.view.angles[ROLL] += v_idlescale.value * sinf(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	r_refdef.view.angles[PITCH] += v_idlescale.value * sinf(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	r_refdef.view.angles[YAW] += v_idlescale.value * sinf(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
static void
V_CalcViewRoll(void)
{
	float		side;

	side = V_CalcRoll (cl_entities[cl.viewentity].angles, cl.velocity);
	r_refdef.view.angles[ROLL] += side;

	if (v_dmg_time > 0)
	{
		r_refdef.view.angles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
		r_refdef.view.angles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
		v_dmg_time -= host_frametime;
	}

	if (cl.stats[STAT_HEALTH] <= 0)
	{
		r_refdef.view.angles[ROLL] = 80;	// dead view angle
		return;
	}

}


/*
==================
V_CalcIntermissionRefdef

==================
*/
static void
V_CalcIntermissionRefdef(void)
{
	entity_t	*ent, *view;
	float		old;

	// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
	// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

	VectorCopy (ent->origin, r_refdef.view.org);
	VectorCopy (ent->angles, r_refdef.view.angles);
	view->model = nil;

	// always idle in intermission
	old = v_idlescale.value;
	v_idlescale.value = 1;
	V_AddIdle ();
	v_idlescale.value = old;
}

/*
==================
V_CalcRefdef

==================
*/
static void
V_CalcRefdef(void)
{
	entity_t	*ent, *view;
	int			i;
	vec3_t		forward, right, up;
	vec3_t		angles;
	float		bob;

	V_DriftPitch ();

	// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
	// view is the weapon model (only visible from inside body)
	view = &cl.viewent;


	// transform the view offset by the model's matrix to get the offset from
	// model origin for the view
	ent->angles[YAW] = cl.viewangles[YAW];	// the model should face
										// the view dir
	ent->angles[PITCH] = -cl.viewangles[PITCH];	// the model should face
										// the view dir


	bob = V_CalcBob ();

	// refresh position
	VectorCopy (ent->origin, r_refdef.view.org);
	r_refdef.view.org[2] += cl.viewheight + bob;

	// never let it sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	r_refdef.view.org[0] += 1.0/32;
	r_refdef.view.org[1] += 1.0/32;
	r_refdef.view.org[2] += 1.0/32;

	VectorCopy (cl.viewangles, r_refdef.view.angles);
	V_CalcViewRoll ();
	V_AddIdle ();

	// offsets
	angles[PITCH] = -ent->angles[PITCH];	// because entity pitches are
											//  actually backward
	angles[YAW] = ent->angles[YAW];
	angles[ROLL] = ent->angles[ROLL];

	AngleVectors (angles, forward, right, up);

	for (i=0 ; i<3 ; i++)
		r_refdef.view.org[i] += scr_ofsx.value*forward[i]
			+ scr_ofsy.value*right[i]
			+ scr_ofsz.value*up[i];


	V_BoundOffsets ();

	// set up gun position
	VectorCopy (cl.viewangles, view->angles);

	CalcGunAngle ();

	VectorCopy (ent->origin, view->origin);
	view->origin[2] += cl.viewheight;

	for (i=0 ; i<3 ; i++)
	{
		view->origin[i] += forward[i]*bob*0.4;
		//view->origin[i] += right[i]*bob*0.4;
		//view->origin[i] += up[i]*bob*0.8;
	}
	view->origin[2] += bob;

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV

	/*
	if (cl.model_precache[cl.stats[STAT_WEAPON]] && strcmp (cl.model_precache[cl.stats[STAT_WEAPON]]->name,  "progs/v_shot2.mdl"))
	*/
	if (scr_viewsize.value == 110)
		view->origin[2] += 1;
	else if (scr_viewsize.value == 100)
		view->origin[2] += 2;

	view->model = cl.model_precache[cl.stats[STAT_WEAPON]];
	view->frame = cl.stats[STAT_WEAPONFRAME];
	view->colormap = vid.colormap;

	// set up the refresh position
	VectorAdd (r_refdef.view.angles, cl.punchangle, r_refdef.view.angles);

	// smooth out stair step ups
	if(!noclip_anglehack && cl.onground && ent->origin[2] - v_step_z > 0){
		v_step_z = v_step_oldz + (cl.time - v_step_time) * 80.0;

		if(v_step_z > ent->origin[2]){
			v_step_z = v_step_oldz = ent->origin[2];
			v_step_time = cl.time;
		}
		if(v_step_z < ent->origin[2]-12.0){
			v_step_z = v_step_oldz = ent->origin[2] - 12.0;
			v_step_time = cl.time;
		}

		r_refdef.view.org[2] += v_step_z - ent->origin[2];
		view->origin[2] += v_step_z - ent->origin[2];
	}else{
		v_step_z = v_step_oldz = ent->origin[2];
		v_step_time = cl.time;
	}

	if (chase_active.value)
		Chase_Update ();
}

void
V_NewMap(void)
{
	v_step_z = v_step_oldz = 0.0;
	v_step_time = 0.0;
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
extern vrect_t	scr_vrect;

void V_RenderView (void)
{
	if (con_forcedup)
		return;

	// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		setcvar ("scr_ofsx", "0");
		setcvar ("scr_ofsy", "0");
		setcvar ("scr_ofsz", "0");
	}

	if (cl.intermission)
	{	// intermission / finale rendering
		V_CalcIntermissionRefdef ();
	}
	else
	{
		if (!cl.paused /* && (sv.maxclients > 1 || key_dest == key_game) */ )
			V_CalcRefdef ();
	}

	R_PushDlights ();

	R_RenderView ();

	if (crosshair.value)
		Draw_Character(
			scr_vrect.x + scr_vrect.width/2 + cl_crossx.value,
			scr_vrect.y + scr_vrect.height/2 + cl_crossy.value,
			crosshair_shape.value < 1 ? '+' : '\xf');
}

//============================================================================

static void
screenshot(cmd_t *c)
{
	static char opath[48];
	static int pathcnt = 2;
	char path[48], *t;
	FILE *f;
	byte *b;
	int n;

	USED(c);
	if((t = sys_timestamp()) == nil){
err:
		Con_Printf("screenshot: %s\n", lerr());
		return;
	}
	n = snprint(path, sizeof(path), "screenshots/%s.tga", t);
	if(strncmp(opath, path, n-4) == 0)
		snprint(path+n-4, sizeof(path)-(n-4), "x%d.tga", pathcnt++);
	else
		pathcnt = 2;
	if((f = createfile(path)) == nil)
		goto err;
	if((n = TGA_Encode(&b, "qk1", vid.buffer, vid.width, vid.height)) > 0)
		n = fwrite(b, n, 1, f);
	free(b);
	fclose(f);
	if(n != 1){
		removefile(path);
		goto err;
	}
	strcpy(opath, path);
}

static void
v_scale_cb(cvar_t *var)
{
	static float scale = 1.0;

	var->value = clamp(var->value, 1, 16);
	if(scale != var->value){
		scale = var->value;
		vid.resized = true;
	}
}

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	cminit();

	Cmd_AddCommand("v_cshift", V_cshift_f);
	Cmd_AddCommand("bf", V_BonusFlash_f);
	Cmd_AddCommand("centerview", V_StartPitchDrift);
	Cmd_AddCommand("screenshot", screenshot);

	Cvar_RegisterVariable(&v_scale);
	v_scale.cb = v_scale_cb;

	Cvar_RegisterVariable(&v_cshiftpercent);
	Cvar_RegisterVariable (&v_centermove);
	Cvar_RegisterVariable (&v_centerspeed);

	Cvar_RegisterVariable (&v_iyaw_cycle);
	Cvar_RegisterVariable (&v_iroll_cycle);
	Cvar_RegisterVariable (&v_ipitch_cycle);
	Cvar_RegisterVariable (&v_iyaw_level);
	Cvar_RegisterVariable (&v_iroll_level);
	Cvar_RegisterVariable (&v_ipitch_level);

	Cvar_RegisterVariable (&v_idlescale);
	Cvar_RegisterVariable (&crosshair);
	Cvar_RegisterVariable (&crosshair_shape);
	Cvar_RegisterVariable (&cl_crossx);
	Cvar_RegisterVariable (&cl_crossy);

	Cvar_RegisterVariable (&scr_ofsx);
	Cvar_RegisterVariable (&scr_ofsy);
	Cvar_RegisterVariable (&scr_ofsz);
	Cvar_RegisterVariable (&cl_rollspeed);
	Cvar_RegisterVariable (&cl_rollangle);
	Cvar_RegisterVariable (&cl_bob);
	Cvar_RegisterVariable (&cl_bobcycle);
	Cvar_RegisterVariable (&cl_bobup);

	Cvar_RegisterVariable (&v_kicktime);
	Cvar_RegisterVariable (&v_kickroll);
	Cvar_RegisterVariable (&v_kickpitch);
}
