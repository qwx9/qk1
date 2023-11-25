// spritegn.h: header file for sprite generation program

enum {
	SPRITE_VERSION = 1,

	SPR_VP_PARALLEL_UPRIGHT = 0,
	SPR_FACING_UPRIGHT,
	SPR_VP_PARALLEL,
	SPR_ORIENTED,
	SPR_VP_PARALLEL_ORIENTED,
};

typedef enum {
	SPR_SINGLE,
	SPR_GROUP,
}spriteframetype_t;

#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I')
// little-endian "IDSP"
