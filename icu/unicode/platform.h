/*
******************************************************************************
*
*   Copyright (C) 1997-2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
*  FILE NAME : platform.h
*
*   Date        Name        Description
*   05/13/98    nos         Creation (content moved here from ptypes.h).
*   03/02/99    stephen     Added AS400 support.
*   03/30/99    stephen     Added Linux support.
*   04/13/99    stephen     Reworked for autoconf.
******************************************************************************
*/

/**
 * \file 
 * \brief Basic types for the platform 
 */

/* Define the platform we're on. */
#ifdef DEPLOYMENT_TARGET_MACOSX
#ifndef U_DARWIN
#define U_DARWIN
#endif
#elif DEPLOYMENT_TARGET_WIN32
#ifndef U_CYGWIN
#define U_CYGWIN
#endif
#endif

/* Define whether inttypes.h is available */
#ifndef U_HAVE_INTTYPES_H
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_INTTYPES_H 1
#else
#define U_HAVE_INTTYPES_H 0
#endif
#endif

/*
 * Define what support for C++ streams is available.
 *     If U_IOSTREAM_SOURCE is set to 199711, then <iostream> is available
 * (1997711 is the date the ISO/IEC C++ FDIS was published), and then
 * one should qualify streams using the std namespace in ICU header
 * files.
 *     If U_IOSTREAM_SOURCE is set to 198506, then <iostream.h> is
 * available instead (198506 is the date when Stroustrup published
 * "An Extensible I/O Facility for C++" at the summer USENIX conference).
 *     If U_IOSTREAM_SOURCE is 0, then C++ streams are not available and
 * support for them will be silently suppressed in ICU.
 *
 */

#ifndef U_IOSTREAM_SOURCE
#define U_IOSTREAM_SOURCE 199711
#endif

/* Determines whether specific types are available */
#ifndef U_HAVE_INT8_T
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_INT8_T 1
#else
#define U_HAVE_INT8_T 0
#endif
#endif

#ifndef U_HAVE_UINT8_T
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_UINT8_T 1
#else
#define U_HAVE_UINT8_T 0
#endif
#endif

#ifndef U_HAVE_INT16_T
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_INT16_T 1
#else
#define U_HAVE_INT16_T 0
#endif
#endif

#ifndef U_HAVE_UINT16_T
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_UINT16_T 1
#else
#define U_HAVE_UINT16_T 0
#endif
#endif

#ifndef U_HAVE_INT32_T
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_INT32_T 1
#else
#define U_HAVE_INT32_T 0
#endif
#endif

#ifndef U_HAVE_UINT32_T
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_UINT32_T 1
#else
#define U_HAVE_UINT32_T 0
#endif
#endif

#ifndef U_HAVE_INT64_T
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_INT64_T 1
#else
#define U_HAVE_INT64_T 0
#endif
#endif

#ifndef U_HAVE_UINT64_T
#ifdef DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_UINT64_T 1
#else
#define U_HAVE_UINT64_T 0
#endif
#endif

/*===========================================================================*/
/* Generic data types                                                        */
/*===========================================================================*/

#include <sys/types.h>

/* If your platform does not have the <inttypes.h> header, you may
   need to edit the typedefs below. */
#if U_HAVE_INTTYPES_H

/* autoconf 2.13 sometimes can't properly find the data types in <inttypes.h> */
/* os/390 needs <inttypes.h>, but it doesn't have int8_t, and it sometimes */
/* doesn't have uint8_t depending on the OS version. */
/* So we have this work around. */
#ifdef OS390
/* The features header is needed to get (u)int64_t sometimes. */
#include <features.h>
#if ! U_HAVE_INT8_T
typedef signed char int8_t;
#endif
#if !defined(__uint8_t)
#define __uint8_t 1
typedef unsigned char uint8_t;
#endif
#endif /* OS390 */

#include <inttypes.h>

#else /* U_HAVE_INTTYPES_H */

#if ! U_HAVE_INT8_T
typedef signed char int8_t;
#endif

#if ! U_HAVE_UINT8_T
typedef unsigned char uint8_t;
#endif

#if ! U_HAVE_INT16_T
typedef signed short int16_t;
#endif

#if ! U_HAVE_UINT16_T
typedef unsigned short uint16_t;
#endif

#if ! U_HAVE_INT32_T
typedef signed int int32_t;
#endif

#if ! U_HAVE_UINT32_T
typedef unsigned int uint32_t;
#endif

#if ! U_HAVE_INT64_T
    typedef signed long long int64_t;
/* else we may not have a 64-bit type */
#endif

#if ! U_HAVE_UINT64_T
    typedef unsigned long long uint64_t;
/* else we may not have a 64-bit type */
#endif

#endif

/*===========================================================================*/
/* Compiler and environment features                                         */
/*===========================================================================*/

/* Define whether namespace is supported */
#ifndef U_HAVE_NAMESPACE
#define U_HAVE_NAMESPACE 1
#endif

/* Determines the endianness of the platform
   It's done this way in case multiple architectures are being built at once.
   For example, Darwin supports fat binaries, which can be both PPC and x86 based. */
#if defined(BYTE_ORDER) && defined(BIG_ENDIAN)
#define U_IS_BIG_ENDIAN (BYTE_ORDER == BIG_ENDIAN)
#else
#define U_IS_BIG_ENDIAN 0
#endif

/* 1 or 0 to enable or disable threads.  If undefined, default is: enable threads. */
#define ICU_USE_THREADS 1

/* On strong memory model CPUs (e.g. x86 CPUs), we use a safe & quick double check lock. */
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#define UMTX_STRONG_MEMORY_MODEL 1
#endif

#ifndef U_DEBUG
#define U_DEBUG 0
#endif

#ifndef U_RELEASE
#define U_RELEASE 1
#endif

/* Determine whether to disable renaming or not. This overrides the
   setting in umachine.h which is for all platforms. */
#ifndef U_DISABLE_RENAMING
#if DEPLOYMENT_TARGET_MACOSX
#define U_DISABLE_RENAMING 1
#else
#define U_DISABLE_RENAMING 0
#endif
#endif

/* Determine whether to override new and delete. */
#ifndef U_OVERRIDE_CXX_ALLOCATION
#if DEPLOYMENT_TARGET_MACOSX
#define U_OVERRIDE_CXX_ALLOCATION 1
#else
#define U_OVERRIDE_CXX_ALLOCATION 0
#endif
#endif
/* Determine whether to override placement new and delete for STL. */
#ifndef U_HAVE_PLACEMENT_NEW
#if DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_PLACEMENT_NEW 1
#else
#define U_HAVE_PLACEMENT_NEW 0
#endif
#endif

/* Determine whether to enable tracing. */
#ifndef U_ENABLE_TRACING
#define U_ENABLE_TRACING 1
#endif

/* Do we allow ICU users to use the draft APIs by default? */
#ifndef U_DEFAULT_SHOW_DRAFT
#define U_DEFAULT_SHOW_DRAFT 1
#endif

/* Define the library suffix in a C syntax. */
#define U_HAVE_LIB_SUFFIX 0
#define U_LIB_SUFFIX_C_NAME 
#define U_LIB_SUFFIX_C_NAME_STRING ""

/*===========================================================================*/
/* Character data types                                                      */
/*===========================================================================*/

#if ((defined(OS390) && (!defined(__CHARSET_LIB) || !__CHARSET_LIB))) || defined(OS400)
#   define U_CHARSET_FAMILY 1
#endif

/*===========================================================================*/
/* Information about wchar support                                           */
/*===========================================================================*/

#define U_HAVE_WCHAR_H      1
#if DEPLOYMENT_TARGET_MACOSX
#define U_SIZEOF_WCHAR_T    4

#define U_HAVE_WCSCPY       1
#else
#define U_SIZEOF_WCHAR_T    2

#define U_HAVE_WCSCPY       0

/**
 * \def U_DECLARE_UTF16
 * Do not use this macro. Use the UNICODE_STRING or U_STRING_DECL macros
 * instead.
 * @internal
 */
#if 1 || defined(U_CHECK_UTF16_STRING)
#if (defined(__xlC__) && defined(__IBM_UTF_LITERAL) && U_SIZEOF_WCHAR_T != 2) \
    || (defined(__HP_aCC) && __HP_aCC >= 035000) \
    || (defined(__HP_cc) && __HP_cc >= 111106)
#define U_DECLARE_UTF16(string) u ## string
#elif (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x550)
/* || (defined(__SUNPRO_C) && __SUNPRO_C >= 0x580) */
/* Sun's C compiler has issues with this notation, and it's unreliable. */
#define U_DECLARE_UTF16(string) U ## string
#elif U_SIZEOF_WCHAR_T == 2 \
    && (U_CHARSET_FAMILY == 0 || ((defined(OS390) || defined(OS400)) && defined(__UCS2__)))
#define U_DECLARE_UTF16(string) L ## string
#endif
#endif

#endif

/*===========================================================================*/
/* Information about POSIX support                                           */
/*===========================================================================*/

#if DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_NL_LANGINFO          1
#define U_HAVE_NL_LANGINFO_CODESET  1
#define U_NL_LANGINFO_CODESET       CODESET
#else
#define U_HAVE_NL_LANGINFO          0
#define U_HAVE_NL_LANGINFO_CODESET  0
#define U_NL_LANGINFO_CODESET       -1
#endif

#if 1
#define U_TZSET         tzset
#endif
#if 1
#define U_TIMEZONE      timezone
#endif
#if DEPLOYMENT_TARGET_MACOSX
#define U_TZNAME        tzname
#endif

#if DEPLOYMENT_TARGET_MACOSX
#define U_HAVE_MMAP     1
#define U_HAVE_POPEN    1
#else
#define U_HAVE_MMAP     0
#define U_HAVE_POPEN    0
#endif

/*===========================================================================*/
/* Symbol import-export control                                              */
/*===========================================================================*/

#if defined(U_DARWIN) && defined(__GNUC__) && (__GNUC__ >= 4)
#define USE_GCC_VISIBILITY_ATTRIBUTE 1
#endif

#ifdef USE_GCC_VISIBILITY_ATTRIBUTE
#define U_EXPORT __attribute__((visibility("default")))
#elif (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x550) \
   || (defined(__SUNPRO_C) && __SUNPRO_C >= 0x550) 
#define U_EXPORT __global
/*#elif defined(__HP_aCC) || defined(__HP_cc)
#define U_EXPORT __declspec(dllexport)*/
#else
#define U_EXPORT
#endif

/* U_CALLCONV is releated to U_EXPORT2 */
#define U_EXPORT2

/* cygwin needs to export/import data */
#ifdef U_CYGWIN
#define U_IMPORT __declspec(dllimport)
#else
#define U_IMPORT 
#endif

/*===========================================================================*/
/* Code alignment and C function inlining                                    */
/*===========================================================================*/

#ifndef U_INLINE
#   ifdef __cplusplus
#       define U_INLINE inline
#   else
#       define U_INLINE __inline
#   endif
#endif

#define U_ALIGN_CODE(n) 

/*===========================================================================*/
/* Programs used by ICU code                                                 */
/*===========================================================================*/

#define U_MAKE  "make"
