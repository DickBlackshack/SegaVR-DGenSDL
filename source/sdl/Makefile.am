# DGen/SDL v1.26+
# SDL interface

CC_FOR_BUILD=@CC_FOR_BUILD@
CFLAGS_FOR_BUILD=@CFLAGS_FOR_BUILD@
OBJEXT_FOR_BUILD=@OBJEXT_FOR_BUILD@
EXEEXT_FOR_BUILD=@EXEEXT_FOR_BUILD@

AM_CPPFLAGS = -I$(top_srcdir) @SDL_CFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/hqx/src -DHQX_NO_CALLCONV -DHQX_NO_API
AM_CPPFLAGS += -I$(top_srcdir)/scale2x
AM_CPPFLAGS += -I$(top_srcdir)/dz80 -D_DZ80_EXCLUDE_SCRIPT

SUFFIXES = .pbm

.pbm.cpp:
	./$(PBM2DF) $< $@

PBM2DF = pbm2df$(EXEEXT_FOR_BUILD)
DIR = $(top_srcdir)/sdl

$(PBM2DF):				\
	$(DIR)/pbm2df.c			\
	$(DIR)/dgenfont_16x26.pbm	\
	$(DIR)/dgenfont_8x13.pbm	\
	$(DIR)/dgenfont_7x5.pbm
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) -o $@ $(DIR)/pbm2df.c

BUILT_SOURCES = 		\
	dgenfont_16x26.cpp	\
	dgenfont_8x13.cpp	\
	dgenfont_7x5.cpp

$(BUILT_SOURCES): $(PBM2DF)

noinst_LIBRARIES = libpd.a
libpd_a_SOURCES =	\
	font.cpp	\
	sdl.cpp		\
	font.h		\
	pd-defs.h	\
	prompt.h	\
	prompt.c	\
	splash.h	\
	$(BUILT_SOURCES)
libpd_a_DEPENDENCIES = $(PBM2DF)

DISTCLEANFILES = $(BUILT_SOURCES) $(PBM2DF)

.NOTPARALLEL:
