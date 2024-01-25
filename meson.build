project(
	'qk1',
	'c',
	version: '1.09',
	default_options: [
		'c_std=c2x',
		'warning_level=2',
		'buildtype=debugoptimized',
		#'b_coverage=true',
	],
)

add_global_arguments(
	'-Wno-unknown-pragmas',
	'-Wno-missing-field-initializers',
	'-Wno-implicit-fallthrough',
	'-fms-extensions',
	'-D_DEFAULT_SOURCE',
	language: 'c',
)

src_common = [
	'cd.c',
	'chase.c',
	'cl_demo.c',
	'cl_input.c',
	'cl_main.c',
	'cl_parse.c',
	'cl_tent.c',
	'cmd.c',
	'common.c',
	'console.c',
	'cvar.c',
	'd_alpha.c',
	'd_edge.c',
	'd_init.c',
	'd_modech.c',
	'd_part.c',
	'd_polyse.c',
	'd_scan.c',
	'd_sky.c',
	'd_sprite.c',
	'd_surf.c',
	'd_vars.c',
	'draw.c',
	'fs.c',
	'host.c',
	'host_cmd.c',
	'i_external.c',
	'i_resize.c',
	'i_tga.c',
	'i_wad.c',
	'keys.c',
	'mathlib.c',
	'menu.c',
	'model.c',
	'model_alias.c',
	'model_brush.c',
	'model_bsp.c',
	'model_bsp2.c',
	'model_bsp30.c',
	'model_sprite.c',
	'net_loop.c',
	'net_main.c',
	'pal.c',
	'pr_cmds.c',
	'pr_edict.c',
	'pr_exec.c',
	'protocol.c',
	'r_aclip.c',
	'r_alias.c',
	'r_bsp.c',
	'r_draw.c',
	'r_edge.c',
	'r_efrag.c',
	'r_fog.c',
	'r_light.c',
	'r_main.c',
	'r_misc.c',
	'r_part.c',
	'r_sky.c',
	'r_sprite.c',
	'r_surf.c',
	'sbar.c',
	'screen.c',
	'softfloat.c',
	'sv_main.c',
	'sv_move.c',
	'sv_phys.c',
	'sv_user.c',
	'view.c',
	'wav.c',
	'world.c',
	'zone.c',
]

src_unix = [
	'3rd/parg/parg.c',
	'in_sdl.c',
	'net_udp_unix.c',
	'seprint.c',
	'snd_openal.c',
	'sys_snail.c',
	'sys_unix.c',
	'vid_sdl.c',
]

sdl2 = dependency('sdl2')
openal = dependency('openal')
threads = dependency('threads')

cc = meson.get_compiler('c')
if cc.get_id() == 'clang'
	add_global_arguments(
		'-Wno-microsoft-anon-tag',
		language: 'c',
	)
endif

libm = cc.find_library('m', required: false)

executable(
	'qk1',
	sources: src_common + src_unix,
	dependencies: [
		sdl2,
		openal,
		threads,
		libm,
	],
	include_directories: include_directories(
		'3rd/parg',
		'unix',
	),
)
