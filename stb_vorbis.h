#pragma once

#include "platform.h"
#ifdef QUAKE_BIG_ENDIAN
#define STB_VORBIS_BIG_ENDIAN
#endif

#define STB_VORBIS_NO_PUSHDATA_API

#ifndef STB_VORBIS_INCLUDE_STB_VORBIS_H
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#endif
