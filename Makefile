TARG=qk1
DESTDIR?=
PREFIX?=/usr/local
BIN=${DESTDIR}${PREFIX}/bin
SDL2_CFLAGS=$$(pkg-config --cflags sdl2)
SDL2_LDFLAGS=$$(pkg-config --libs sdl2)
CFLAGS?=-O2 -g
CFLAGS+=-Wall -Wextra -Wno-unknown-pragmas -Wno-missing-field-initializers -Wno-implicit-fallthrough
CFLAGS+=-ffast-math -fms-extensions
CFLAGS+=-I3rd/parg -Iunix -I. ${SDL2_CFLAGS}
LDFLAGS?=
LDFLAGS+=-lm ${SDL2_LDFLAGS}

OBJS=\
	3rd/parg/parg.o\
	chase.o\
	cl_demo.o\
	cl_input.o\
	cl_main.o\
	cl_parse.o\
	cl_tent.o\
	cmd.o\
	common.o\
	console.o\
	cvar.o\
	dotadd.o\
	d_alpha.o\
	d_edge.o\
	d_init.o\
	d_modech.o\
	d_part.o\
	d_polyse.o\
	d_scan.o\
	d_sky.o\
	d_sprite.o\
	d_surf.o\
	d_vars.o\
	dotproduct.o\
	draw.o\
	fs.o\
	host.o\
	host_cmd.o\
	keys.o\
	mathlib.o\
	menu.o\
	model.o\
	model_alias.o\
	model_brush.o\
	model_bsp.o\
	model_bsp2.o\
	model_bsp30.o\
	model_sprite.o\
	net_loop.o\
	net_main.o\
	pal.o\
	pr_cmds.o\
	pr_edict.o\
	pr_exec.o\
	protocol.o\
	r_aclip.o\
	r_alias.o\
	r_bsp.o\
	r_draw.o\
	r_edge.o\
	r_efrag.o\
	r_light.o\
	r_main.o\
	r_misc.o\
	r_part.o\
	r_sky.o\
	r_sprite.o\
	r_surf.o\
	sbar.o\
	screen.o\
	snd.o\
	softfloat.o\
	span.o\
	span_alpha.o\
	sv_main.o\
	sv_move.o\
	sv_phys.o\
	sv_user.o\
	unix/cd.o\
	unix/in.o\
	unix/net_udp.o\
	unix/qk1.o\
	unix/seprint.o\
	unix/snd_sdl.o\
	unix/vid.o\
	view.o\
	wad.o\
	world.o\
	zone.o\

HDRS=\
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
	input.h\
	keys.h\
	mathlib.h\
	menu.h\
	model.h\
	modelgen.h\
	net.h\
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
	spritegn.h\
	unix/libc.h\
	unix/u.h\
	vid.h\
	view.h\
	wad.h\
	world.h\
	zone.h\

.PHONY: all default install uninstall clean

all: default

default: ${TARG}

install: ${TARG}
	install -d ${BIN}
	install -m 755 ${TARG} ${BIN}

uninstall:
	rm -f ${BIN}/${TARG}

${TARG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

.SUFFIXES: .c .o
.c.o:
	${CC} -o $@ -c $< ${CFLAGS}

${OBJS}: ${HDRS}

clean:
	rm -f ${TARG} ${OBJS}
