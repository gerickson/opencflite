/*
 *   Copyright (c) 2021 OpenCFLite Authors. All Rights Reserved.
 *
 *   This source code is a modified version of the CoreFoundation
 *   sources released by Apple Inc. under the terms of the APSL
 *   version 2.0 (see below).
 *
 *   For information about changes from the original Apple source
 *   release can be found by reviewing the source control system for the
 *   project at http://github.com/gerickson/opencflite/.
 *
 *   The original license information is as follows:
 *
 * Copyright (c) 2006-2019, Apple Inc. All rights reserved.
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

/*	CFFileDescriptor.h
	Copyright (c) 2006-2019, Apple Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFFILEDESCRIPTOR__)
#define __COREFOUNDATION_CFFILEDESCRIPTOR__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFRunLoop.h>

CF_EXTERN_C_BEGIN

typedef int CFFileDescriptorNativeDescriptor;

typedef struct __CFFileDescriptor * CFFileDescriptorRef;

/* Callback Reason Types */
enum {
    kCFFileDescriptorReadCallBack = 1UL << 0,
    kCFFileDescriptorWriteCallBack = 1UL << 1
};

typedef void (*CFFileDescriptorCallBack)(CFFileDescriptorRef f, CFOptionFlags callBackTypes, void *info);

typedef struct {
    CFIndex	version;
    void *	info;
    void *	(*retain)(void *info);
    void	(*release)(void *info);
    CFStringRef	(*copyDescription)(void *info);
} CFFileDescriptorContext;

CF_EXPORT CFTypeID	CFFileDescriptorGetTypeID(void) CF_AVAILABLE(10_5, 2_0);

CF_EXPORT CFFileDescriptorRef	CFFileDescriptorCreate(CFAllocatorRef allocator, CFFileDescriptorNativeDescriptor fd, Boolean closeOnInvalidate, CFFileDescriptorCallBack callout, const CFFileDescriptorContext *context) CF_AVAILABLE(10_5, 2_0);

CF_EXPORT CFFileDescriptorNativeDescriptor	CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f) CF_AVAILABLE(10_5, 2_0);

CF_EXPORT void	 CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context) CF_AVAILABLE(10_5, 2_0);

CF_EXPORT void	 CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes) CF_AVAILABLE(10_5, 2_0);

CF_EXPORT void	 CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes) CF_AVAILABLE(10_5, 2_0);

CF_EXPORT void	 CFFileDescriptorInvalidate(CFFileDescriptorRef f) CF_AVAILABLE(10_5, 2_0);
CF_EXPORT Boolean	CFFileDescriptorIsValid(CFFileDescriptorRef f) CF_AVAILABLE(10_5, 2_0);

CF_EXPORT CFRunLoopSourceRef	CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator, CFFileDescriptorRef f, CFIndex order) CF_AVAILABLE(10_5, 2_0);


CF_EXTERN_C_END

#endif /* ! __COREFOUNDATION_CFFILEDESCRIPTOR__ */

