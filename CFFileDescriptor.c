/*
 *   Copyright (c) 2021 OpenCFLite Authors. All Rights Reserved.
 *
 *   This file contains Original Code and/or Modifications of Original Code
 *   as defined in and that are subject to the Apple Public Source License
 *   Version 2.0 (the 'License'). You may not use this file except in
 *   compliance with the License. Please obtain a copy of the License at
 *   http://www.opensource.apple.com/apsl/ and read it before using this
 *   file.
 *
 *   The Original Code and all software distributed under the License are
 *   distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *   EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *   INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *   Please see the License for the specific language governing rights and
 *   limitations under the License.
 *
 */

/**
 *   @file
 *     This file implements the CFFileDescriptor CoreFoundation object
 *     class.
 *
 */

#include <inttypes.h>

#include <AssertMacros.h>

#include <CoreFoundation/CFFileDescriptor.h>
#include "CFInternal.h"

#if DEPLOYMENT_TARGET_MACOS || DEPLOYMENT_TARGET_LINUX
#include <dlfcn.h>
#endif

/* Preprocessor Definitions */

#define LOG_CFFILEDESCRIPTOR 1

#define __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR (CFFileDescriptorNativeDescriptor)(-1)

// In the CFRuntimeBase info reserved bits:
//
//   Bit    6 is used for write-signaled state (mutable)
//   Bit    5 is used for read-signaled state (mutable)
//   Bit    4 is used for valid state (mutable)
//   Bit    3 is used for close-on-invalidate state (mutable)
//   Bits 0-2 are used for callback types (immutable)

#define __kCFInfoCFFileDescriptorWriteSignaledFirstBit             6
#define __kCFInfoCFFileDescriptorWriteSignaledLastBit              6
#define __kCFInfoCFFileDescriptorReadSignaledFirstBit              5
#define __kCFInfoCFFileDescriptorReadSignaledLastBit               5
#define __kCFInfoCFFileDescriptorValidFirstBit                     4
#define __kCFInfoCFFileDescriptorValidLastBit                      4
#define __kCFInfoCFFileDescriptorCloseOnInvalidateFirstBit         3
#define __kCFInfoCFFileDescriptorCloseOnInvalidateLastBit          3
#define __kCFInfoCFFileDescriptorCallBackTypesFirstBit             3
#define __kCFInfoCFFileDescriptorCallBackTypesLastBit              3

/* Type Declarations */

struct __CFFileDescriptor {
    CFRuntimeBase                    _base;
	CFSpinLock_t                     _lock;
	CFFileDescriptorNativeDescriptor _descriptor;
	CFFileDescriptorCallBack         _callout;
	CFFileDescriptorContext          _context;
    CFRunLoopSourceRef               _source;
};

/* Function Prototypes */

// CFRuntimeClass Functions

static void        __CFFileDescriptorDeallocate(CFTypeRef cf);
static CFStringRef __CFFileDescriptorCopyDescription(CFTypeRef cf);

// Other Functions

static void __CFFileDescriptorInvalidate_Retained(CFFileDescriptorRef f);

/* Global Variables */

static CFTypeID __kCFFileDescriptorTypeID = _kCFRuntimeNotATypeID;

static const CFRuntimeClass __CFFileDescriptorClass = {
    0,
    "CFFileDescriptor",
    NULL,                             // init
    NULL,                             // copy
    __CFFileDescriptorDeallocate,
    NULL,                             // equal
    NULL,                             // hash
    NULL,                             // copyFormattingDescription
    __CFFileDescriptorCopyDescription
};

CF_INLINE Boolean __CFFileDescriptorIsReadSignaled(CFFileDescriptorRef f) {
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
										 __kCFInfoCFFileDescriptorReadSignaledLastBit,
										 __kCFInfoCFFileDescriptorReadSignaledFirstBit);
}

CF_INLINE void __CFFileDescriptorSetReadSignaled(CFFileDescriptorRef f) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
						 __kCFInfoCFFileDescriptorReadSignaledLastBit,
						 __kCFInfoCFFileDescriptorReadSignaledFirstBit,
						 1);
}

CF_INLINE void __CFFileDescriptorClearReadSignaled(CFFileDescriptorRef f) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
						 __kCFInfoCFFileDescriptorReadSignaledLastBit,
						 __kCFInfoCFFileDescriptorReadSignaledFirstBit,
						 0);
}

CF_INLINE Boolean __CFFileDescriptorIsWriteSignaled(CFFileDescriptorRef f) {
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
										 __kCFInfoCFFileDescriptorWriteSignaledLastBit,
										 __kCFInfoCFFileDescriptorWriteSignaledFirstBit);
}

CF_INLINE void __CFFileDescriptorSetWriteSignaled(CFFileDescriptorRef f) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
						 __kCFInfoCFFileDescriptorWriteSignaledLastBit,
						 __kCFInfoCFFileDescriptorWriteSignaledFirstBit,
						 1);
}

CF_INLINE void __CFFileDescriptorClearWriteSignaled(CFFileDescriptorRef f) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
						 __kCFInfoCFFileDescriptorWriteSignaledLastBit,
						 __kCFInfoCFFileDescriptorWriteSignaledFirstBit,
						 0);
}

CF_INLINE Boolean __CFFileDescriptorIsValid(CFFileDescriptorRef f) {
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
										 __kCFInfoCFFileDescriptorValidLastBit,
										 __kCFInfoCFFileDescriptorValidFirstBit);
}

CF_INLINE void __CFFileDescriptorSetValid(CFFileDescriptorRef f) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
						 __kCFInfoCFFileDescriptorValidLastBit,
						 __kCFInfoCFFileDescriptorValidFirstBit,
						 1);
}

CF_INLINE void __CFFileDescriptorClearValid(CFFileDescriptorRef f) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
						 __kCFInfoCFFileDescriptorValidLastBit,
						 __kCFInfoCFFileDescriptorValidFirstBit,
						 0);
}

CF_INLINE Boolean __CFFileDescriptorShouldCloseOnInvalidate(CFFileDescriptorRef f) {
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
										 __kCFInfoCFFileDescriptorCloseOnInvalidateLastBit,
										 __kCFInfoCFFileDescriptorCloseOnInvalidateFirstBit);
}

CF_INLINE void __CFFileDescriptorSetCloseOnInvalidate(CFFileDescriptorRef f) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
						 __kCFInfoCFFileDescriptorCloseOnInvalidateLastBit,
						 __kCFInfoCFFileDescriptorCloseOnInvalidateFirstBit,
						 1);
}

CF_INLINE void __CFFileDescriptorClearCloseOnInvalidate(CFFileDescriptorRef f) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
						 __kCFInfoCFFileDescriptorCloseOnInvalidateLastBit,
						 __kCFInfoCFFileDescriptorCloseOnInvalidateFirstBit,
						 0);
}

CF_INLINE void __CFFileDescriptorLock(CFFileDescriptorRef f) {
    __CFSpinLock(&(f->_lock));
}

CF_INLINE void __CFFileDescriptorUnlock(CFFileDescriptorRef f) {
    __CFSpinUnlock(&(f->_lock));
}

// MARK: Other Functions

/* static */ void
__CFFileDescriptorInvalidate_Retained(CFFileDescriptorRef f) {
    __CFFileDescriptorLock(f);

    if (__CFFileDescriptorIsValid(f)) {
        void *contextInfo = NULL;
        void (*contextRelease)(const void *info) = NULL;

        __CFFileDescriptorClearValid(f);
        __CFFileDescriptorClearWriteSignaled(f);
        __CFFileDescriptorClearReadSignaled(f);

        if (__CFFileDescriptorShouldCloseOnInvalidate(f)) {
			close(f->_descriptor);
		}

        f->_descriptor = __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR;

		// Save the context information and release function pointer
		// before we zero them out such that we can invoke the release
		// function on the context after we zero them out and unlock
		// the object since neither has anything to do with the object
		// itself.

        contextInfo = f->_context.info;
        contextRelease = f->_context.release;

        f->_context.info = NULL;
        f->_context.retain = NULL;
        f->_context.release = NULL;
        f->_context.copyDescription = NULL;

        __CFFileDescriptorUnlock(f);

		// OK, we are done with the object. Release the context, if
		// a function has been provided to do so.

        if (NULL != contextRelease) {
            contextRelease(contextInfo);
        }
    } else {
        __CFFileDescriptorUnlock(f);
    }
}

// MARK: CFRuntimeClass Functions

__private_extern__ void __CFFileDescriptorInitialize(void) {
    __kCFFileDescriptorTypeID = _CFRuntimeRegisterClass(&__CFFileDescriptorClass);
}

/* static */ void
__CFFileDescriptorDeallocate(CFTypeRef cf) {
    __CFGenericValidateType(cf, CFFileDescriptorGetTypeID());
}

/* static */ Boolean
__CFFileDescriptorEqual(CFTypeRef left, CFTypeRef right) {
	Boolean result = FALSE;

    __CFGenericValidateType(left, CFFileDescriptorGetTypeID());
    __CFGenericValidateType(right, CFFileDescriptorGetTypeID());

	return result;
}

/* static */ CFHashCode
__CFFileDescriptorHash(CFTypeRef cf) {
	CFHashCode result = 0;

    __CFGenericValidateType(cf, CFFileDescriptorGetTypeID());

	return result;
}

/* static */ CFStringRef
__CFFileDescriptorCopyDescription(CFTypeRef cf) {
	static const char * const kUnknownName = "???";
    CFFileDescriptorRef f = (CFFileDescriptorRef)cf;
	void *              addr;
	const char *        name;
    void *              contextInfo = NULL;
    CFStringRef (*contextCopyDescription)(const void *info) = NULL;
    CFStringRef         contextDesc = NULL;
    CFMutableStringRef  result      = NULL;

    __CFGenericValidateType(cf, CFFileDescriptorGetTypeID());

    result = CFStringCreateMutable(CFGetAllocator(f), 0);
	__Require(result, done);

    __CFFileDescriptorLock(f);

    addr = (void*)f->_callout;
#if DEPLOYMENT_TARGET_WINDOWS
    // FIXME:  Get name using win32 analog of dladdr?
    name = kUnknownName;
#else
    Dl_info info;
    name = (dladdr(addr, &info) && info.dli_saddr == addr && info.dli_sname) ? info.dli_sname : kUnknownName;
#endif

    CFStringAppendFormat(result,
						 NULL,
						 CFSTR("<CFFileDescriptor %p [%p]>{valid = %s, fd = %d source = %p, callout = %s (%p) context = "),
						 cf,
						 CFGetAllocator(f),
						 (__CFFileDescriptorIsValid(f) ? "Yes" : "No"),
						 f->_descriptor,
						 f->_source,
						 name,
						 addr);

    contextInfo = f->_context.info;
    contextCopyDescription = f->_context.copyDescription;

    __CFFileDescriptorUnlock(f);

    if (NULL != contextInfo && NULL != contextCopyDescription) {
        contextDesc = (CFStringRef)contextCopyDescription(contextInfo);
    }
    if (NULL == contextDesc) {
        contextDesc = CFStringCreateWithFormat(CFGetAllocator(f), NULL, CFSTR("<CFFileDescriptor context %p>"), contextInfo);
    }

    CFStringAppend(result, contextDesc);
    CFStringAppend(result, CFSTR("}"));

    CFRelease(contextDesc);

 done:
	return result;
}

// MARK: CFFileDescriptor Public API Functions

CFTypeID
CFFileDescriptorGetTypeID(void) {
	return __kCFFileDescriptorTypeID;
}

CFFileDescriptorRef
CFFileDescriptorCreate(CFAllocatorRef allocator, CFFileDescriptorNativeDescriptor fd, Boolean closeOnInvalidate, CFFileDescriptorCallBack callout, const CFFileDescriptorContext *context) {
	CFFileDescriptorRef result = NULL;

	return result;
}

CFFileDescriptorNativeDescriptor
CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f) {
    CHECK_FOR_FORK();
	int result;

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    result = f->_descriptor;

	return result;
}

void
CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context) {
    CHECK_FOR_FORK();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    CFAssert1(context->version == 0, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);

    *context = f->_context;
}

void
CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes) {
    CHECK_FOR_FORK();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

	__Require(callBackTypes != 0, done);

 done:
	return;
}

void
CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes) {
    CHECK_FOR_FORK();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

}

void
CFFileDescriptorInvalidate(CFFileDescriptorRef f) {
    CHECK_FOR_FORK();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

#if defined(LOG_CFFILEDESCRIPTOR)
    fprintf(stdout, "invalidating file descriptor %d\n", f->_descriptor);
#endif

    CFRetain(f);

	__CFFileDescriptorInvalidate_Retained(f);

    CFRelease(f);
}

Boolean
CFFileDescriptorIsValid(CFFileDescriptorRef f) {
    CHECK_FOR_FORK();
	Boolean result;

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    result = __CFFileDescriptorIsValid(f);

	return result;
}

CFRunLoopSourceRef
CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator, CFFileDescriptorRef f, CFIndex order) {
    CHECK_FOR_FORK();
    CFRunLoopSourceRef result = NULL;

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    __CFFileDescriptorLock(f);

    __CFFileDescriptorUnlock(f);

	return result;
}
