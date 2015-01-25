<$PLAN9/src/mkhdr

# [words]
CFLAGS= -D_DEFAULT_SOURCE -Dstricmp=strcasecmp -DX11 -O0 -ggdb -Wall -trigraphs
LDFLAGS= -lX11 -lXext -lXxf86dga
AFLAGS= $CFLAGS -DELF -x assembler-with-cpp

BIN=.
TARG=qk1

OFILES=\
	cl_demo.o\
	cl_input.o\
	cl_main.o\
	cl_parse.o\
	cl_tent.o\
	chase.o\
	cmd.o\
	common.o\
	console.o\
	crc.o\
	cvar.o\
	draw.o\
	d_edge.o\
	d_fill.o\
	d_init.o\
	d_modech.o\
	d_part.o\
	d_polyse.o\
	d_scan.o\
	d_sky.o\
	d_sprite.o\
	d_surf.o\
	d_vars.o\
	d_zpoint.o\
	host.o\
	host_cmd.o\
	keys.o\
	menu.o\
	mathlib.o\
	model.o\
	net_dgrm.o\
	net_loop.o\
	net_main.o\
	net_vcr.o\
	net_udp.o\
	net_bsd.o\
	nonintel.o\
	pr_cmds.o\
	pr_edict.o\
	pr_exec.o\
	r_aclip.o\
	r_alias.o\
	r_bsp.o\
	r_light.o\
	r_draw.o\
	r_efrag.o\
	r_edge.o\
	r_misc.o\
	r_main.o\
	r_sky.o\
	r_sprite.o\
	r_surf.o\
	r_part.o\
	r_vars.o\
	screen.o\
	sbar.o\
	sv_main.o\
	sv_phys.o\
	sv_move.o\
	sv_user.o\
	zone.o	\
	view.o	\
	wad.o\
	world.o\
	cd_linux.o\
	sys_linux.o\
	vid_x.o\
	snd_dma.o\
	snd_mem.o\
	snd_mix.o\
	snd_linux.o\
	d_draw.o\
	d_draw16.o\
	d_parta.o\
	d_polysa.o\
	d_scana.o\
	d_spr8.o\
	d_varsa.o\
	math.o\
	r_aliasa.o\
	r_drawa.o\
	r_edgea.o\
	r_varsa.o\
	surf16.o\
	surf8.o\
	worlda.o\
	r_aclipa.o\
	snd_mixa.o\
	#sys_dosa.o\

HFILES=\
	adivtab.h\
	anorms.h\
	asm_draw.h\
	asm_i386.h\
	block16.h\
	bspfile.h\
	cdaudio.h\
	client.h\
	cmd.h\
	common.h\
	console.h\
	crc.h\
	cvar.h\
	d_ifacea.h\
	d_iface.h\
	d_local.h\
	draw.h\
	input.h\
	keys.h\
	mathlib.h\
	modelgen.h\
	model.h\
	net_dgrm.h\
	net.h\
	net_loop.h\
	net_udp.h\
	net_vcr.h\
	pr_comp.h\
	progdefs.h\
	progs.h\
	protocol.h\
	quakeasm.h\
	quakedef.h\
	render.h\
	r_local.h\
	r_shared.h\
	sbar.h\
	screen.h\
	server.h\
	sound.h\
	spritegn.h\
	sys.h\
	vid.h\
	view.h\
	wad.h\
	world.h\
	zone.h\

<$PLAN9/src/mkone

AS=gcc

%.$O:	%.s
	$AS $AFLAGS -o $target -c $stem.s
