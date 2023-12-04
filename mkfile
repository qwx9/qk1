</$objtype/mkfile

BIN=/$objtype/bin/games
TARG=quake
CFLAGS=$CFLAGS -D__plan9__

OFILES=\
	span`{test -f span_$objtype.s && echo -n _$objtype}.$O\
	span_alpha.$O\
	dotproduct`{test -f span_$objtype.s && echo -n _$objtype}.$O\
	cd.$O\
	cl_demo.$O\
	cl_input.$O\
	cl_main.$O\
	cl_parse.$O\
	cl_tent.$O\
	chase.$O\
	cmd.$O\
	common.$O\
	console.$O\
	cvar.$O\
	dotadd.$O\
	draw.$O\
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
	fs.$O\
	host.$O\
	host_cmd.$O\
	in.$O\
	isnanf.$O\
	keys.$O\
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
	net_dgrm.$O\
	net_loop.$O\
	net_main.$O\
	net_udp.$O\
	pr_cmds.$O\
	pr_edict.$O\
	pr_exec.$O\
	protocol.$O\
	qk1.$O\
	r_aclip.$O\
	r_alias.$O\
	r_bsp.$O\
	r_draw.$O\
	r_efrag.$O\
	r_edge.$O\
	r_light.$O\
	r_main.$O\
	r_misc.$O\
	r_part.$O\
	r_sky.$O\
	r_sprite.$O\
	r_surf.$O\
	screen.$O\
	sbar.$O\
	snd.$O\
	snd_plan9.$O\
	softfloat.$O\
	sv_main.$O\
	sv_move.$O\
	sv_phys.$O\
	sv_user.$O\
	vid.$O\
	view.$O\
	wad.$O\
	world.$O\
	zone.$O\

HFILES=\
	dat.h\
	fns.h\
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
	draw.h\
	input.h\
	keys.h\
	mathlib.h\
	menu.h\
	modelgen.h\
	model.h\
	net.h\
	pr_comp.h\
	progdefs.h\
	progs.h\
	protocol.h\
	quakedef.h\
	render.h\
	r_local.h\
	r_shared.h\
	sbar.h\
	screen.h\
	server.h\
	softfloat.h\
	spritegn.h\
	vid.h\
	view.h\
	wad.h\
	world.h\
	zone.h\

</sys/src/cmd/mkone

dotadd.$O: dotadd.c
	$CC $CFLAGS -p dotadd.c

softfloat.$O: softfloat.c
	$CC $CFLAGS -p softfloat.c
