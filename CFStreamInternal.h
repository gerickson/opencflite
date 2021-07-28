/*
 * Copyright (c) 2008-2012 Brent Fulgham <bfulgham@gmail.org>.  All rights reserved.
 *
 * This source code is a modified version of the CoreFoundation sources released by Apple Inc. under
 * the terms of the APSL version 2.0 (see below).
 *
 * For information about changes from the original Apple source release can be found by reviewing the
 * source control system for the project at https://sourceforge.net/svn/?group_id=246198.
 *
 * The original license information is as follows:
 * 
 * Copyright (c) 2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#if !defined(__COREFOUNDATION_CFSTREAMINTERNAL__)
#define __COREFOUNDATION_CFSTREAMINTERNAL__ 1

#include <CoreFoundation/CFStreamAbstract.h>
#include <CoreFoundation/CFStreamPriv.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation_Prefix.h>

CF_EXTERN_C_BEGIN


// Older versions of the callbacks; v0 callbacks match v1 callbacks, except that create, finalize, and copyDescription are missing.
typedef Boolean (*_CFStreamCBOpenV1)(struct _CFStream *stream, CFStreamError *error, Boolean *openComplete, void *info);
typedef Boolean (*_CFStreamCBOpenCompletedV1)(struct _CFStream *stream, CFStreamError *error, void *info);
typedef CFIndex (*_CFStreamCBReadV1)(CFReadStreamRef stream, UInt8 *buffer, CFIndex bufferLength, CFStreamError *error, Boolean *atEOF, void *info);
typedef const UInt8 *(*_CFStreamCBGetBufferV1)(CFReadStreamRef sream, CFIndex maxBytesToRead, CFIndex *numBytesRead, CFStreamError *error, Boolean *atEOF, void *info);
typedef Boolean (*_CFStreamCBCanReadV1)(CFReadStreamRef, void *info);
typedef CFIndex (*_CFStreamCBWriteV1)(CFWriteStreamRef, const UInt8 *buffer, CFIndex bufferLength, CFStreamError *error, void *info);
typedef Boolean (*_CFStreamCBCanWriteV1)(CFWriteStreamRef, void *info);

struct _CFStreamCallBacksV1 {
    CFIndex version;
    void *(*create)(struct _CFStream *stream, void *info);
    void (*finalize)(struct _CFStream *stream, void *info);
    CFStringRef (*copyDescription)(struct _CFStream *stream, void *info);

    _CFStreamCBOpenV1 open;
    _CFStreamCBOpenCompletedV1 openCompleted;
    _CFStreamCBReadV1 read;
    _CFStreamCBGetBufferV1 getBuffer;
    _CFStreamCBCanReadV1 canRead;
    _CFStreamCBWriteV1 write;
    _CFStreamCBCanWriteV1 canWrite;
    void (*close)(struct _CFStream *stream, void *info);

    CFTypeRef (*copyProperty)(struct _CFStream *stream, CFStringRef propertyName, void *info);
    Boolean (*setProperty)(struct _CFStream *stream, CFStringRef propertyName, CFTypeRef propertyValue, void *info);
    void (*requestEvents)(struct _CFStream *stream, CFOptionFlags events, void *info);
    void (*schedule)(struct _CFStream *stream, CFRunLoopRef runLoop, CFStringRef runLoopMode, void *info);
    void (*unschedule)(struct _CFStream *stream, CFRunLoopRef runLoop, CFStringRef runLoopMode, void *info);
};

// These two are defined in CFSocketStream.c because that's where the glue for CFNetwork is.
__private_extern__ CFErrorRef _CFErrorFromStreamError(CFAllocatorRef alloc, CFStreamError *err);
__private_extern__ CFStreamError _CFStreamErrorFromError(CFErrorRef error);

CF_EXTERN_C_END

#endif /* ! __COREFOUNDATION_CFSTREAMINTERNAL__ */


