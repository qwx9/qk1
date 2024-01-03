</$objtype/mkfile

BIN=/$objtype/bin/games
TARG=quake
CFLAGS=$CFLAGS -D__plan9__ -D__${objtype}__ -Iplan9

OFILES=\
	cd.$O\
	cd_plan9.$O\
	chase.$O\
	cl_demo.$O\
	cl_input.$O\
	cl_main.$O\
	cl_parse.$O\
	cl_tent.$O\
	cmd.$O\
	common.$O\
	console.$O\
	cvar.$O\
	d_alpha.$O\
	d_edge.$O\
	d_init.$O\
	d_modech.$O\
	d_part.$O\
	d_polyse.$O\
	d_scan.$O\
	d_sky.$O\
	d_sprite.$O\
	d_surf.$O\
	d_vars.$O\
	draw.$O\
	fs.$O\
	host.$O\
	host_cmd.$O\
	i_external.$O\
	i_resize.$O\
	i_tga.$O\
	i_wad.$O\
	in_plan9.$O\
	isnanf.$O\
	keys.$O\
	m_dotproduct`{test -f span_$objtype.s && echo -n _$objtype}.$O\
	mathlib.$O\
	menu.$O\
	model.$O\
	model_alias.$O\
	model_brush.$O\
	model_bsp.$O\
	model_bsp2.$O\
	model_bsp30.$O\
	model_sprite.$O\
	nanosec.$O\
	net_dgrm_plan9.$O\
	net_loop.$O\
	net_main.$O\
	net_udp_plan9.$O\
	pr_cmds.$O\
	pr_edict.$O\
	pr_exec.$O\
	protocol.$O\
	r_aclip.$O\
	r_alias.$O\
	r_bsp.$O\
	r_draw.$O\
	r_edge.$O\
	r_efrag.$O\
	r_fog.$O\
	r_light.$O\
	r_main.$O\
	r_misc.$O\
	r_part.$O\
	r_sky.$O\
	r_sprite.$O\
	r_surf.$O\
	sbar.$O\
	screen.$O\
	snd_mix.$O\
	snd_plan9.$O\
	softfloat.$O\
	sv_main.$O\
	sv_move.$O\
	sv_phys.$O\
	sv_user.$O\
	sys_plan9.$O\
	vid_plan9.$O\
	view.$O\
	wav.$O\
	world.$O\
	zone.$O\

HFILES=\
	adivtab.h\
	anorms.h\
	bspfile.h\
	client.h\
	cmd.h\
	common.h\
	console.h\
	cvar.h\
	d_iface.h\
	d_local.h\
	dat.h\
	draw.h\
	fns.h\
	i_tga.h\
	i_wad.h\
	input.h\
	keys.h\
	mathlib.h\
	menu.h\
	model.h\
	modelgen.h\
	net.h\
	plan9/platform.h\
	pr_comp.h\
	progdefs.h\
	progs.h\
	protocol.h\
	quakedef.h\
	r_local.h\
	r_shared.h\
	render.h\
	sbar.h\
	screen.h\
	server.h\
	softfloat.h\
	spritegn.h\
	vid.h\
	view.h\
	world.h\
	zone.h\

</sys/src/cmd/mkone

d_scan.$O: r_fog.h

i_resize.$O: stb_image_resize2.h

r_surf.$O: r_surf_block.h r_surf_light.h r_surf_x.h

i_resize.$O: i_resize.c
	$CC $CFLAGS -p i_resize.c
