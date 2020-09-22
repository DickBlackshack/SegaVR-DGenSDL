# DGen/SDL v1.26+

include $(top_srcdir)/Makefile.asm.am

DIST_SUBDIRS = musa mz80 sdl star cyclone doc

# SDL
SUBDIRS = sdl
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/sdl
dgen_DEPENDENCIES = sdl/libpd.a
dgen_LDADD = sdl/libpd.a

# Musashi
if WITH_MUSA
SUBDIRS += musa
dgen_DEPENDENCIES += musa/libmusa68.a
dgen_LDADD += musa/libmusa68.a
endif

# MZ80
if WITH_MZ80
SUBDIRS += mz80
dgen_DEPENDENCIES += mz80/libmz80.a
dgen_LDADD += mz80/libmz80.a
endif

# DOCS
SUBDIRS += doc

# StarScream
if WITH_STAR
SUBDIRS += star
dgen_DEPENDENCIES += star/libstarcpu.a
dgen_LDADD += star/libstarcpu.a
endif

# Cyclone 68000
if WITH_CYCLONE
SUBDIRS += cyclone
dgen_DEPENDENCIES += cyclone/libcyclonecpu.a
dgen_LDADD += cyclone/libcyclonecpu.a
endif

bin_PROGRAMS = dgen dgen_tobin

man_MANS = dgen.1 dgenrc.5 dgen_tobin.1

EXTRA_DIST = sample.dgenrc

# dgen
dgen_LDADD += $(DGEN_LIBS)
dgen_SOURCES =		\
	rc.h		\
	rc-vars.h	\
	rc.cpp		\
	romload.c	\
	md.h		\
	md.cpp		\
	mdfr.cpp	\
	decode.h	\
	decode.c	\
	vdp.cpp		\
	save.cpp	\
	graph.cpp	\
	fm.h		\
	fm.c		\
	myfm.cpp	\
	sn76496.h	\
	sn76496.c	\
	ras-drawplane.h	\
	ras.cpp		\
	main.cpp	\
	mem.cpp		\
	pd.h		\
	ckvp.c		\
	ckvp.h		\
	joystick.cpp	\
	system.c

# debugger
if WITH_DEBUGGER
AM_CPPFLAGS += -DUSE_UTF8
dgen_SOURCES +=			\
	debug.cpp		\
	linenoise/linenoise.h	\
	linenoise/linenoise.c	\
	linenoise/utf8.c
endif

# dZ80
if WITH_DZ80
AM_CPPFLAGS += -I$(top_srcdir)/dz80 -D_DZ80_EXCLUDE_SCRIPT
dgen_SOURCES += 		\
	dz80/dissz80.c		\
	dz80/dissz80.h		\
	dz80/dissz80p.h		\
	dz80/types.h		\
	dz80/tables.c		\
	dz80/noscript.c
endif

# CZ80
if WITH_CZ80
AM_CPPFLAGS += -I$(top_srcdir)/cz80
dgen_SOURCES += cz80/cz80.c
endif

# DrZ80
if WITH_DRZ80
AM_CPPFLAGS += -I$(top_srcdir)/drz80
dgen_SOURCES += drz80/drz80.s
endif

# hqx
if WITH_HQX
AM_CPPFLAGS += -I$(top_srcdir)/hqx/src -DHQX_NO_CALLCONV -DHQX_NO_API
dgen_SOURCES +=			\
	hqx/src/init.c		\
	hqx/src/hq2x_32.c	\
	hqx/src/hq2x_24.c	\
	hqx/src/hq2x_16.c	\
	hqx/src/hq2x-int.h	\
	hqx/src/hq3x_32.c	\
	hqx/src/hq3x_24.c	\
	hqx/src/hq3x_16.c	\
	hqx/src/hq3x-int.h	\
	hqx/src/hq4x_32.c	\
	hqx/src/hq4x_24.c	\
	hqx/src/hq4x_16.c	\
	hqx/src/hq4x-int.h	\
	hqx/src/hqx.h		\
	hqx/src/common.h
endif

# scale2x
if WITH_SCALE2X
AM_CPPFLAGS += -I$(top_srcdir)/scale2x
dgen_SOURCES +=			\
	scale2x/scale2x.c	\
	scale2x/scale2x.h	\
	scale2x/scale3x.c	\
	scale2x/scale3x.h	\
	scale2x/scalebit.c	\
	scale2x/scalebit.h
endif

if WITH_X86_TILES
dgen_SOURCES += x86_tiles.asm
endif

if WITH_X86_CTV
dgen_SOURCES += x86_ctv.asm
endif

if WITH_X86_ASM
if WITH_X86_MMX
dgen_SOURCES += x86_mmx_memcpy.asm
else
dgen_SOURCES += x86_memcpy.asm
endif
endif

# dgen_tobin
dgen_tobin_SOURCES = tobin.c romload.c system.c
