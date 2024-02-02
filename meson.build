project(
	'qk1',
	'c',
	version: '1.09',
	default_options: [
		'c_std=c99',
		'warning_level=2',
		'buildtype=debug',
		#'b_coverage=true',
	],
)

add_project_arguments(
	'-Wno-unknown-pragmas',
	'-Wno-missing-field-initializers',
	'-Wno-implicit-fallthrough',
	'-Werror=odr',
	'-Werror=strict-aliasing',
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
	'm_random.c',
	'mathlib.c',
	'menu.c',
	'model.c',
	'model_alias.c',
	'model_brush.c',
	'model_bsp.c',
	'model_bsp2.c',
	'model_bsp30.c',
	'model_sprite.c',
	'mt19937-64.c',
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
	'stb_vorbis.c',
	'sv_main.c',
	'sv_move.c',
	'sv_phys.c',
	'sv_user.c',
	'view.c',
	'wav.c',
	'world.c',
	'zone.c',
]

cc = meson.get_compiler('c')

if cc.get_id() == 'clang'
	add_project_arguments(
		'-Wno-microsoft-anon-tag',
		'-D__wchar_t=__please_no_wchar_t_thank_you',
		language: 'c',
	)
else
	add_project_arguments(
		'-Werror=lto-type-mismatch',
		language: 'c',
	)
endif

src_sys = [
	'seprint.c',
]

math = cc.find_library('m', required: false)
network = dependency('', required: false)
audio = dependency('', required: false)
video = dependency('', required: false)

if host_machine.system() == 'macos'
	src_sys += [
		'cd_null.c',
		'in_null.c',
		'net_udp_null.c',
		'snd_mix.c',
		'snd_null.c',
		'sys_macos.c',
		'vid_null.c',
	]
else
	audio = dependency('openal')
	video = dependency('sdl2')
	src_sys += [
		'in_sdl.c',
		'parg.c',
		'snd_openal.c',
		'vid_sdl.c',
	]
	if host_machine.system() == 'windows'
		src_sys += [
			'net_udp_windows.c',
			'sys_windows.c',
		]
		network = cc.find_library('ws2_32')
	else
		if host_machine.system() == 'haiku'
			network = cc.find_library('network')
		endif
		src_sys += [
			'net_udp_unix.c',
			'sys_unix.c',
		]
	endif
endif

if cc.check_header('endian.h')
	add_project_arguments(
		'-DHAVE_ENDIAN_H',
		language: 'c',
	)
elif host_machine.endian() == 'big'
	add_project_arguments(
		'-DQUAKE_BIG_ENDIAN',
		language: 'c',
	)
else
	add_project_arguments(
		'-DQUAKE_LITTLE_ENDIAN',
		language: 'c',
	)
endif

executable(
	'qk1',
	sources: src_common + src_sys,
	dependencies: [
		audio,
		math,
		network,
		video,
	],
	include_directories: include_directories(
		'posix',
	),
)
