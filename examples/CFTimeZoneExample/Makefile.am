#
#    Copyright (c) 2021 OpenCFLite Authors. All Rights Reserved.
#
#    This file contains Original Code and/or Modifications of Original Code
#    as defined in and that are subject to the Apple Public Source License
#    Version 2.0 (the 'License'). You may not use this file except in
#    compliance with the License. Please obtain a copy of the License at
#    http://www.opensource.apple.com/apsl/ and read it before using this
#    file.
#
#    The Original Code and all software distributed under the License are
#    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
#    EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
#    INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
#    FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
#    Please see the License for the specific language governing rights and
#    limitations under the License.
#

#
#    Description:
#      This file is the GNU autoconf input source file for
#      a CFTimeZone example.
#

AM_CFLAGS			= -I${top_srcdir}/include

if CF_BUILD_TESTS
check_PROGRAMS			= CFTimeZoneExample
endif

CFTimeZoneExample_LDADD	= ${top_builddir}/libCoreFoundation.la

CFTimeZoneExample_SOURCES	= CFTimeZoneExample.cpp

if CF_BUILD_TESTS
check:
	${LIBTOOL} --mode execute ./CFTimeZoneExample

ddd gdb lldb:
	${LIBTOOL} --mode execute ${@} ./CFTimeZoneExample

valgrind:
	${LIBTOOL} --mode execute ${@} ${VALGRINDFLAGS} ./CFTimeZoneExample
endif
