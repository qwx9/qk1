BASEVERSION=1.09
VERSION=1.09
ARCH=$(shell uname -m)
BASE_CFLAGS=-Dstricmp=strcasecmp
DEBUG_CFLAGS=$(BASE_CFLAGS) -ggdb -O0 -trigraphs -Wall -Wextra -m32
LDFLAGS=-lm
XLDFLAGS= -lX11 -lXext -lXxf86dga
XCFLAGS=-DX11
DO_CC=$(CC) $(CFLAGS) -o $@ -c $<
DO_X11_CC=$(CC) $(CFLAGS) $(XCFLAGS) -o $@ -c $<
DO_O_CC=$(CC) -O $(CFLAGS) -o $@ -c $<
DO_AS=$(CC) $(CFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<
CFLAGS=$(DEBUG_CFLAGS)

all: qk1

X11_OBJS =\
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
	zone.o\
	view.o\
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
	sys_dosa.o\

qk1: $(X11_OBJS)
	$(CC) $(CFLAGS) -o $@ $(X11_OBJS) $(XLDFLAGS) $(LDFLAGS)

####

cl_demo.o :  cl_demo.c
	$(DO_X11_CC)

cl_input.o : cl_input.c
	$(DO_X11_CC)

cl_main.o :  cl_main.c
	$(DO_X11_CC)

cl_parse.o : cl_parse.c
	$(DO_X11_CC)

cl_tent.o :  cl_tent.c
	$(DO_X11_CC)

chase.o :    chase.c
	$(DO_X11_CC)

cmd.o :      cmd.c
	$(DO_X11_CC)

common.o :   common.c
	$(DO_X11_CC)

console.o :  console.c
	$(DO_X11_CC)

crc.o :      crc.c
	$(DO_X11_CC)

cvar.o :     cvar.c
	$(DO_X11_CC)

draw.o :     draw.c
	$(DO_X11_CC)

d_edge.o :   d_edge.c
	$(DO_X11_CC)

d_fill.o :   d_fill.c
	$(DO_X11_CC)

d_init.o :   d_init.c
	$(DO_X11_CC)

d_modech.o : d_modech.c
	$(DO_X11_CC)

d_part.o :   d_part.c
	$(DO_X11_CC)

d_polyse.o : d_polyse.c
	$(DO_X11_CC)

d_scan.o :   d_scan.c
	$(DO_X11_CC)

d_sky.o :    d_sky.c
	$(DO_X11_CC)

d_sprite.o : d_sprite.c
	$(DO_X11_CC)

d_surf.o :   d_surf.c
	$(DO_X11_CC)

d_vars.o :   d_vars.c
	$(DO_X11_CC)

d_zpoint.o : d_zpoint.c
	$(DO_X11_CC)

host.o :     host.c
	$(DO_X11_CC)

host_cmd.o : host_cmd.c
	$(DO_X11_CC)

keys.o :     keys.c
	$(DO_X11_CC)

menu.o :     menu.c
	$(DO_X11_CC)

mathlib.o :  mathlib.c
	$(DO_X11_CC)

model.o :    model.c
	$(DO_X11_CC)

net_dgrm.o : net_dgrm.c
	$(DO_X11_CC)

net_loop.o : net_loop.c
	$(DO_X11_CC)

net_main.o : net_main.c
	$(DO_X11_CC)

net_vcr.o :  net_vcr.c
	$(DO_X11_CC)

net_udp.o :  net_udp.c
	$(DO_X11_CC)

net_bsd.o :  net_bsd.c
	$(DO_X11_CC)

nonintel.o : nonintel.c
	$(DO_X11_CC)

pr_cmds.o :  pr_cmds.c
	$(DO_X11_CC)

pr_edict.o : pr_edict.c
	$(DO_X11_CC)

pr_exec.o :  pr_exec.c
	$(DO_X11_CC)

r_aclip.o :  r_aclip.c
	$(DO_X11_CC)

r_alias.o :  r_alias.c
	$(DO_X11_CC)

r_bsp.o :    r_bsp.c
	$(DO_X11_CC)

r_light.o :  r_light.c
	$(DO_X11_CC)

r_draw.o :   r_draw.c
	$(DO_X11_CC)

r_efrag.o :  r_efrag.c
	$(DO_X11_CC)

r_edge.o :   r_edge.c
	$(DO_X11_CC)

r_misc.o :   r_misc.c
	$(DO_X11_CC)

r_main.o :   r_main.c
	$(DO_X11_CC)

r_sky.o :    r_sky.c
	$(DO_X11_CC)

r_sprite.o : r_sprite.c
	$(DO_X11_CC)

r_surf.o :   r_surf.c
	$(DO_X11_CC)

r_part.o :   r_part.c
	$(DO_X11_CC)

r_vars.o :   r_vars.c
	$(DO_X11_CC)

screen.o :   screen.c
	$(DO_X11_CC)

sbar.o :     sbar.c
	$(DO_X11_CC)

sv_main.o :  sv_main.c
	$(DO_X11_CC)

sv_phys.o :  sv_phys.c
	$(DO_X11_CC)

sv_move.o :  sv_move.c
	$(DO_X11_CC)

sv_user.o :  sv_user.c
	$(DO_X11_CC)

zone.o	:   zone.c
	$(DO_X11_CC)

view.o	:   view.c
	$(DO_X11_CC)

wad.o :      wad.c
	$(DO_X11_CC)

world.o :    world.c
	$(DO_X11_CC)

cd_linux.o : cd_linux.c
	$(DO_X11_CC)

sys_linux.o :sys_linux.c
	$(DO_X11_CC)

vid_x.o: vid_x.c
	$(DO_O_CC)

snd_dma.o :  snd_dma.c
	$(DO_X11_CC)

snd_mem.o :  snd_mem.c
	$(DO_X11_CC)

snd_mix.o :  snd_mix.c
	$(DO_X11_CC)

snd_linux.o :snd_linux.c
	$(DO_X11_CC)

d_copy.o :   d_copy.s
	$(DO_AS)

d_draw.o :   d_draw.s
	$(DO_AS)

d_draw16.o : d_draw16.s
	$(DO_AS)

d_parta.o :  d_parta.s
	$(DO_AS)

d_polysa.o : d_polysa.s
	$(DO_AS)

d_scana.o :  d_scana.s
	$(DO_AS)

d_spr8.o :   d_spr8.s
	$(DO_AS)

d_varsa.o :  d_varsa.s
	$(DO_AS)

math.o :     math.s
	$(DO_AS)

r_aliasa.o : r_aliasa.s
	$(DO_AS)

r_drawa.o :  r_drawa.s
	$(DO_AS)

r_edgea.o :  r_edgea.s
	$(DO_AS)

r_varsa.o :  r_varsa.s
	$(DO_AS)

surf16.o :   surf16.s
	$(DO_AS)

surf8.o :    surf8.s
	$(DO_AS)

worlda.o :   worlda.s
	$(DO_AS)

r_aclipa.o : r_aclipa.s
	$(DO_AS)

snd_mixa.o : snd_mixa.s
	$(DO_AS)

sys_dosa.o : sys_dosa.s
	$(DO_AS)


clean:
	rm -f qk1 $(X11_OBJS)
