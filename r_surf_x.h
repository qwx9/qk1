#include "r_surf_light.h"

#define mip 0
#define DrawSurfaceBlock DrawSurfaceBlock_m0
#include "r_surf_block.h"
#undef mip
#undef DrawSurfaceBlock

#define mip 1
#define DrawSurfaceBlock DrawSurfaceBlock_m1
#include "r_surf_block.h"
#undef mip
#undef DrawSurfaceBlock

#define mip 2
#define DrawSurfaceBlock DrawSurfaceBlock_m2
#include "r_surf_block.h"
#undef mip
#undef DrawSurfaceBlock

#define mip 3
#define DrawSurfaceBlock DrawSurfaceBlock_m3
#include "r_surf_block.h"
#undef mip
#undef DrawSurfaceBlock
