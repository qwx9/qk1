#include "quakedef.h"

static int num_temp_entities;
static entity_t cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t		cl_beams[MAX_BEAMS];

static Sfx *cl_Sfxink1;
static Sfx *cl_sfx_wizhit;
static Sfx *cl_sfx_knighthit;
static Sfx *cl_sfx_ric1;
static Sfx *cl_sfx_ric2;
static Sfx *cl_sfx_ric3;
static Sfx *cl_sfx_r_exp3;

/*
=================
CL_ParseTEnt
=================
*/
void CL_InitTEnts (void)
{
	cl_sfx_wizhit = precachesfx ("wizard/hit.wav");
	cl_sfx_knighthit = precachesfx ("hknight/hit.wav");
	cl_Sfxink1 = precachesfx ("weapons/tink1.wav");
	cl_sfx_ric1 = precachesfx ("weapons/ric1.wav");
	cl_sfx_ric2 = precachesfx ("weapons/ric2.wav");
	cl_sfx_ric3 = precachesfx ("weapons/ric3.wav");
	cl_sfx_r_exp3 = precachesfx ("weapons/r_exp3.wav");
}

/*
=================
CL_ParseBeam
=================
*/
static void CL_ParseBeam (model_t *m)
{
	int		ent;
	vec3_t	start, end;
	beam_t	*b;
	int		i;

	ent = MSG_ReadShort ();

	MSG_ReadVec(cl.protocol, start);
	MSG_ReadVec(cl.protocol, end);

	// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}

	// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
	Con_Printf ("beam list overflow!\n");
}

/*
=================
CL_ParseTEnt
=================
*/
void CL_ParseTEnt (void)
{
	int		type;
	vec3_t	pos;
	dlight_t	*dl;
	int		rnd;
	int		colorStart, colorLength;

	type = MSG_ReadByte ();
	switch (type)
	{
	case TE_WIZSPIKE:			// spike hitting wall
		MSG_ReadVec(cl.protocol, pos);
		R_RunParticleEffect (pos, vec3_origin, 20, 30);
		startsfx (-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;

	case TE_KNIGHTSPIKE:			// spike hitting wall
		MSG_ReadVec(cl.protocol, pos);
		R_RunParticleEffect (pos, vec3_origin, 226, 20);
		startsfx (-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;

	case TE_SPIKE:			// spike hitting wall
		MSG_ReadVec(cl.protocol, pos);
		R_RunParticleEffect (pos, vec3_origin, 0, 10);
		if ( rand() % 5 )
			startsfx (-1, 0, cl_Sfxink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				startsfx (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				startsfx (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				startsfx (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKE:			// super spike hitting wall
		MSG_ReadVec(cl.protocol, pos);
		R_RunParticleEffect (pos, vec3_origin, 0, 20);

		if ( rand() % 5 )
			startsfx (-1, 0, cl_Sfxink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				startsfx (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				startsfx (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				startsfx (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;

	case TE_GUNSHOT:			// bullet hitting wall
		MSG_ReadVec(cl.protocol, pos);
		R_RunParticleEffect (pos, vec3_origin, 0, 20);
		break;

	case TE_EXPLOSION:			// rocket explosion
		MSG_ReadVec(cl.protocol, pos);
		R_ParticleExplosion (pos);
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		startsfx (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TAREXPLOSION:			// tarbaby explosion
		MSG_ReadVec(cl.protocol, pos);
		R_BlobExplosion (pos);

		startsfx (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_LIGHTNING1:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt.mdl", true));
		break;

	case TE_LIGHTNING2:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt2.mdl", true));
		break;

	case TE_LIGHTNING3:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt3.mdl", true));
		break;

	case TE_BEAM:				// grappling hook beam
		CL_ParseBeam (Mod_ForName("progs/beam.mdl", true));
		break;

	case TE_LAVASPLASH:
		MSG_ReadVec(cl.protocol, pos);
		R_LavaSplash (pos);
		break;

	case TE_TELEPORT:
		MSG_ReadVec(cl.protocol, pos);
		R_TeleportSplash (pos);
		break;

	case TE_EXPLOSION2:				// color mapped explosion
		MSG_ReadVec(cl.protocol, pos);
		colorStart = MSG_ReadByte ();
		colorLength = MSG_ReadByte ();
		R_ParticleExplosion2 (pos, colorStart, colorLength);
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		startsfx (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	default:
		fatal ("CL_ParseTEnt: bad type");
	}
}


/*
=================
CL_NewTempEntity
=================
*/
static entity_t *CL_NewTempEntity (void)
{
	entity_t	*ent;

	if (cl_numvisedicts == MAX_VISEDICTS)
		return nil;
	if (num_temp_entities == MAX_TEMP_ENTITIES)
		return nil;
	ent = &cl_temp_entities[num_temp_entities];
	memset(ent, 0, sizeof *ent);
	num_temp_entities++;
	cl_visedicts[cl_numvisedicts] = ent;
	cl_numvisedicts++;

	ent->colormap = vid.colormap;
	return ent;
}

void
CL_ResetTEnts(void)
{
	memset(cl_temp_entities, 0, sizeof(cl_temp_entities));
	num_temp_entities = 0;
}

/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts (void)
{
	int			i;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	*ent;
	float		yaw, pitch;
	float		forward;

	num_temp_entities = 0;

	// update lightning
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.viewentity)
		{
			VectorCopy (cl_entities[cl.viewentity].origin, b->start);
		}

		// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;

			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (int) (atan2(dist[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		// add new entities for the lightning
		VectorCopy (b->start, org);
		d = VectorNormalize(dist);
		while (d > 0)
		{
			ent = CL_NewTempEntity ();
			if (!ent)
				return;
			VectorCopy (org, ent->origin);
			ent->model = b->model;
			ent->angles[0] = pitch;
			ent->angles[1] = yaw;
			ent->angles[2] = rand()%360;

			for (i=0 ; i<3 ; i++)
				org[i] += dist[i]*30;
			d -= 30;
		}
	}

}


