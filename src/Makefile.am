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

lib_LTLIBRARIES = libCAEN_FELib.la
libCAEN_FELib_la_SOURCES = \
	CAEN_FELib.c \
	definitions.h
libCAEN_FELib_la_CPPFLAGS = \
	-I$(top_srcdir)/include
libCAEN_FELib_la_CFLAGS = 
libCAEN_FELib_la_LIBADD = \
	$(LIBADD_DLOPEN)
libCAEN_FELib_la_LDFLAGS = \
	$(BUG67791_LDFLAGS) \
	-version-info 0:0:0
