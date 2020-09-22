dnl DGen/SDL v1.33
dnl At long last, the configure script!

AC_INIT(
	[DGen/SDL],
	[1.33],
	[zamaz@users.sourceforge.net],
	[dgen-sdl],
	[http://sourceforge.net/projects/dgen],
)

dnl We'll need machine type later.
AC_CANONICAL_SYSTEM

dnl Initialize Automake.
AM_INIT_AUTOMAKE([silent-rules])
AM_SILENT_RULES([yes])
AM_PROG_AS

dnl Check for standard programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB

dnl Set the value of make, if make doesn't already.
AC_PROG_MAKE_SET

dnl We're using C.
AC_LANG([C])

dnl Check endianness.
AC_C_BIGENDIAN

dnl Cross compilation stuff.
AS_IF(
	[test "x$cross_compiling" = xno],
	[CC_FOR_BUILD="${CC}"]
	[CXX_FOR_BUILD="${CXX}"]
	[CPPFLAGS_FOR_BUILD="${CPPFLAGS}"]
	[CFLAGS_FOR_BUILD="${CFLAGS}"]
	[CXXFLAGS_FOR_BUILD="${CXXFLAGS}"]
	[LDFLAGS_FOR_BUILD="${LDFLAGS}"]
	[OBJEXT_FOR_BUILD="${OBJEXT}"]
	[EXEEXT_FOR_BUILD="${EXEEXT}"]
)
AS_IF(
	[test "x$cross_compiling" = xyes],
	[AC_MSG_CHECKING([for host C compiler])]
	[CC_FOR_BUILD="${CC_FOR_BUILD-cc}"]
	[AC_MSG_RESULT([$CC_FOR_BUILD])]
	[AC_MSG_CHECKING([for host C++ compiler])]
	[CXX_FOR_BUILD="${CXX_FOR_BUILD-c++}"]
	[AC_MSG_RESULT([$CXX_FOR_BUILD])]
	[AC_MSG_CHECKING([for suffix of host object files])]
	[
{
	# Sane values by default.
	OBJEXT_FOR_BUILD=o
	EXEEXT_FOR_BUILD=
	# Try to compile a program, find out default extensions.
	EXT_SUCCESS=0
	rm -f conftest*
	cat <<EOF > conftest.c &&
int main(void) { return 0; }
EOF
	$CC_FOR_BUILD -c conftest.c &&
	rm -f conftest.c &&
	for file in conftest.*
	do
		test -f $file ||
		! continue
		OBJEXT_FOR_BUILD="${file##*.}"
		test -z "$OBJEXT_FOR_BUILD" &&
		! continue
		break
	done &&
	for ext in '' '.exe'
	do
		$CC_FOR_BUILD -o conftest$ext $file &&
		./conftest$ext ||
		! continue
		EXEEXT_FOR_BUILD="$ext"
		break
	done &&
	EXT_SUCCESS=1
	rm -f conftest*
} 1>&AS_MESSAGE_LOG_FD 2>&1
	]
	[AS_IF(
		[test "x$EXT_SUCCESS" = x1],
		[AC_MSG_RESULT([$OBJEXT_FOR_BUILD])]
		[AC_MSG_CHECKING([for suffix of host executables])]
		[AS_IF(
			[test "x$EXEEXT_FOR_BUILD" = x],
			[AC_MSG_RESULT([none])],
			[AC_MSG_RESULT([$EXEEXT_FOR_BUILD])]
		)],
		[AC_MSG_RESULT([unable to determine])]
		[AC_MSG_FAILURE([can't cross-compile if host objects \
extension isn't known])]
	)]
)

dnl Libraries used by dgen_tobin.
TOBIN_LIBS="${LIBS} ${TOBIN_LIBS}"

dnl Check for SDL libs.
AM_PATH_SDL([1.2.0], [], [AC_MSG_ERROR([SDL version >= 1.2.0 not found.])])
CFLAGS="$CFLAGS $SDL_CFLAGS"
CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

dnl Check for the SDL_WM_ToggleFullScreen function.
AC_CHECK_FUNCS([SDL_WM_ToggleFullScreen])

dnl Check for ftello().
AC_CHECK_FUNCS([ftello])

dnl Debugging?
AC_ARG_ENABLE(
	[debug],
	[AS_HELP_STRING(
		[--enable-debug],
		[enable internal debugging [default=no]]
	)],
	[USE_DEBUG=$enableval],
	[USE_DEBUG=no]
)

dnl Check for joystick support.
AC_ARG_ENABLE(
	[joystick],
	[AS_HELP_STRING(
		[--enable-joystick],
		[enable joystick support [default=auto]]
	)],
	[USE_JOYSTICK=$enableval],
	[USE_JOYSTICK=auto]
)
AS_IF(
	[test "x$USE_JOYSTICK" != xno],
	[AC_CHECK_HEADER(
		[SDL_joystick.h],
		[USE_JOYSTICK=yes],
		[USE_JOYSTICK=no]
	)]
)
AS_IF(
	[test "x$enable_joystick" = xyes -a "x$USE_JOYSTICK" = xno],
	[AC_MSG_FAILURE([joystick support seems unavailable])]
)

dnl Check for OpenGL support.
AC_ARG_ENABLE(
	[opengl],
	[AS_HELP_STRING(
		[--enable-opengl],
		[enable OpenGL support [default=auto]]
	)],
	[USE_OPENGL=$enableval],
	[USE_OPENGL=auto]
)
AS_IF(
	[test "x$USE_OPENGL" != xno],
	[AC_CHECK_HEADER(
		[SDL_opengl.h],
		[AC_SEARCH_LIBS(
			[glEnd],
			[GL opengl32 gl],
			[USE_OPENGL=yes],
			[AS_IF(
				[test "x$cross_compiling" = xyes],
				dnl When nothing works, use brute force.
				[USE_OPENGL=no]
				[AC_CHECK_LIB(
					[GL],
					[main],
					[LIBS="-lGL $LIBS"]
					[USE_OPENGL=yes]
				)]
				[AC_CHECK_LIB(
					[opengl32],
					[main],
					[LIBS="-lopengl32 $LIBS"]
					[USE_OPENGL=yes]
				)]
				[AC_CHECK_LIB(
					[gl],
					[main],
					[LIBS="-lgl $LIBS"]
					[USE_OPENGL=yes]
				)],
				[USE_OPENGL=no]
			)]
		)],
		[USE_OPENGL=no]
	)]
)
AS_IF(
	[test "x$enable_opengl" = xyes -a "x$USE_OPENGL" = xno],
	[AC_MSG_FAILURE([OpenGL support seems unavailable])]
)

dnl Check for threads support.
AC_ARG_ENABLE(
	[threads],
	[AS_HELP_STRING(
		[--enable-threads],
		[enable multi-threading support [default=auto]]
	)],
	[USE_THREADS=$enableval],
	[USE_THREADS=auto]
)
AS_IF(
	[test "x$USE_THREADS" != xno],
	[AC_CHECK_HEADER(
		[SDL_thread.h],
		[USE_THREADS=yes],
		[USE_THREADS=no]
	)]
)
AS_IF(
	[test "x$enable_threads" = xyes -a "x$USE_THREADS" = xno],
	[AC_MSG_FAILURE([multi-threading support seems unavailable])]
)

dnl Check for libarchive. Note that older versions of libarchive don't have
dnl archive_read_support_format_raw().
AC_ARG_WITH(
	[libarchive],
	[AS_HELP_STRING(
		[--with-libarchive],
		[use libarchive for compressed ROMs [default=auto]]
	)],
	[USE_LIBARCHIVE=$withval],
	[USE_LIBARCHIVE=auto]
)
AS_IF(
	[test "x$USE_LIBARCHIVE" != xno],
	[AC_CHECK_HEADER(
		[archive.h],
		[AC_CHECK_LIB(
			[archive],
			[archive_read_support_format_raw],
			[LIBS="-larchive $LIBS"]
			[TOBIN_LIBS="-larchive $TOBIN_LIBS"]
			[USE_LIBARCHIVE=yes],
			[USE_LIBARCHIVE=no]
		)],
		[USE_LIBARCHIVE=no]
	)]
)

dnl Check for glob.h.
AC_CHECK_HEADERS([glob.h])

dnl Try to add shell32.
AC_CHECK_LIB([shell32], [main])

dnl Check for ASM availability.
AC_ARG_ENABLE(
	[asm],
	[AS_HELP_STRING(
		[--disable-asm],
		[disable all ASM-related options [default=auto]]
	)],
	[USE_ASM=$enableval],
	[USE_ASM=auto]
)
USE_X86_ASM=no
USE_ARM_ASM=no
AS_IF(
	[test "x$USE_ASM" != xno],
	[AS_CASE(
		[$target],

		[i?86-*],
		[AC_PATH_PROG([NASM], [nasm])]
		[AS_IF(
			[test "x$NASM" != x],
			[USE_ASM=yes]
			[USE_X86_ASM=yes],
			[USE_ASM=no]
		)],

		[arm*-*],
		[AC_MSG_NOTICE([ARM-specific ASM routines enabled])]
		[USE_ASM=yes]
		[USE_ARM_ASM=yes],

		[AC_MSG_NOTICE([no ASM routines available for this target])]
		[USE_ASM=unavailable]
		[NASM=]
	)]
)
AS_IF(
	[test "x$enable_asm" = xyes -a "x$USE_ASM" = xno],
	[AC_MSG_FAILURE([unable to enable ASM routines])]
)

dnl Check for NASM output format.
AC_ARG_WITH(
	[nasm-format],
	[AC_HELP_STRING(
		[--with-nasm-format[[=format]]],
		[use this output format for NASM (see "nasm -hf" output for]
		[available formats) [default=auto]]
	)],
	[NASM_FORMAT=$withval],
	[AS_IF([test "x$NASM" != x], [NASM_FORMAT=auto], [NASM_FORMAT=])]
)
AS_IF(
	[test "x$NASM_FORMAT" = xauto],
	[AC_MSG_CHECKING([for nasm output format])]
	[AS_CASE(
		[$target],

		[i?86-*mingw*],
		[NASM_FORMAT=win32],

		[x86_64-*mingw*],
		[NASM_FORMAT=win64],

		[i?86-*],
		[NASM_FORMAT=elf32],

		[x86_64-*],
		[NASM_FORMAT=elf64],

		[AC_MSG_RESULT([failed])]
		[AC_MSG_ERROR(
			[can't determine what format NASM should use, \
please specify --with-nasm-format=format]
		)]
	)]
	[AC_MSG_RESULT([$NASM_FORMAT])]
	[AC_MSG_WARN([NASM format auto-detection is crappy at best])],
	[AS_IF(
		[test "x$NASM_FORMAT" != x],
		[AC_MSG_NOTICE([NASM output format is $NASM_FORMAT])]
	)]
)

dnl Check for the need to prefix ASM globals with underscores.
dnl For ELF, also add -DNASM_STACK_NOEXEC.
AS_IF(
	[! test -z "$NASM_FORMAT"],
	[AC_MSG_CHECKING([whether ASM globals require an underscore])]
	[AS_CASE(
		[$NASM_FORMAT],

		[elf*],
		[AC_MSG_RESULT([no])]
		[NASM="$NASM -DNASM_STACK_NOEXEC"],

		[AC_MSG_RESULT([yes])]
		[NASM="$NASM --prefix _"]
	)]
)

dnl Crap TV filters support.
AC_ARG_ENABLE(
	[ctv],
	[AC_HELP_STRING([--disable-ctv], [disable Crap TV filters])],
	[USE_CTV=$enableval],
	[USE_CTV=yes]
)

dnl hqx filters support.
AC_ARG_ENABLE(
	[hqx],
	[AS_HELP_STRING([--disable-hqx], [disable hqx filters])],
	[USE_HQX=$enableval],
	[USE_HQX=yes]
)

dnl scale2x filters support.
AC_ARG_ENABLE(
	[scale2x],
	[AS_HELP_STRING([--disable-scale2x], [disable scale2x filters])],
	[USE_SCALE2X=$enableval],
	[USE_SCALE2X=yes]
)

dnl x86-specific ASM options.
AC_ARG_ENABLE(
	[x86-mmx],
	[AC_HELP_STRING([--enable-x86-mmx], [enable MMX memcpy])],
	dnl If --disable-asm isn't specified, MMX must be auto-detected,
	dnl unless forced via --enable-x86-mmx.
	[USE_X86_MMX=$enableval],
	[AS_IF(
		[test "x$USE_X86_ASM" != xno],
		[USE_X86_MMX=auto],
		[USE_X86_MMX=no]
	)]
)
AC_ARG_ENABLE(
	[x86-ctv],
	[AC_HELP_STRING([--enable-x86-ctv], [enable ASM Crap TV filters])],
	dnl Disable this if --disable-ctv is given.
	[AS_IF(
		[test "x$USE_CTV" != xno],
		[USE_X86_CTV=$enableval],
		[USE_X86_CTV=no]
	)],
	[AS_IF(
		[test "x$USE_CTV" != xno],
		[USE_X86_CTV=$USE_X86_ASM],
		[USE_X86_CTV=no]
	)]
)
AC_ARG_ENABLE(
	[x86-tiles],
	[AC_HELP_STRING([--enable-x86-tiles], [enable ASM tiles])],
	[USE_X86_TILES=$enableval],
	[USE_X86_TILES=$USE_X86_ASM]
)
AC_ARG_ENABLE(
	[x86-mz80],
	[AC_HELP_STRING([--enable-x86-mz80], [use ASM version of MZ80])],
	[USE_X86_MZ80=$enableval],
	[USE_X86_MZ80=$USE_X86_ASM]
)

dnl If MMX is to be automatically enabled, make sure it is actually supported.
AS_IF(
	[test "x$USE_X86_MMX" = xauto],
	[AC_MSG_CHECKING([for MMX support])]
	[AS_IF(
		[test "x$cross_compiling" = xyes],
		USE_X86_MMX=$USE_X86_ASM
		[AC_MSG_RESULT([$USE_X86_ASM (untested)])],
		[
{
	cat <<EOF > conftest.asm &&
bits 32
section .text
global main
main:
	emms
	xor eax, eax	; Return value of 0
	ret
EOF
	$NASM -f $NASM_FORMAT -o conftest.$OBJEXT -- conftest.asm &&
	$CC -o conftest$EXEEXT conftest.$OBJEXT &&
	./conftest$EXEEXT &&
	USE_X86_MMX=yes ||
	USE_X86_MMX=no
	rm -f conftest*
} 1>&AS_MESSAGE_LOG_FD 2>&1
		]
		[AC_MSG_RESULT([$USE_X86_MMX])]
	)]
)

AS_IF(
	[test "x$USE_X86_ASM" = xno &&	\
	 test "x$USE_X86_MMX" = xyes -o	\
	 "x$USE_X86_CTV" = xyes -o	\
	 "x$USE_X86_TILES" = xyes -o	\
	 "x$USE_X86_MZ80" = xyes],
	[AC_MSG_FAILURE(
		[x86 ASM support is unavailable, you can't use x86 options]
	)]
)

dnl CPU emulators.
AC_ARG_WITH(
	[star],
	[AS_HELP_STRING(
		[--with-star],
		[include StarScream CPU core [default=auto]]
	)],
	[WITH_STAR=$withval],
	[WITH_STAR=$USE_X86_ASM]
)
AS_IF(
	[test "x$WITH_STAR" = xyes -a "x$USE_X86_ASM" = xno],
	[AC_MSG_FAILURE(
		[StarScream can only be enabled on x86 targets with a \
working ASM support]
	)]
)

AC_ARG_WITH(
	[musa],
	[AS_HELP_STRING(
		[--with-musa],
		[include Musashi CPU core [default=yes]]
	)],
	[WITH_MUSA=$withval],
	[WITH_MUSA=yes]
)
AS_IF(
	[test "x$WITH_MUSA" != xyes -a "x$WITH_STAR" != xyes],
	[AC_MSG_WARN([neither Musashi nor StarScream are enabled])]
)

AC_ARG_WITH(
	[cyclone],
	[AS_HELP_STRING(
		[--with-cyclone],
		[include Cyclone 68000 CPU core [default=auto]]
	)],
	[WITH_CYCLONE=$withval],
	[WITH_CYCLONE=$USE_ARM_ASM]
)
AS_IF(
	[test "x$WITH_CYCLONE" = xyes -a "x$USE_ARM_ASM" = xno],
	[AC_MSG_FAILURE(
		[Cyclone 68000 can only be enabled on ARM targets with a \
working ASM support]
	)]
)

AC_ARG_WITH(
	[mz80],
	[AS_HELP_STRING(
		[--with-mz80],
		[include MZ80 CPU core [default=yes]]
	)],
	[WITH_MZ80=$withval],
	[WITH_MZ80=yes]
)

AC_ARG_WITH(
	[cz80],
	[AS_HELP_STRING(
		[--with-cz80],
		[include CZ80 CPU core [default=yes]]
	)],
	[WITH_CZ80=$withval],
	[WITH_CZ80=yes]
)

AC_ARG_WITH(
	[drz80],
	[AS_HELP_STRING(
		[--with-drz80],
		[include DrZ80 CPU core [default=auto]]
	)],
	[WITH_DRZ80=$withval],
	[WITH_DRZ80=$USE_ARM_ASM]
)
AS_IF(
	[test "x$WITH_DRZ80" = xyes -a "x$USE_ARM_ASM" = xno],
	[AC_MSG_FAILURE(
		[DrZ80 can only be enabled on ARM targets with a \
working ASM support]
	)]
)

dnl Check if debugger should be enabled.
AC_ARG_ENABLE(
	[debugger],
	[AS_HELP_STRING(
		[--enable-debugger],
		[enable debugger [default=no]]
	)],
	[USE_DEBUGGER=$enableval],
	[USE_DEBUGGER=no]
)

dnl Check if dZ80 is to be included.
AC_ARG_WITH(
	[dz80],
	[AS_HELP_STRING(
		[--with-dz80],
		[include dZ80 disassembler in debugger [default=auto]],
	)],
	[WITH_DZ80=$withval],
	[WITH_DZ80=$USE_DEBUGGER]
)
AS_IF(
	[test "x$WITH_DZ80" = xyes -a "x$USE_DEBUGGER" != xyes],
	[AC_MSG_FAILURE([dZ80 is only meaningful if debugger is also enabled])]
)

dnl Check if VDP debugging should be enabled.
AC_ARG_ENABLE(
	[debug-vdp],
	[AS_HELP_STRING(
		[--enable-debug-vdp],
		[enable extra debugging for the VDP [default=auto]]
	)],
	[USE_DEBUG_VDP=$enableval],
	[USE_DEBUG_VDP=auto]
)
AS_IF(
	[test "x$USE_DEBUG_VDP" != xno -a "x$USE_DEBUG_VDP" != xyes],
	[AS_IF(
		[test "x$USE_DEBUG" = xyes -o "x$USE_DEBUGGER" = xyes],
		[USE_DEBUG_VDP=yes],
		[USE_DEBUG_VDP=no]
	)]
)

dnl Check if Sega Pico emulation should be enabled.
AC_ARG_ENABLE(
	[pico],
	[AS_HELP_STRING(
		[--enable-pico],
		[enable Sega Pico emulation (experimental) [default=no]]
	)],
	[USE_PICO=$enableval],
	[USE_PICO=no]
)

dnl Check if VGM dumping functionality should be enabled.
AC_ARG_ENABLE(
	[vgmdump],
	[AS_HELP_STRING(
		[--enable-vgmdump],
		[enable VGM dumping [default=no]]
	)],
	[USE_VGMDUMP=$enableval],
	[USE_VGMDUMP=no]
)

dnl Check for Doxygen.
AC_ARG_WITH(
	[doxygen],
	[AS_HELP_STRING(
		[--with-doxygen],
		[build Doxygen documentation [default=auto]]
	)],
	[WITH_DOXYGEN=$withval],
	[WITH_DOXYGEN=auto]
)
AS_IF(
	[test "x$WITH_DOXYGEN" != xno],
	[AC_CHECK_PROGS([DOXYGEN], [doxygen])]
	[AS_IF(
		[test "x$DOXYGEN" = x],
		[AS_IF(
			[test "x$WITH_DOXYGEN" = xyes],
			[AC_MSG_FAILURE(
				[Doxygen not found, try --without-doxygen.]
			)],
			[WITH_DOXYGEN=no]
		)],
		[WITH_DOXYGEN=yes]
	)]
)

dnl Prevent dgen_tobin from being linked with unnecessary libraries.
DGEN_LIBS="$LIBS"
LIBS="$TOBIN_LIBS"

dnl Define everything.
AS_IF([test "x$USE_OPENGL" = xyes], [AC_DEFINE([WITH_OPENGL])])
AS_IF([test "x$USE_LIBARCHIVE" = xyes], [AC_DEFINE([WITH_LIBARCHIVE])])
AS_IF([test "x$USE_DEBUGGER" = xyes], [AC_DEFINE([WITH_DEBUGGER])])
AS_IF([test "x$WITH_DZ80" = xyes], [AC_DEFINE([WITH_DZ80])])
AS_IF([test "x$USE_DEBUG" != xyes], [AC_DEFINE([NDEBUG])])
AS_IF([test "x$USE_DEBUG_VDP" = xyes], [AC_DEFINE([WITH_DEBUG_VDP])])
AS_IF([test "x$USE_PICO" = xyes], [AC_DEFINE([WITH_PICO])])
AS_IF([test "x$USE_VGMDUMP" = xyes], [AC_DEFINE([WITH_VGMDUMP])])
AS_IF([test "x$USE_JOYSTICK" = xyes], [AC_DEFINE([WITH_JOYSTICK])])
AS_IF([test "x$USE_THREADS" = xyes], [AC_DEFINE([WITH_THREADS])])
AS_IF([test "x$WITH_MUSA" = xyes], [AC_DEFINE([WITH_MUSA])])
AS_IF([test "x$WITH_STAR" = xyes], [AC_DEFINE([WITH_STAR])])
AS_IF([test "x$WITH_MZ80" = xyes], [AC_DEFINE([WITH_MZ80])])
AS_IF([test "x$WITH_CZ80" = xyes], [AC_DEFINE([WITH_CZ80])])
AS_IF([test "x$WITH_DRZ80" = xyes], [AC_DEFINE([WITH_DRZ80])])
AS_IF([test "x$WITH_CYCLONE" = xyes], [AC_DEFINE([WITH_CYCLONE])])
AS_IF([test "x$USE_ASM" = xyes], [AC_DEFINE([WITH_ASM])])
AS_IF(
	[test "x$USE_X86_ASM" = xyes],
	[AC_DEFINE([WITH_X86_ASM])]
	[AC_DEFINE([HAVE_MEMCPY_H])]
)
AS_IF([test "x$USE_CTV" = xyes], [AC_DEFINE([WITH_CTV])])
AS_IF([test "x$USE_HQX" = xyes], [AC_DEFINE([WITH_HQX])])
AS_IF([test "x$USE_SCALE2X" = xyes], [AC_DEFINE([WITH_SCALE2X])])
AS_IF([test "x$USE_X86_MZ80" = xyes], [AC_DEFINE([WITH_X86_MZ80])])
AS_IF([test "x$USE_X86_MMX" = xyes], [AC_DEFINE([WITH_X86_MMX])])
AS_IF([test "x$USE_X86_CTV" = xyes], [AC_DEFINE([WITH_X86_CTV])])
AS_IF([test "x$USE_X86_TILES" = xyes], [AC_DEFINE([WITH_X86_TILES])])

AM_CONDITIONAL([WITH_DEBUG_VDP], [test "x$USE_DEBUG_VDP" = xyes])
AM_CONDITIONAL([WITH_PICO], [test "x$USE_PICO" = xyes])
AM_CONDITIONAL([WITH_VGMDUMP], [test "x$USE_VGMDUMP" = xyes])
AM_CONDITIONAL([WITH_DEBUGGER], [test "x$USE_DEBUGGER" = xyes])
AM_CONDITIONAL([WITH_DZ80], [test "x$WITH_DZ80" = xyes])
AM_CONDITIONAL([WITH_CTV], [test "x$USE_CTV" = xyes])
AM_CONDITIONAL([WITH_HQX], [test "x$USE_HQX" = xyes])
AM_CONDITIONAL([WITH_SCALE2X], [test "x$USE_SCALE2X" = xyes])
AM_CONDITIONAL([WITH_MUSA], [test "x$WITH_MUSA" = xyes])
AM_CONDITIONAL([WITH_STAR], [test "x$WITH_STAR" = xyes])
AM_CONDITIONAL([WITH_MZ80], [test "x$WITH_MZ80" = xyes])
AM_CONDITIONAL([WITH_CZ80], [test "x$WITH_CZ80" = xyes])
AM_CONDITIONAL([WITH_DRZ80], [test "x$WITH_DRZ80" = xyes])
AM_CONDITIONAL([WITH_CYCLONE], [test "x$WITH_CYCLONE" = xyes])
AM_CONDITIONAL([WITH_X86_ASM], [test "x$USE_X86_ASM" = xyes])
AM_CONDITIONAL([WITH_X86_MZ80], [test "x$USE_X86_MZ80" = xyes])
AM_CONDITIONAL([WITH_X86_MMX], [test "x$USE_X86_MMX" = xyes])
AM_CONDITIONAL([WITH_X86_CTV], [test "x$USE_X86_CTV" = xyes])
AM_CONDITIONAL([WITH_X86_TILES], [test "x$USE_X86_TILES" = xyes])
AM_CONDITIONAL([WITH_DOXYGEN], [test "x$WITH_DOXYGEN" = xyes])
AM_COND_IF([WITH_DOXYGEN], [AC_CONFIG_FILES([doc/Doxyfile])])

AC_SUBST(CC_FOR_BUILD)
AC_SUBST(CXX_FOR_BUILD)
AC_SUBST(CPPFLAGS_FOR_BUILD)
AC_SUBST(CFLAGS_FOR_BUILD)
AC_SUBST(CXXFLAGS_FOR_BUILD)
AC_SUBST(LDFLAGS_FOR_BUILD)
AC_SUBST(OBJEXT_FOR_BUILD)
AC_SUBST(EXEEXT_FOR_BUILD)
AC_SUBST(NASM)
AC_SUBST(NASM_FORMAT)
AC_SUBST(DGEN_LIBS)

dnl The following shuts up automake when overriding *FLAGS macros.
CPPFLAGS_B=CPPFLAGS
CFLAGS_B=CFLAGS
CXXFLAGS_B=CXXFLAGS
LDFLAGS_B=LDFLAGS
AC_SUBST(CPPFLAGS_B)
AC_SUBST(CFLAGS_B)
AC_SUBST(CXXFLAGS_B)
AC_SUBST(LDFLAGS_B)

dnl Done!
AC_OUTPUT(
	[Makefile]
	[star/Makefile]
	[musa/Makefile]
	[cyclone/Makefile]
	[mz80/Makefile]
	[sdl/Makefile]
	[doc/Makefile]
)

AC_MSG_NOTICE([

Front-end
  OpenGL: $USE_OPENGL
  Joystick: $USE_JOYSTICK
  Multi-threading: $USE_THREADS
  Crap TV filters: $USE_CTV
  hqx filters: $USE_HQX
  scale2x filters: $USE_SCALE2X
  Compressed ROMs: $USE_LIBARCHIVE
  Debugger: $USE_DEBUGGER
  dZ80 disassembler: $WITH_DZ80
  Debugging: $USE_DEBUG
  VDP debugging: $USE_DEBUG_VDP
  Sega Pico: $USE_PICO
  VGM dumping: $USE_VGMDUMP

CPU cores
  Musashi M68K: $WITH_MUSA
  StarScream: $WITH_STAR
  Cyclone: $WITH_CYCLONE
  MZ80: $WITH_MZ80
  CZ80: $WITH_CZ80
  DrZ80: $WITH_DRZ80

ASM support ($USE_ASM)
  x86 ASM
    MZ80: $USE_X86_MZ80
    MMX memcpy: $USE_X86_MMX
    Crap TV filters: $USE_X86_CTV
    Tiles: $USE_X86_TILES
  ARM ASM
    Cyclone: $WITH_CYCLONE
    DrZ80: $WITH_DRZ80

Doxygen documentation: $WITH_DOXYGEN
])
