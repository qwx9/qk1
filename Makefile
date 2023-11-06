TARG=qk1
DESTDIR?=
PREFIX?=/usr/local
BIN=${DESTDIR}${PREFIX}/bin
MAN=${DESTDIR}${PREFIX}/share/man/man1
SDL2_CFLAGS=$$(pkg-config --cflags sdl2)
SDL2_LDFLAGS=$$(pkg-config --libs sdl2)
CFLAGS?=-O2 -pipe -g -Wall
CFLAGS+=-fms-extensions -Iunix -I. ${SDL2_CFLAGS}
LDFLAGS?=
LDFLAGS+=-lm ${SDL2_LDFLAGS}

OBJS=\
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
	d_alpha.o\
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
	dotproduct.o\
	draw.o\
	host.o\
	host_cmd.o\
	keys.o\
	mathlib.o\
	menu.o\
	model.o\
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
	r_vars.o\
	sbar.o\
	screen.o\
	span.o\
	span_alpha.o\
	sv_main.o\
	sv_move.o\
	sv_phys.o\
	sv_user.o\
	unix/cd.o\
	unix/fs.o\
	unix/in.o\
	unix/net_udp.o\
	unix/qk1.o\
	unix/seprint.o\
	unix/snd.o\
	unix/vid.o\
	view.o\
	wad.o\
	world.o\
	zone.o\

.PHONY: all default install uninstall clean

all: default

default: ${TARG}

install: ${TARG} ${TARG}.1
	install -d ${BIN}
	install -m 755 ${TARG} ${BIN}
	install -d ${MAN}
	install -m 644 ${TARG}.1 ${MAN}

uninstall:
	rm -f ${BIN}/${TARG}
	rm -f ${MAN}/${TARG}.1

${TARG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

.SUFFIXES: .c .o
.c.o:
	${CC} -o $@ -c $< ${CFLAGS}

clean:
	rm -f ${TARG} ${OBJS}
