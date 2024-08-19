#	Copyright (C) 2020-2023 CAEN SpA
#
#	This file is part of the CAEN FE Library.
#
#	The CAEN FE Library is free software; you can redistribute it and/or
#	modify it under the terms of the GNU Lesser General Public
#	License as published by the Free Software Foundation; either
#	version 3 of the License, or (at your option) any later version.
#
#	The CAEN FE Library is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#	Lesser General Public License for more details.
#
#	You should have received a copy of the GNU Lesser General Public
#	License along with the CAEN FE Library; if not, see
#	https://www.gnu.org/licenses/.
#
#	SPDX-License-Identifier: LGPL-3.0-or-later

# Tested with autoconf 2.69, released in 2012
AC_PREREQ([2.69])
AC_INIT(CAEN_FELib, [m4_esyscmd_s([echo $VERSION])], support.computing@caen.it)

# Search in m4 folder for custom macros. It requires also ACLOCAL_AMFLAGS on
# Makefile.am, unless we use AC_CONFIG_MACRO_DIRS introduced on autoconf 2.70.
AC_CONFIG_MACRO_DIR([m4])

# Get target_os
AC_CANONICAL_TARGET

# tar-ustar required to remove the filename length limit on default tar-v7 format
# is old enough to be supported everywhere
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects tar-ustar])
AM_PROG_AR

# Requires constructor to be invoked at startup, cannot be used as static
LT_INIT([disable-static])

# Set langage to C
# Note:
#   -std=gnu99 is set only if default compiler does not support it by default.
#   For example, if -std=gnu17 is the compiler default, it should also support
#   C99 features and then no flag is appended.
AC_PROG_CC
AC_PROG_CC_C99

# Checks for other programs
AC_PROG_INSTALL

# Checks for compiler support
AM_CONDITIONAL([NO_C99_SUPPORT], [test x"$ac_cv_prog_cc_c99" = x"no"])
AM_COND_IF([NO_C99_SUPPORT], AC_MSG_FAILURE([C99 support required.]))

# If available, use -fvisibility=hidden to hide internal names
AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden])

# If available, use -zdefs to improve linker consistency
AX_APPEND_LINK_FLAGS([-Wl,-z,defs])

# Detect if the target system use libstdc++ by default
case "${target_os}" in
	linux*)				default_use_libstdcxx="yes";;
	cygwin*|mingw*)		default_use_libstdcxx="yes";; # To be tested
	darwin*)			default_use_libstdcxx="no";;
	*)					AC_MSG_ERROR(unsupported target host $target_os);;
esac

AS_IF(
[test "x$default_use_libstdcxx" = x"yes"], [
	# Workaround for libstdc++ bug:
	#   GCC bug 67791 occours when an application that does not require threads invokes CAEN_FELib_Open
	#   with an implementation library that requires thread support (like Dig2).
	#   Adding "-lpthread" is not sufficient, as linker does not add a library to dynamic section if
	#   that library is not actually required. We need to force it using "--no-as-needed" linker option.
	#   "-Wl,--push-state,--no-as-needed,-lpthread,--pop-state" is a patch to mitigate this bug:
	#   it forces linker to add a forced dependency to pthread on its dynamic section.
	# Details:
	# - https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791
	# Note
	# - "--push-state" and "--pop-state" require GNU ld >= 2.25 (released in 2014); as a workaround for older
	#   version use "-Wl,--no-as-needed,-lpthread,--as-needed" to restore default state, assuming GCC pass
	#   "--as-needed" as default linker flag.
	# - "--no-as-needed" and "--as-needed" require GNU ld >= 2.15 (released in 2004), should be always fine.
	# Alternatives:
	# - add `| RTLD_GLOBAL` to dlopen argument (slower, see https://stackoverflow.com/a/51253734/3287591)
	AX_CHECK_LINK_FLAG(
		[-Wl,--push-state,--pop-state],
		[BUG67791_LDFLAGS="-Wl,--push-state,--no-as-needed,-lpthread,--pop-state"],
		[BUG67791_LDFLAGS="-Wl,--no-as-needed,-lpthread,--as-needed"]
	)
],
[test "x$default_use_libstdcxx" = x"no"], [
	BUG67791_LDFLAGS=""
])
AC_SUBST([BUG67791_LDFLAGS])

# Add support for --disable-assert
AC_HEADER_ASSERT

# Check for __thread
AC_COMPILE_IFELSE([AC_LANG_SOURCE([__thread int foo;])], [], [AC_MSG_ERROR(__thread support required.)])

# Check for dlopen and define LIBADD_DLOPEN (usually to -ldl)
LT_LIB_DLLOAD

AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT
