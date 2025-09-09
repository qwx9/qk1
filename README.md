qk1
===

Modernized Quake for Linux/X11 and [9front](http://9front.org). For
9front-specific documentation refer to
[README-9front.md](./README-9front.md).

It was mainly extended in order to support [Arcane
Dimensions](https://www.moddb.com/mods/arcane-dimensions).

Some of the notable changes:

 * 32-bit and 64-bit systems support
 * raised [engine limits](https://quakewiki.org/wiki/Engine_Limits) and refactored memory management
 * better console editing and completion
 * support for any screen resolution
 * 24-bit *software* renderer as opposed to 8-bit one
 * colored lighting with built-in or [external LIT files](https://quakewiki.org/wiki/External_Lit_And_Vis_Files)
 * [BSP2](https://quakewiki.org/wiki/BSP2) and [BSP30 (Half-Life)](https://developer.valvesoftware.com/wiki/BSP_(GoldSrc)) support
 * translucent water with built-in or [external VIS files](https://quakewiki.org/wiki/External_Lit_And_Vis_Files)
 * [external textures](https://quakewiki.org/wiki/DP_GFX_EXTERNALTEXTURES) (TGA)
 * [skyboxes](https://quakewiki.org/wiki/Skybox_Support)
 * exponential fog
 * client's screen refresh rate not tied to server's
 * OpenAL with optional HRTF for sound
 * OGG/Vorbis for music
 * RMQ (v999) [network protocol](https://quakewiki.org/wiki/Network_Protocols); original v15 is supported as well
 * a ton of other bugfixes and improvements

Advertised [QSG extensions](https://quakewiki.org/wiki/QSG_extensions):

 * [DP_CL_LOADSKY](https://quakewiki.org/wiki/DP_CL_LOADSKY)
 * [DP_EF_ADDITIVE](https://quakewiki.org/wiki/DP_EF_ADDITIVE)
 * [DP_EF_NODRAW](https://quakewiki.org/wiki/DP_EF_NODRAW)
 * [DP_ENT_ALPHA](https://quakewiki.org/wiki/DP_ENT_ALPHA)
 * [DP_GFX_FOG](https://quakewiki.org/wiki/DP_GFX_FOG)
 * [DP_GFX_SKYBOX](https://quakewiki.org/wiki/DP_GFX_SKYBOX)
 * [DP_HALFLIFE_MAP](https://quakewiki.org/wiki/DP_HALFLIFE_MAP)
 * [DP_HALFLIFE_MAP_CVAR](https://quakewiki.org/wiki/DP_HALFLIFE_MAP_CVAR)
 * [DP_LITSUPPORT](https://quakewiki.org/wiki/DP_LITSUPPORT)
 * [DP_QC_ASINACOSATANATAN2TAN](https://quakewiki.org/wiki/DP_QC_DP_QC_ASINACOSATANATAN2TAN)
 * [DP_QC_CHANGEPITCH](https://quakewiki.org/wiki/DP_QC_CHANGEPITCH)
 * [DP_QC_MINMAXBOUND](https://quakewiki.org/wiki/DP_QC_MINMAXBOUND)
 * [DP_QC_SINCOSSQRTPOW](https://quakewiki.org/wiki/DP_QC_SINCOSSQRTPOW)
 * [DP_QC_TRACETOSS](https://quakewiki.org/wiki/DP_QC_TRACETOSS)
 * [DP_QC_TOKENIZE_CONSOLE](https://quakewiki.org/wiki/DP_QC_TOKENIZE_CONSOLE)
 * [DP_SPRITE32](https://quakewiki.org/wiki/DP_SPRITE32)
 * [DP_SV_ROTATINGBMODEL](https://quakewiki.org/wiki/DP_SV_ROTATINGBMODEL)
 * [KRIMZON_SV_PARSECLIENTCOMMAND](https://quakewiki.org/wiki/KRIMZON_SV_PARSECLIENTCOMMAND)
