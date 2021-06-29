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

#include <fcntl.h>
#include <inttypes.h>

#include <AssertMacros.h>

#include <CoreFoundation/CFFileDescriptor.h>
#include "CFInternal.h"

#if DEPLOYMENT_TARGET_MACOS || DEPLOYMENT_TARGET_LINUX
#include <dlfcn.h>
#include <sys/param.h>
#endif

/* Preprocessor Definitions */

#define LOG_CFFILEDESCRIPTOR 1

#if LOG_CFFILEDESCRIPTOR
#define __CFFileDescriptorMaybeLog(format, ...)  do { fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr); } while (0)
#else
#define __CFFileDescriptorMaybeLog(format, ...)
#endif

#define __CFFileDescriptorMaybeTrace(dir, name)  __CFFileDescriptorMaybeLog(dir " %s\n", name);
#define __CFFileDescriptorEnter()                __CFFileDescriptorMaybeTrace("-->", __func__);
#define __CFFileDescriptorExit()                 __CFFileDescriptorMaybeTrace("<--", __func__);

#define __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR    (CFFileDescriptorNativeDescriptor)(-1)

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

/*
 * Locks are to be acquired in the following order:
 *
 * 1. Manager:    mAllFileDescriptorsLock
 * 2. Descriptor: _lock (via __CFFileDescriptorLock)
 * 3. Manager:    mActiveFileDescriptorsLock
 *
 */

struct __CFFileDescriptorManager {
	// Covers:
	//   - mAllFileDescriptorsMap
	CFSpinLock_t                      mAllFileDescriptorsLock;

	CFMutableDictionaryRef            mAllFileDescriptorsMap;

	// Covers:
	//   - mRead/WriteFileDescriptors
	//   - mRead/WriteFileDescriptorsNativeDescriptors
	//   - mGeneration
	//   - mThread
	CFSpinLock_t                      mActiveFileDescriptorsLock;

	CFMutableArrayRef                 mReadFileDescriptors;
	CFMutableArrayRef                 mWriteFileDescriptors;
	CFMutableDataRef                  mReadFileDescriptorsNativeDescriptors;
	CFMutableDataRef                  mWriteFileDescriptorsNativeDescriptors;
	volatile UInt32                   mGeneration;
	void *                            mThread;
	Boolean                           mReadFileDescriptorsTimeoutInvalid;
	CFFileDescriptorNativeDescriptor  mWakeupNativeDescriptorPair[2];
};

struct __CFFileDescriptor {
    CFRuntimeBase                     _base;
	CFSpinLock_t                      _lock;
	CFFileDescriptorNativeDescriptor  _descriptor;
	CFFileDescriptorCallBack          _callout;
	CFFileDescriptorContext           _context;
    CFRunLoopSourceRef                _source;
    CFMutableArrayRef                 _loops;
};

typedef void *	    (__CFFileDescriptorContextRetainCallBack)(void *info);
typedef void	    (__CFFileDescriptorContextReleaseCallBack)(void *info);
typedef CFStringRef	(__CFFileDescriptorContextCopyDescriptionCallBack)(void *info);

/* Function Prototypes */

// CFRuntimeClass Functions

static void        __CFFileDescriptorDeallocate(CFTypeRef cf);
static CFStringRef __CFFileDescriptorCopyDescription(CFTypeRef cf);

// Other Functions

static CFFileDescriptorRef __CFFileDescriptorCreateWithNative(CFAllocatorRef                   allocator,
															  CFFileDescriptorNativeDescriptor fd,
															  Boolean                          closeOnInvalidate,
															  CFFileDescriptorCallBack         callout,
															  const CFFileDescriptorContext *  context,
															  Boolean                          reuseExistingInstance);
static SInt32              __CFFileDescriptorManagerCreateWakeupPipe(void);
static void                __CFFileDescriptorManagerInitialize_Locked(void);
static void                __CFFileDescriptorInvalidate_Retained(CFFileDescriptorRef f);

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

static struct __CFFileDescriptorManager __sCFFileDescriptorManager = {
	CFSpinLockInit,
	NULL,
	CFSpinLockInit,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	NULL,
	TRUE,
	{ __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR, __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR }
};

// MARK: Inline Functions

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

CF_INLINE CFIndex __CFFileDescriptorFdGetSize(CFDataRef fdSet) {
#if DEPLOYMENT_TARGET_WINDOWS
    fd_set* set = (fd_set*)CFDataGetBytePtr(fdSet);
    return set ? set->fd_count : 0;
#else
    return NBBY * CFDataGetLength(fdSet);
#endif
}

CF_INLINE Boolean __CFFileDescriptorFdSet(CFFileDescriptorNativeDescriptor fd, CFMutableDataRef fdSet) {
    /* returns true if a change occurred, false otherwise */
    Boolean retval = false;

	__Require(fd != __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR, done);
	__Require(fd >= 0, done);

#if DEPLOYMENT_TARGET_WINDOWS
	fd_set* set = (fd_set*)CFDataGetMutableBytePtr(fdSet);
	if ((set->fd_count * sizeof(HANDLE) + sizeof(u_int)) >= CFDataGetLength(fdSet)) {
		CFDataIncreaseLength(fdSet, sizeof(HANDLE));
		set = (fd_set*)CFDataGetMutableBytePtr(fdSet);
	}
	if (!FD_ISSET(fd, set)) {
		retval = true;
		FD_SET(fd, set);
	}
#else
	CFIndex numFds = NBBY * CFDataGetLength(fdSet);
	fd_mask *fds_bits;
	if (fd >= numFds) {
		CFIndex oldSize = numFds / NFDBITS, newSize = (fd + NFDBITS) / NFDBITS, changeInBytes = (newSize - oldSize) * sizeof(fd_mask);
		CFDataIncreaseLength(fdSet, changeInBytes);
		fds_bits = (fd_mask *)CFDataGetMutableBytePtr(fdSet);
		memset(fds_bits + oldSize, 0, changeInBytes);
	} else {
		fds_bits = (fd_mask *)CFDataGetMutableBytePtr(fdSet);
	}
	if (!FD_ISSET(fd, (fd_set *)fds_bits)) {
		retval = true;
		FD_SET(fd, (fd_set *)fds_bits);
	}
#endif /* DEPLOYMENT_TARGET_WINDOWS */

 done:
    return retval;
}

CF_INLINE Boolean __CFFileDescriptorFdClr(CFFileDescriptorNativeDescriptor fd, CFMutableDataRef fdSet) {
    /* returns true if a change occurred, false otherwise */
    Boolean retval = false;

	__Require(fd != __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR, done);
	__Require(fd >= 0, done);

#if DEPLOYMENT_TARGET_WINDOWS
	fd_set* set = (fd_set*)CFDataGetMutableBytePtr(fdSet);
	if (FD_ISSET(fd, set)) {
		retval = true;
		FD_CLR(fd, set);
	}
#else
	CFIndex numFds = NBBY * CFDataGetLength(fdSet);
	fd_mask *fds_bits;
	if (fd < numFds) {
		fds_bits = (fd_mask *)CFDataGetMutableBytePtr(fdSet);
		if (FD_ISSET(fd, (fd_set *)fds_bits)) {
			retval = true;
			FD_CLR(fd, (fd_set *)fds_bits);
		}
	}
#endif /* DEPLOYMENT_TARGET_WINDOWS */

 done:
    return retval;
}

// MARK: Other Functions

/* static */ CFFileDescriptorRef
__CFFileDescriptorCreateWithNative(CFAllocatorRef                   allocator,
								   CFFileDescriptorNativeDescriptor fd,
								   Boolean                          closeOnInvalidate,
								   CFFileDescriptorCallBack         callout,
								   const CFFileDescriptorContext *  context,
								   Boolean                          reuseExistingInstance) {
    CHECK_FOR_FORK();
	Boolean                          cached;
	CFFileDescriptorRef              result = NULL;

	__CFFileDescriptorEnter();

	// Ensure the native descriptor is valid.

	__Require(fd > __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR, done);

	// Check and, if necessary, perform lazy initialization and
	// start-up of the file descriptor manager.

    __CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);
    if (__sCFFileDescriptorManager.mReadFileDescriptors == NULL) {
		__CFFileDescriptorManagerInitialize_Locked();
	}
    __CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	// Check and, if necessary, perform lazy creation of a map for all
	// file descriptor objects under management.

    __CFSpinLock(&__sCFFileDescriptorManager.mAllFileDescriptorsLock);

    if (__sCFFileDescriptorManager.mAllFileDescriptorsMap == NULL) {
        __sCFFileDescriptorManager.mAllFileDescriptorsMap =
			CFDictionaryCreateMutable(kCFAllocatorSystemDefault,
									  0,
									  NULL,
									  &kCFTypeDictionaryValueCallBacks);
    }

	__CFFileDescriptorMaybeLog("Manager initialized.\n");

	// The file descriptor manager is running and we have a map for
	// all file descriptor objects under management; start the work of
	// creating this file descriptor.

	// First, check the management map and see if we already have an
	// object under management for the specified native file
	// descriptor.

	__CFFileDescriptorMaybeLog("Checking cache %p for descriptor %d...\n",
							   __sCFFileDescriptorManager.mAllFileDescriptorsMap,
							   fd);

	cached = CFDictionaryGetValueIfPresent(__sCFFileDescriptorManager.mAllFileDescriptorsMap, (void *)(uintptr_t)fd, (const void **)&result);

	__CFFileDescriptorMaybeLog("Descriptor %d cached? %u\n", fd, cached);

	// If we had an object in the management map, we can either reuse
	// or recycle it, depending on what the caller specified.

	if (cached) {
		// Reuse it

        if (reuseExistingInstance) {
			__CFSpinUnlock(&__sCFFileDescriptorManager.mAllFileDescriptorsLock);
			CFRetain(result);

		// Recycle it

		} else {
			__CFFileDescriptorMaybeLog("reuseExistingInstance is %u, "
									   "removing existing instance %p\n",
									   reuseExistingInstance, result);

			__CFSpinUnlock(&__sCFFileDescriptorManager.mAllFileDescriptorsLock);

			CFFileDescriptorInvalidate(result);
			result = NULL;

			__CFSpinLock(&__sCFFileDescriptorManager.mAllFileDescriptorsLock);
		}
	}

	// Either there was nothing in the cache or there was something in
	// the cache and the caller wants it recycled. Regardless, we are
	// creating a file descriptor object as if anew.

	if (!cached || !reuseExistingInstance) {
		result = (CFFileDescriptorRef)_CFRuntimeCreateInstance(allocator,
															   __kCFFileDescriptorTypeID,
															   sizeof(struct __CFFileDescriptor) - sizeof(CFRuntimeBase),
															   NULL);
		__Require_Action(result != NULL,
						 done,
						 __CFSpinUnlock(&__sCFFileDescriptorManager.mAllFileDescriptorsLock));

		__CFFileDescriptorClearWriteSignaled(result);
		__CFFileDescriptorClearReadSignaled(result);

		CF_SPINLOCK_INIT_FOR_STRUCTS(result->_lock);

		result->_descriptor              = fd;
		result->_loops                   = CFArrayCreateMutable(allocator, 0, NULL);
		result->_callout                 = callout;

		result->_context.info            = NULL;
		result->_context.retain          = NULL;
		result->_context.release         = NULL;
		result->_context.copyDescription = NULL;

		// Error-wise, we should have clear sailing from here. Add the
		// newly-created descriptor object to the management map.

		CFDictionaryAddValue(__sCFFileDescriptorManager.mAllFileDescriptorsMap,
							 (void *)(uintptr_t)fd,
							 result);

		__CFSpinUnlock(&__sCFFileDescriptorManager.mAllFileDescriptorsLock);

		// If the caller provided context, establish it, retaining the
		// user-supplied info pointer if a retain function pointer was
		// supplied.

		if (context != NULL) {
			void *contextInfo = ((context->retain != NULL) ? (void *)context->retain(context->info) : context->info);

			__CFFileDescriptorLock(result);

			result->_context.retain          = context->retain;
			result->_context.release         = context->release;
			result->_context.copyDescription = context->copyDescription;
			result->_context.info            = contextInfo;

			__CFFileDescriptorUnlock(result);
		}
	}

 done:
	__CFFileDescriptorExit();

	return result;
}

/* static */ SInt32
__CFFileDescriptorManagerCreateWakeupPipe(void)
{
	return pipe(__sCFFileDescriptorManager.mWakeupNativeDescriptorPair);
}

/* static */ void
__CFFileDescriptorManagerInitialize_Locked(void) {
	SInt32 status;

	__CFFileDescriptorEnter();

    __sCFFileDescriptorManager.mReadFileDescriptors  = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
    __sCFFileDescriptorManager.mWriteFileDescriptors = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, NULL);

    __sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors  = CFDataCreateMutable(kCFAllocatorSystemDefault, 0);
    __sCFFileDescriptorManager.mWriteFileDescriptorsNativeDescriptors = CFDataCreateMutable(kCFAllocatorSystemDefault, 0);

#if DEPLOYMENT_TARGET_WINDOWS
#warning "CFFileDescriptor Windows portability issue!"
#endif

	status = __CFFileDescriptorManagerCreateWakeupPipe();

	if (status < 0) {
        CFLog(kCFLogLevelWarning, CFSTR("*** Could not create wakeup pipe for CFFileDescriptor!!!"));
    } else {
		status = fcntl(__sCFFileDescriptorManager.mWakeupNativeDescriptorPair[0], F_SETFL, O_NONBLOCK);
		__Require(status == 0, done);

		status = fcntl(__sCFFileDescriptorManager.mWakeupNativeDescriptorPair[1], F_SETFL, O_NONBLOCK);
		__Require(status == 0, done);

        __CFFileDescriptorFdSet(__sCFFileDescriptorManager.mWakeupNativeDescriptorPair[1],
								__sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors);
    }

 done:
	__CFFileDescriptorExit();

	return;
}

/* static */ void
__CFFileDescriptorInvalidate_Retained(CFFileDescriptorRef f) {
    __CFFileDescriptorLock(f);

    if (__CFFileDescriptorIsValid(f)) {
		__CFFileDescriptorContextReleaseCallBack *contextReleaseCallBack = NULL;
        void *                                    contextInfo            = NULL;

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

        contextInfo            = f->_context.info;
        contextReleaseCallBack = f->_context.release;

        f->_context.info = NULL;
        f->_context.retain = NULL;
        f->_context.release = NULL;
        f->_context.copyDescription = NULL;

        __CFFileDescriptorUnlock(f);

		// OK, we are done with the object. Release the context, if
		// a function has been provided to do so.

        if (contextReleaseCallBack != NULL) {
            contextReleaseCallBack(contextInfo);
        }
    } else {
        __CFFileDescriptorUnlock(f);
    }
}

// MARK: CFRuntimeClass Functions

__private_extern__ void __CFFileDescriptorInitialize(void) {
	__CFFileDescriptorEnter();

    __kCFFileDescriptorTypeID = _CFRuntimeRegisterClass(&__CFFileDescriptorClass);

	__CFFileDescriptorExit();
}

/* static */ void
__CFFileDescriptorDeallocate(CFTypeRef cf) {
    __CFGenericValidateType(cf, CFFileDescriptorGetTypeID());
}

/* static */ CFStringRef
__CFFileDescriptorCopyDescription(CFTypeRef cf) {
	static const char * const                          kUnknownName = "???";
    CFFileDescriptorRef                                f = (CFFileDescriptorRef)cf;
	void *                                             addr;
	const char *                                       name;
    void *                                             contextInfo                    = NULL;
	__CFFileDescriptorContextCopyDescriptionCallBack * contextCopyDescriptionCallBack = NULL;
    CFStringRef                                        contextDesc                    = NULL;
    CFMutableStringRef                                 result                         = NULL;

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
    contextCopyDescriptionCallBack = f->_context.copyDescription;

    __CFFileDescriptorUnlock(f);

    if (contextInfo != NULL && contextCopyDescriptionCallBack != NULL) {
        contextDesc = contextCopyDescriptionCallBack(contextInfo);
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

/**
 *  Returns the type identifier for the CFFileDescriptor opaque type.
 *
 *  @returns
 *    The type identifier for the CFFileDescriptor opaque type.
 *
 */
CFTypeID
CFFileDescriptorGetTypeID(void) {
	return __kCFFileDescriptorTypeID;
}

/**
 *  Creates a new CFFileDescriptor.
 *
 *  @param[in]  allocator          The allocator to use to allocate
 *                                 memory for the new file descriptor
 *                                 object. Pass NULL or
 *                                 kCFAllocatorDefault to use the
 *                                 current default allocator.
 *  @param[in]  fd                 The file descriptor for the new
 *                                 CFFileDescriptor.
 *  @param[in]  closeOnInvalidate  true if the new CFFileDescriptor
 *                                 should close fd when it is
 *                                 invalidated, otherwise false.
 *  @param[in]  callout            The CFFileDescriptorCallBack for
 *                                 the new CFFileDescriptor.
 *  @param[in]  context            Contextual information for the new
 *                                 CFFileDescriptor.
 *
 *  @returns
 *    A new CFFileDescriptor or NULL if there was a problem creating
 *    the object. Ownership follows the "The Create Rule".
 *
 */
CFFileDescriptorRef
CFFileDescriptorCreate(CFAllocatorRef                   allocator,
					   CFFileDescriptorNativeDescriptor fd,
					   Boolean                          closeOnInvalidate,
					   CFFileDescriptorCallBack         callout,
					   const CFFileDescriptorContext *  context) {
	static const Boolean kReuseExistingInstance = TRUE;
	CFFileDescriptorRef  result                 = NULL;

	__CFFileDescriptorEnter();

	result = __CFFileDescriptorCreateWithNative(allocator,
												fd,
												closeOnInvalidate,
												callout,
												context,
												kReuseExistingInstance);

	__CFFileDescriptorExit();

	return result;
}

/**
 *  Returns the native file descriptor for a given CFFileDescriptor.
 *
 *  @param[in]  f  A CFFileDescriptor.
 *
 *  @returns
 *    The native file descriptor for f.
 *
 */
CFFileDescriptorNativeDescriptor
CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f) {
    CHECK_FOR_FORK();
	int result;

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    result = f->_descriptor;

	return result;
}

/**
 *  Gets the context for a given CFFileDescriptor.
 *
 *  @param[in]      f        A CFFileDescriptor.
 *  @param[in,out]  context  Upon return, contains the context passed
 *                           to f in CFFileDescriptorCreate.
 *
 */
void
CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context) {
    CHECK_FOR_FORK();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    CFAssert1(context->version == 0, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);

    *context = f->_context;
}

/**
 *  Enables callbacks for a given CFFileDescriptor.
 *
 *  @note
 *    Callbacks are one-shot and must be re-enabled, potentially from
 *    the callback itself, for each desired invocation.
 *
 *  @param[in]  f              A CFFileDescriptor.
 *  @param[in]  callBackTypes  A bitmask that specifies which
 *                             callbacks to enable.
 *
 *  @sa CFFileDescriptorDisableCallBacks
 */
void
CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes) {
    CHECK_FOR_FORK();

	__CFFileDescriptorEnter();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

	__Require(callBackTypes != 0, done);

 done:
	__CFFileDescriptorExit();

	return;
}

/**
 *  Disables callbacks for a given CFFileDescriptor.
 *
 *  @param[in]  f              A CFFileDescriptor.
 *  @param[in]  callBackTypes  A bitmask that specifies which
 *                             callbacks to disable.
 *
 *  @sa CFFileDescriptorEnableCallBacks
 */
void
CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes) {
    CHECK_FOR_FORK();

	__CFFileDescriptorEnter();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

	__Require(callBackTypes != 0, done);

 done:
	__CFFileDescriptorExit();

	return;
}

/**
 *  @brief
 *    Invalidates a CFFileDescriptor object.
 *
 *  Once invalidated, the CFFileDescriptor object will no longer be
 *  read from or written to at the Core Fundation level.
 *
 *  If you passed @a true for the @a closeOnInvalidate parameter when
 *  you called #CFFileDescriptorCreate, this function also closes the
 *  underlying file descriptor. If you passed @a false, you must close
 *  the descriptor yourself @a after invalidating the CFFileDescriptor
 *  object.
 *
 *  @warning
 *    You must invalidate the CFFileDescriptor before closing the
 *    underlying file descriptor.
 *
 *  @param[in]  f  A CFFileDescriptor.
 *
 */
void
CFFileDescriptorInvalidate(CFFileDescriptorRef f) {
    CHECK_FOR_FORK();

	__CFFileDescriptorEnter();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

#if defined(LOG_CFFILEDESCRIPTOR)
    fprintf(stdout, "invalidating file descriptor %d\n", f->_descriptor);
#endif

    CFRetain(f);

	__CFFileDescriptorInvalidate_Retained(f);

    CFRelease(f);

	__CFFileDescriptorExit();
}

/**
 *  Returns a Boolean value that indicates whether the native file
 *  descriptor for a given CFFileDescriptor is valid.
 *
 *  @param[in]  f  A CFFileDescriptor.
 *
 *  @returns
 *    true if the native file descriptor for f is valid, otherwise
 *    false.
 *
 */
Boolean
CFFileDescriptorIsValid(CFFileDescriptorRef f) {
    CHECK_FOR_FORK();
	Boolean result;

	__CFFileDescriptorEnter();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    result = __CFFileDescriptorIsValid(f);

	__CFFileDescriptorEnter();

	return result;
}

CFRunLoopSourceRef
CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator, CFFileDescriptorRef f, CFIndex order) {
    CHECK_FOR_FORK();
    CFRunLoopSourceRef result = NULL;

	__CFFileDescriptorEnter();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    __CFFileDescriptorLock(f);

    __CFFileDescriptorUnlock(f);

	__CFFileDescriptorExit();

	return result;
}
