#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

static void
MSG_WriteProtocolInfoNQ(sizebuf_t *sb, protocol_t *proto)
{
	USED(sb); USED(proto);
}

static void
MSG_ReadProtocolInfoNQ(protocol_t *proto)
{
	USED(proto);
	proto->flags = 0;
}

static void
MSG_WriteProtocolInfoRMQ(sizebuf_t *sb, protocol_t *proto)
{
	USED(proto);
	MSG_WriteLong(sb, PF_RMQ_ANGLE_INT16|PF_RMQ_COORD_INT32);
}

static void
MSG_ReadProtocolInfoRMQ(protocol_t *proto)
{
	proto->flags = MSG_ReadLong();
	if(proto->flags != (PF_RMQ_ANGLE_INT16|PF_RMQ_COORD_INT32))
		fatal("FIXME RMQ protocol implementation limited to angle_int16 and coord_int32");
}

protocol_t protos[PROTO_NUM] = {
	[PROTO_NQ] = {
		.name = "Quake",
		.version = 15,
		.limit_entity = 8192,
		.limit_channel = 8,
		.limit_sound = 256,
		.limit_frame = 256,
		.limit_model = 256,
		.MSG_WriteCoord = MSG_WriteCoord,
		.MSG_WriteAngle = MSG_WriteAngle,
		.MSG_ReadCoord = MSG_ReadCoord,
		.MSG_ReadAngle = MSG_ReadAngle,
		.MSG_WriteProtocolInfo = MSG_WriteProtocolInfoNQ,
		.MSG_ReadProtocolInfo = MSG_ReadProtocolInfoNQ,
	},
	[PROTO_RMQ] = {
		.name = "RMQ",
		.version = 999,
		.fl_large_entity = 1<<3, .large_entity = 8192, .limit_entity = 65536,
		.fl_large_channel = 1<<3, .large_channel = 8, .limit_channel = 256,
		.fl_large_sound = 1<<4, .large_sound = 256, .limit_sound = MAX_SOUNDS,
		.fl_large_frame = 1<<17, .large_frame = 256, .limit_frame = 65536,
		.fl_large_model = 1<<18, .large_model = 256, .limit_model = MAX_MODELS,
		.fl_large_weaponframe = 1<<24,
		.fl_large_weaponmodel = 1<<16,
		.fl_large_baseline_model = 1<<0,
		.fl_large_baseline_frame = 1<<1,
		.fl_alpha = 1<<16,
		.fl_baseline_alpha = 1<<2,
		.fl_weapon_alpha = 1<<25,
		.MSG_WriteCoord = MSG_WriteCoordInt32,
		.MSG_WriteAngle = MSG_WriteAngleInt16,
		.MSG_ReadCoord = MSG_ReadCoordInt32,
		.MSG_ReadAngle = MSG_ReadAngleInt16,
		.MSG_WriteProtocolInfo = MSG_WriteProtocolInfoRMQ,
		.MSG_ReadProtocolInfo = MSG_ReadProtocolInfoRMQ,
	},
};
