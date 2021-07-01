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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

#include <AssertMacros.h>

#include <CoreFoundation/CFFileDescriptor.h>
#include "CFInternal.h"

#if DEPLOYMENT_TARGET_MACOS || DEPLOYMENT_TARGET_LINUX
#include <dlfcn.h>
#include <unistd.h>
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
#define __kCFInfoCFFileDescriptorCallBackTypesFirstBit             0
#define __kCFInfoCFFileDescriptorCallBackTypesLastBit              2

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
#if DEPLOYMENT_TARGET_WINDOWS
	// We need to select on exceptFDs on Win32 to hear of connect failures
	CFMutableDataRef                  mExceptFileDescriptorsNativeDescriptors;
#endif
    volatile UInt32                   mGeneration;
    void *                            mThread;
    Boolean                           mReadFileDescriptorsTimeoutInvalid;
    CFFileDescriptorNativeDescriptor  mWakeupNativeDescriptorPipe[2];
};

struct __CFFileDescriptor {
    CFRuntimeBase                     _base;
    struct {
        unsigned disabled:8;
        unsigned enabled:8;
        unsigned unused:16;
    }                                 _flags;
    CFSpinLock_t                      _lock;
    CFFileDescriptorNativeDescriptor  _descriptor;
    SInt32                            _errorCode;
    CFFileDescriptorCallBack          _callout;
    CFFileDescriptorContext           _context;
    SInt32                            _fileDescriptorSetCount;
    CFRunLoopSourceRef                _rlsource;
    CFMutableArrayRef                 _rloops;
};

struct __CFFileDescriptorManagerWatchedDescriptors {
	fd_set * _read;
	fd_set * _write;
	fd_set * _except;
};

struct __CFFileDescriptorManagerSelectedDescriptorsContainer {
	CFMutableArrayRef _descriptors;
	CFIndex           _index;
};

struct __CFFileDescriptorManagerSelectedDescriptors {
	struct __CFFileDescriptorManagerSelectedDescriptorsContainer _read;
	struct __CFFileDescriptorManagerSelectedDescriptorsContainer _write;
};

struct __CFFileDescriptorManagerSelectState {
	struct __CFFileDescriptorManagerWatchedDescriptors  _watches;
	struct __CFFileDescriptorManagerSelectedDescriptors _selected;
};

typedef void *      (__CFFileDescriptorContextRetainCallBack)(void *info);
typedef void        (__CFFileDescriptorContextReleaseCallBack)(void *info);
typedef CFStringRef (__CFFileDescriptorContextCopyDescriptionCallBack)(void *info);

typedef void        (__CFFileDescriptorReadyHandler)(CFFileDescriptorRef f, Boolean value);

enum {
    __kWakeupPipeWriterIndex = 0,
    __kWakeupPipeReaderIndex = 1
};

enum {
    __kWakeupReasonDisable  = 'u',
    __kWakeupReasonEnable   = 'r',
    __kWakeupReasonPerform  = 'p',
    __kWakeupReasonSchedule = 's'
};

/* Function Prototypes */

// CFRuntimeClass Functions

static void        __CFFileDescriptorDeallocate(CFTypeRef cf);
static CFStringRef __CFFileDescriptorCopyDescription(CFTypeRef cf);

// CFRunLoopSource Functions

static void        __CFFileDescriptorRunLoopSchedule(void *info, CFRunLoopRef rl, CFStringRef mode);
static void        __CFFileDescriptorRunLoopCancel(void *info, CFRunLoopRef rl, CFStringRef mode);
static void        __CFFileDescriptorRunLoopPerform(void *info);

// Other Functions

static void *              __CFFileDescriptorAllocateWatchedDescriptors(struct __CFFileDescriptorManagerWatchedDescriptors *watches, CFIndex count);
static void *              __CFFileDescriptorAllocateSelectState(struct __CFFileDescriptorManagerSelectState *state, CFIndex watches_count);
static void *              __CFFileDescriptorAllocateSelectedDescriptors(struct __CFFileDescriptorManagerSelectedDescriptors *selected);
static void *              __CFFileDescriptorAllocateSelectedDescriptorsContainer(struct __CFFileDescriptorManagerSelectedDescriptorsContainer *container);
static void                __CFFileDescriptorCalculateMinTimeout_Locked(const void * value, void * context);
static CFRunLoopRef        __CFFileDescriptorCopyRunLoopToWakeUp(CFFileDescriptorRef f);
static CFFileDescriptorRef __CFFileDescriptorCreateWithNative(CFAllocatorRef                   allocator,
                                                              CFFileDescriptorNativeDescriptor fd,
                                                              Boolean                          closeOnInvalidate,
                                                              CFFileDescriptorCallBack         callout,
                                                              const CFFileDescriptorContext *  context,
                                                              Boolean                          reuseExistingInstance);
static Boolean             __CFFileDescriptorDisableCallBacks_Locked(CFFileDescriptorRef f,
                                                                     CFOptionFlags callBackTypes);
static void                __CFFileDescriptorDoCallback_LockedAndUnlock(CFFileDescriptorRef f);
static void                __CFFileDescriptorEnableCallBacks_LockedAndUnlock(CFFileDescriptorRef f,
                                                                             CFOptionFlags callBackTypes,
                                                                             Boolean force,
                                                                             char wakeupReason);
static void                __CFFileDescriptorFindAndAppendInvalidDescriptors(CFArrayRef descriptors,
																			 CFMutableArrayRef array,
																			 const char *what);
static void                __CFFileDescriptorHandleRead(CFFileDescriptorRef f,
														Boolean causedByTimeout);
static void                __CFFileDescriptorHandleReadyDescriptors(CFMutableArrayRef descriptors,
																	CFIndex count,
																	__CFFileDescriptorReadyHandler handler,
																	Boolean handler_flag);
static void                __CFFileDescriptorHandleWrite(CFFileDescriptorRef f,
														 Boolean callBackNow);
static void                __CFFileDescriptorInvalidate_Retained(CFFileDescriptorRef f);
static void                __CFFileDescriptorManager(void * arg);
static SInt32              __CFFileDescriptorManagerCreateWakeupPipe(void);
static void                __CFFileDescriptorManagerHandleError(void);
static void                __CFFileDescriptorManagerHandleReadyDescriptors(struct __CFFileDescriptorManagerSelectState *state,
																		   CFIndex max,
																		   Boolean causedByTimeout);
static void                __CFFileDescriptorManagerHandleTimeout(struct __CFFileDescriptorManagerSelectState *state, const struct timeval *elapsed);
static void                __CFFileDescriptorManagerInitialize_Locked(void);
static Boolean  		   __CFFileDescriptorManagerMaybeAdd_Locked(CFFileDescriptorRef f,
																	Boolean forRead,
																	Boolean forWrite,
																	Boolean force);
static CFIndex             __CFFileDescriptorManagerPrepareWatches(struct __CFFileDescriptorManagerWatchedDescriptors *watches, struct timeval *timeout);
#if LOG_CFFILEDESCRIPTOR
static void                __CFFileDescriptorManagerPrepareWatchesMaybeLog(void);
#endif
static void                __CFFileDescriptorManagerProcessState(struct __CFFileDescriptorManagerSelectState *state, CFIndex count, CFIndex max, const struct timeval *elapsed);
static void         	   __CFFileDescriptorManagerRemoveInvalidFileDescriptors(void);
static void                __CFFileDescriptorManagerRemove_Locked(CFFileDescriptorRef f);
static Boolean             __CFFileDescriptorManagerShouldWake_Locked(CFFileDescriptorRef f,
                                                                      CFOptionFlags callBackTypes);
static void                __CFFileDescriptorManagerWakeup(char reason);
static void                __CFFileDescriptorMaybeLogFileDescriptorList(CFArrayRef descriptors, CFDataRef fdSet, Boolean onlyIfSet);
static const char *        __CFFileDescriptorNameForSymbol(void *address);
static void *              __CFFileDescriptorReallocateWatchedDescriptors(struct __CFFileDescriptorManagerWatchedDescriptors *watches, CFIndex count);
static Boolean             __CFNativeFileDescriptorIsValid(CFFileDescriptorNativeDescriptor fd);

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
#if DEPLOYMENT_TARGET_WINDOWS
	NULL,
#endif
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

CF_INLINE uint8_t __CFFileDescriptorCallBackTypes(CFFileDescriptorRef f) {
    return (uint8_t)__CFBitfieldGetValue(((const CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
                                         __kCFInfoCFFileDescriptorCallBackTypesLastBit,
                                         __kCFInfoCFFileDescriptorCallBackTypesFirstBit);
}

CF_INLINE void __CFFileDescriptorSetCallBackTypes(CFFileDescriptorRef f, uint8_t types) {
    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
                         __kCFInfoCFFileDescriptorCallBackTypesLastBit,
                         __kCFInfoCFFileDescriptorCallBackTypesFirstBit,
                         types);
}

CF_INLINE void __CFFileDescriptorClearCallBackTypes(CFFileDescriptorRef f) {
    const uint8_t kNoTypes = 0;

    __CFBitfieldSetValue(((CFRuntimeBase *)f)->_cfinfo[CF_INFO_BITS],
                         __kCFInfoCFFileDescriptorCallBackTypesLastBit,
                         __kCFInfoCFFileDescriptorCallBackTypesFirstBit,
                         kNoTypes);
}

CF_INLINE void __CFFileDescriptorLock(CFFileDescriptorRef f) {
    __CFSpinLock(&(f->_lock));
}

CF_INLINE void __CFFileDescriptorUnlock(CFFileDescriptorRef f) {
    __CFSpinUnlock(&(f->_lock));
}

CF_INLINE Boolean __CFFileDescriptorIsScheduled(CFFileDescriptorRef f) {
    return (f->_fileDescriptorSetCount > 0);
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

CF_INLINE SInt32 __CFFileDescriptorManagerLastError(void) {
#if DEPLOYMENT_TARGET_WINDOWS
    return WSAGetLastError();
#else
    return thread_errno();
#endif
}

CF_INLINE Boolean __CFFileDescriptorManagerSetFDForRead_Locked(CFFileDescriptorRef f) {
    __sCFFileDescriptorManager.mReadFileDescriptorsTimeoutInvalid = true;

    return __CFFileDescriptorFdSet(f->_descriptor,
                                   __sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors);
}

CF_INLINE Boolean __CFFileDescriptorManagerClearFDForRead_Locked(CFFileDescriptorRef f) {
    __sCFFileDescriptorManager.mReadFileDescriptorsTimeoutInvalid = true;

    return __CFFileDescriptorFdClr(f->_descriptor,
                                   __sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors);
}

CF_INLINE Boolean __CFFileDescriptorManagerSetFDForWrite_Locked(CFFileDescriptorRef f) {
    return __CFFileDescriptorFdSet(f->_descriptor,
                                   __sCFFileDescriptorManager.mWriteFileDescriptorsNativeDescriptors);
}

CF_INLINE Boolean __CFFileDescriptorManagerClearFDForWrite_Locked(CFFileDescriptorRef f) {
    return __CFFileDescriptorFdClr(f->_descriptor,
                                   __sCFFileDescriptorManager.mWriteFileDescriptorsNativeDescriptors);
}

// MARK: Other Functions

/* static */ void *
__CFFileDescriptorAllocateWatchedDescriptors(struct __CFFileDescriptorManagerWatchedDescriptors *watches, CFIndex count)
{
	void * result = NULL;

	__Require(watches != NULL, done);
	__Require(count > 0, done);

#if !DEPLOYMENT_TARGET_WINDOWS
    watches->_except = NULL;
    watches->_write  = (fd_set *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count * sizeof(fd_mask), 0);
    watches->_read   = (fd_set *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count * sizeof(fd_mask), 0);
#else
    watches->_except = (fd_set *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count * sizeof(HANDLE) + sizeof(u_int), 0);
    watches->_write  = (fd_set *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count * sizeof(HANDLE) + sizeof(u_int), 0);
    watches->_read   = (fd_set *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count * sizeof(HANDLE) + sizeof(u_int), 0);
#endif /* !DEPLOYMENT_TARGET_WINDOWS */

	result = watches;

 done:
	return result;
}

static void *
__CFFileDescriptorAllocateSelectState(struct __CFFileDescriptorManagerSelectState *state, CFIndex watches_count) {
	void *result = NULL;

	__Require(state != NULL, done);

	result = __CFFileDescriptorAllocateWatchedDescriptors(&state->_watches,
														  watches_count);
	__Require(result != NULL, done);

	result = __CFFileDescriptorAllocateSelectedDescriptors(&state->_selected);
	__Require(result != NULL, done);

	result = state;

 done:
	return result;
}

/* static */ void *
__CFFileDescriptorAllocateSelectedDescriptors(struct __CFFileDescriptorManagerSelectedDescriptors *selected) {
	void *result = NULL;

	__Require(selected != NULL, done);

	result = __CFFileDescriptorAllocateSelectedDescriptorsContainer(&selected->_read);
	__Require(result != NULL, done);

	result = __CFFileDescriptorAllocateSelectedDescriptorsContainer(&selected->_write);
	__Require(result != NULL, done);

	result = selected;

 done:
	return result;
}

static void *
__CFFileDescriptorAllocateSelectedDescriptorsContainer(struct __CFFileDescriptorManagerSelectedDescriptorsContainer *container) {
	void *result = NULL;

	__Require(container != NULL, done);

	container->_descriptors = CFArrayCreateMutable(kCFAllocatorSystemDefault,
														0,
														&kCFTypeArrayCallBacks);
	container->_index = 0;

	result = container;

 done:
	return result;
}

/* static */ CFRunLoopRef
__CFFileDescriptorCopyRunLoopToWakeUp(CFFileDescriptorRef f) {
	const CFIndex count  = CFArrayGetCount(f->_rloops);
	CFIndex       index  = 0;
    CFRunLoopRef  result = NULL;

	__Require_Quiet(count > 0, done);

	result = (CFRunLoopRef)CFArrayGetValueAtIndex(f->_rloops, index);

	for (index = 1; result != NULL && index < count; index++) {
		CFRunLoopRef value = (CFRunLoopRef)CFArrayGetValueAtIndex(f->_rloops, index);
		if (value != result) {
			result = NULL;
		}
	}

	// There is more than one different runloop, so we must pick one.

	if (result == NULL) {
		Boolean foundIt = false;
		Boolean foundBackup = false;
		CFIndex foundIndex = 0;

		/* ideally, this would be a run loop which isn't also in a
		 * signaled state for this or another source, but that's tricky;
		 * we pick one that is running in an appropriate mode for this
		 * source, and from those if possible one that is waiting; then
		 * we move this run loop to the end of the list to scramble them
		 * a bit, and always search from the front */

		for (index = 0; !foundIt && index < count; index++) {
			CFRunLoopRef value = (CFRunLoopRef)CFArrayGetValueAtIndex(f->_rloops, index);
			CFStringRef currentMode = CFRunLoopCopyCurrentMode(value);
			if (NULL != currentMode) {
				if (CFRunLoopContainsSource(value, f->_rlsource, currentMode)) {
					if (CFRunLoopIsWaiting(value)) {
						foundIndex = index;
						foundIt = true;
					} else if (!foundBackup) {
						foundIndex = index;
						foundBackup = true;
					}
				}

				CFRelease(currentMode);
			}
		}

		result = (CFRunLoopRef)CFArrayGetValueAtIndex(f->_rloops, foundIndex);

		CFRetain(result);

		CFArrayRemoveValueAtIndex(f->_rloops, foundIndex);
		CFArrayAppendValue(f->_rloops, result);

	} else {
		CFRetain(result);

	}

 done:
    return result;
}

/* static */ void
__CFFileDescriptorCalculateMinTimeout_Locked(const void * value,
											 void * context) {
	CFFileDescriptorRef f = (CFFileDescriptorRef)(value);
	struct timeval** minTime = (struct timeval**)(context);

#if 0
	if (timerisset(&f->_readBufferTimeout) && (*minTime == NULL || timercmp(&f->_readBufferTimeout, *minTime, <)))
		*minTime = &f->_readBufferTimeout;
   else if (f->_leftoverBytes) {
      /* If there's anyone with leftover bytes, they'll need to be awoken immediately */
      static struct timeval sKickerTime = { 0, 0 };
      *minTime = &sKickerTime;
   }
#else
	(void)f;
	(void)minTime;
#endif
}

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

        __CFFileDescriptorClearCallBackTypes(result);

        __CFFileDescriptorSetValid(result);

        __CFFileDescriptorClearWriteSignaled(result);
        __CFFileDescriptorClearReadSignaled(result);

        result->_flags.enabled           = 0;
        result->_flags.disabled          = 0;

        CF_SPINLOCK_INIT_FOR_STRUCTS(result->_lock);

        result->_descriptor              = fd;
		result->_errorCode               = 0;
        result->_fileDescriptorSetCount  = 0;
        result->_rlsource                = NULL;
        result->_rloops                  = CFArrayCreateMutable(allocator, 0, NULL);
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

/* static */ Boolean
__CFFileDescriptorDisableCallBacks_Locked(CFFileDescriptorRef f, CFOptionFlags callBackTypes) {
    Boolean result = FALSE;

    __CFFileDescriptorEnter();

    if (__CFFileDescriptorIsValid(f) && __CFFileDescriptorIsScheduled(f)) {
        callBackTypes &= __CFFileDescriptorCallBackTypes(f);
        f->_flags.disabled |= callBackTypes;

        __CFFileDescriptorMaybeLog("unscheduling file descriptor %d disabled callback "
                                   "types 0x%x for callback types 0x%lx\n",
                                   f->_descriptor,
                                   f->_flags.disabled,
                                   callBackTypes);

        result = __CFFileDescriptorManagerShouldWake_Locked(f, callBackTypes);
    }

    __CFFileDescriptorExit();

    return result;
}

static void
__CFFileDescriptorDoCallback_LockedAndUnlock(CFFileDescriptorRef f) {
    CFFileDescriptorCallBack callout = NULL;
    void *contextInfo = NULL;
    Boolean readSignaled = false;
    Boolean writeSignaled = false;
    Boolean calledOut = false;
    uint8_t callBackTypes;

    __CFFileDescriptorEnter();

    callBackTypes = __CFFileDescriptorCallBackTypes(f);
    readSignaled  = __CFFileDescriptorIsReadSignaled(f);
    writeSignaled = __CFFileDescriptorIsWriteSignaled(f);

    __CFFileDescriptorClearReadSignaled(f);
    __CFFileDescriptorClearWriteSignaled(f);

    callout     = f->_callout;
    contextInfo = f->_context.info;

    __CFFileDescriptorMaybeLog("entering perform for descriptor %d "
                               "with read signaled %d write signaled %d callback types %x\n",
                               f->_descriptor, readSignaled, writeSignaled, callBackTypes);

    __CFFileDescriptorUnlock(f);

    if ((callBackTypes & kCFFileDescriptorReadCallBack) != 0) {
        if (readSignaled && (!calledOut || CFFileDescriptorIsValid(f))) {
            __CFFileDescriptorMaybeLog("perform calling out read to descriptor %d\n", f->_descriptor);

            if (callout) {
                callout(f, kCFFileDescriptorReadCallBack, contextInfo);
                calledOut = true;
            }
        }
    }

    if ((callBackTypes & kCFFileDescriptorWriteCallBack) != 0) {
        if (writeSignaled && (!calledOut || CFFileDescriptorIsValid(f))) {
            __CFFileDescriptorMaybeLog("perform calling out write to descriptor %d\n", f->_descriptor);

            if (callout) {
                callout(f, kCFFileDescriptorWriteCallBack, contextInfo);
                calledOut = true;
            }
        }
    }

    __CFFileDescriptorExit();
}

/**
 *  @brief
 *    Enables callbacks for a given CFFileDescriptor.
 *
 *  If @a force is not asserted, it is assumed that the file
 *  descriptor has already been added to the file descriptor manager
 *  mReadFileDescriptors and mWriteFileDescriptors arrays.
 *
 *  @note
 *    This should be called with the file descriptor lock held but
 *    returns with it released!
 *
 *  @param[in]  f              A CFFileDescriptor.
 *  @param[in]  callBackTypes  A bitmask that specifies which
 *                             callbacks to enable.
 *  @param[in]  force          A flag indicating that when asserted to
 *                             clear the descriptor callback disabled
 *                             mask and always reenable. If not
 *                             asserted, always respect the callback
 *                             disabled mask which may prevent this
 *                             from enabling any callbacks at all.
 *  @param[in]  wakeupReason   The wakeup reason to send to the file
 *                             descriptor manager to wake it up for
 *                             watched descriptor watching and
 *                             processing.
 *
 *  @sa CFFileDescriptorDisableCallBacks
 */
/* static */ void
__CFFileDescriptorEnableCallBacks_LockedAndUnlock(CFFileDescriptorRef f,
                                                  CFOptionFlags callBackTypes,
                                                  Boolean force,
                                                  char wakeupReason)
{
    Boolean wakeup = FALSE;

    __CFFileDescriptorEnter();

    __CFFileDescriptorMaybeLog("Attempting to enable descriptor %d callbacks 0x%lx w/ reason '%c'\n",
                               f->_descriptor, callBackTypes, wakeupReason);

    __Require(callBackTypes != 0, unlock);

    if (__CFFileDescriptorIsValid(f) && __CFFileDescriptorIsScheduled(f)) {
        Boolean enableRead = FALSE;
        Boolean enableWrite = FALSE;

        if (enableRead || enableWrite) {
            __CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

            if (enableWrite) {

            }

            if (enableRead) {

            }

            if (wakeup && __sCFFileDescriptorManager.mThread == NULL) {
                __sCFFileDescriptorManager.mThread = __CFStartSimpleThread((void*)__CFFileDescriptorManager, 0);
            }

            __CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);
        }
    }

 unlock:
    __CFFileDescriptorUnlock(f);

    if (wakeup) {
        __CFFileDescriptorManagerWakeup(wakeupReason);
    }

    __CFFileDescriptorExit();

    return;
}

static void
__CFFileDescriptorFindAndAppendInvalidDescriptors(CFArrayRef descriptors, CFMutableArrayRef array, const char *what)
{
	const CFIndex count = CFArrayGetCount(descriptors);
	CFIndex index;

	(void)what;

	for (index = 0; index < count; index++) {
		CFFileDescriptorRef f = (CFFileDescriptorRef)CFArrayGetValueAtIndex(descriptors, index);

		if (!__CFNativeFileDescriptorIsValid(f->_descriptor)) {
			__CFFileDescriptorMaybeLog("file descriptor manager found %s descriptor %d invalid\n", what, f->_descriptor);

			CFArrayAppendValue(array, f);
		}
	}

}

/* static */ void
__CFFileDescriptorHandleRead(CFFileDescriptorRef f,
							 Boolean causedByTimeout) {
    __CFFileDescriptorEnter();

    __CFFileDescriptorExit();
}

/* static */ void
__CFFileDescriptorHandleReadyDescriptors(CFMutableArrayRef descriptors, CFIndex count, __CFFileDescriptorReadyHandler handler, Boolean handler_flag)
{
	CFIndex             index;
	CFFileDescriptorRef f;

    __CFFileDescriptorEnter();

	__Require(descriptors != NULL, done);
	__Require(count > 0, done);
	__Require(handler != NULL, done);

	for (index = 0; index < count; index++) {
		f = (CFFileDescriptorRef)CFArrayGetValueAtIndex(descriptors, index);

		if ((CFNullRef)(f) == kCFNull)
			continue;

		__CFFileDescriptorMaybeLog("signaling descriptor %d with handler %s (%p)\n",
								   f->_descriptor,
								   __CFFileDescriptorNameForSymbol(handler),
								   handler);

		handler(f, handler_flag);

		CFArraySetValueAtIndex(descriptors, index, kCFNull);
	}

 done:
    __CFFileDescriptorExit();

	return;
}

/* static */ void
__CFFileDescriptorHandleWrite(CFFileDescriptorRef f,
							  Boolean callBackNow) {
	Boolean       valid;
    SInt32        errorCode = 0;
    CFOptionFlags writeCallBacksAvailable;

    __CFFileDescriptorEnter();

	valid = CFFileDescriptorIsValid(f);
	__Require(valid, done);

#if 0
#if DEPLOYMENT_TARGET_WINDOWS
    if (0 != getsockopt(f->_descriptor, SOL_SOCKET, SO_ERROR, (char *)&errorCode, (socklen_t *)&errorSize)) { errorCode = 0; }
#else
    if (0 != getsockopt(f->_descriptor, SOL_SOCKET, SO_ERROR, (void *)&errorCode, (socklen_t *)&errorSize)) { errorCode = 0; }
#endif
#endif

    if (errorCode) {
		__CFFileDescriptorMaybeLog("error %d on descriptor %d\n", errorCode, f->_descriptor);
	}

    __CFFileDescriptorLock(f);

    writeCallBacksAvailable = __CFFileDescriptorCallBackTypes(f) & (kCFFileDescriptorWriteCallBack);

    if (!__CFFileDescriptorIsValid(f) || ((f->_flags.disabled & writeCallBacksAvailable) == writeCallBacksAvailable)) {
        __CFFileDescriptorUnlock(f);
		goto done;
    }

    f->_errorCode = errorCode;
    __CFFileDescriptorSetWriteSignaled(f);

    __CFFileDescriptorMaybeLog("write signaling source for descriptor %d\n", f->_descriptor);

    if (callBackNow) {
        __CFFileDescriptorDoCallback_LockedAndUnlock(f);
    } else {
        CFRunLoopRef rl;

        CFRunLoopSourceSignal(f->_rlsource);

        rl = __CFFileDescriptorCopyRunLoopToWakeUp(f);

        __CFFileDescriptorUnlock(f);

        if (rl != NULL) {
            CFRunLoopWakeUp(rl);
            CFRelease(rl);
        }
    }

 done:
    __CFFileDescriptorExit();
}

/* static */ void
__CFFileDescriptorInvalidate_Retained(CFFileDescriptorRef f) {
    __CFFileDescriptorLock(f);

    if (__CFFileDescriptorIsValid(f)) {
        __CFFileDescriptorContextReleaseCallBack *contextReleaseCallBack = NULL;
        void *                                    contextInfo            = NULL;
        CFIndex                                   index;
        CFRunLoopSourceRef                        rlsource;

        __CFFileDescriptorClearValid(f);
        __CFFileDescriptorClearWriteSignaled(f);
        __CFFileDescriptorClearReadSignaled(f);

		__CFFileDescriptorManagerRemove_Locked(f);

        if (__CFFileDescriptorShouldCloseOnInvalidate(f)) {
            close(f->_descriptor);
        }

        f->_descriptor = __CFFILEDESCRIPTOR_INVALID_DESCRIPTOR;

		f->_fileDescriptorSetCount = 0;

        for (index = CFArrayGetCount(f->_rloops); index--;) {
			CFRunLoopRef rloop = (CFRunLoopRef)CFArrayGetValueAtIndex(f->_rloops, index);

            CFRunLoopWakeUp(rloop);
        }
        CFRelease(f->_rloops);
        f->_rloops   = NULL;
        rlsource     = f->_rlsource;
        f->_rlsource = NULL;

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

        if (rlsource != NULL) {
            CFRunLoopSourceInvalidate(rlsource);
            CFRelease(rlsource);
        }
    } else {
        __CFFileDescriptorUnlock(f);
    }
}

#ifdef __GNUC__
__attribute__ ((noreturn))
#endif /* __GNUC__ */
/* static */ void
__CFFileDescriptorManager(void * arg) {
	struct __CFFileDescriptorManagerSelectState state;
	void *result;
    CFIndex nrfds, maxnrfds, fdentries = 1;
    struct timeval timeout;
    struct timeval timeBeforeSelect;
    struct timeval timeAfterSelect;
	struct timeval timeElapsed;

    __CFFileDescriptorEnter();

#if defined(__OBJC__)
    if (objc_collecting_enabled()) auto_zone_register_thread(auto_zone());
#endif

	result = __CFFileDescriptorAllocateSelectState(&state, fdentries);
	__Verify_Action(result != NULL, abort());

    for (;;) {
		maxnrfds = __CFFileDescriptorManagerPrepareWatches(&state._watches, &timeout);

		gettimeofday(&timeBeforeSelect, NULL);

        nrfds = select(maxnrfds,
					   state._watches._read,
					   state._watches._write,
					   state._watches._except,
					   &timeout);

		gettimeofday(&timeAfterSelect, NULL);

		timersub(&timeAfterSelect, &timeBeforeSelect, &timeElapsed);

		__CFFileDescriptorMaybeLog("file descriptor manager woke from select, "
								   "ret=%ld\n",
								   nrfds);

		__CFFileDescriptorManagerProcessState(&state, nrfds, maxnrfds, &timeElapsed);
    }

#if defined(__OBJC__)
    if (objc_collecting_enabled()) auto_zone_unregister_thread(auto_zone());
#endif

    __CFFileDescriptorExit();
}


/* static */ SInt32
__CFFileDescriptorManagerCreateWakeupPipe(void)
{
    return pipe(__sCFFileDescriptorManager.mWakeupNativeDescriptorPipe);
}

/* static */ void
__CFFileDescriptorManagerHandleError(void) {
	const SInt32 selectError = __CFFileDescriptorManagerLastError();

	__CFFileDescriptorMaybeLog("file descriptor manager received error %d from select\n", selectError);

	switch (selectError) {

	case EBADF:
		__CFFileDescriptorManagerRemoveInvalidFileDescriptors();
		break;

	default:
		__CFFileDescriptorMaybeLog("Unhandled select error %d: %s\n", selectError, strerror(selectError));
		abort();
		break;

	}
}

/* static */ void
__CFFileDescriptorManagerHandleReadyDescriptors(struct __CFFileDescriptorManagerSelectState *state, CFIndex max, Boolean causedByTimeout) {
	CFIndex    count;
	CFIndex    index;
	fd_set *   tempfds = NULL;

	__CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	// Process descriptors ready for writing

	tempfds = NULL;
	count = CFArrayGetCount(__sCFFileDescriptorManager.mWriteFileDescriptors);
	for (index = 0; index < count; index++) {
		CFFileDescriptorRef f = (CFFileDescriptorRef)CFArrayGetValueAtIndex(__sCFFileDescriptorManager.mWriteFileDescriptors, index);
		CFFileDescriptorNativeDescriptor fd = f->_descriptor;
		// We might have an new element in __sCFFileDescriptorManager.mWriteFileDescriptors that we weren't listening to,
		// in which case we must be sure not to test a bit in the fdset that is
		// outside our mask size.
#if !DEPLOYMENT_TARGET_WINDOWS
		const Boolean fdInBounds = (0 <= fd && fd < max);
#else
		const Boolean fdInBounds = true;
#endif
		if (__CFFILEDESCRIPTOR_INVALID_DESCRIPTOR != fd && fdInBounds) {
			if (FD_ISSET(fd, state->_watches._write)) {
				CFArraySetValueAtIndex(state->_selected._write._descriptors, state->_selected._write._index, f);
				state->_selected._write._index++;
				/* descriptor is removed from fds here, restored by CFFileDescriptorReschedule */
				if (!tempfds) tempfds = (fd_set *)CFDataGetMutableBytePtr(__sCFFileDescriptorManager.mWriteFileDescriptorsNativeDescriptors);
				FD_CLR(fd, tempfds);
#if DEPLOYMENT_TARGET_WINDOWS
				fd_set *exfds = (fd_set *)CFDataGetMutableBytePtr(__sCFFileDescriptorManager.mExceptFileDescriptorsNativeDescriptors);
				FD_CLR(fd, exfds);
#endif

			}
#if DEPLOYMENT_TARGET_WINDOWS
			else if (FD_ISSET(fd, exceptfds)) {
				// On Win32 connect errors come in on exceptFDs.  We treat these as if
				// they had come on writeFDs, since the rest of our Unix-based code
				// expects that.
				CFArrayAppendValue(state->_selected._write._descriptors, f);
				fd_set *exfds = (fd_set *)CFDataGetMutableBytePtr(__sCFFileDescriptorManager.mExceptFileDescriptorsNativeDescriptors);
				FD_CLR(fd, exfds);
			}
#endif
		}
	}

	// Process descriptors ready for reading

	tempfds = NULL;
	count = CFArrayGetCount(__sCFFileDescriptorManager.mReadFileDescriptors);
	for (index = 0; index < count; index++) {
		CFFileDescriptorRef f = (CFFileDescriptorRef)CFArrayGetValueAtIndex(__sCFFileDescriptorManager.mReadFileDescriptors, index);
		CFFileDescriptorNativeDescriptor fd = f->_descriptor;
#if !DEPLOYMENT_TARGET_WINDOWS
		// We might have an new element in __sCFFileDescriptorManager.mReadFileDescriptors that we weren't listening to,
		// in which case we must be sure not to test a bit in the fdset that is
		// outside our mask size.
		const Boolean fdInBounds = (0 <= fd && fd < max);
#else
		// fdset's are arrays, so we don't have that issue above
		const Boolean fdInBounds = true;
#endif
		if (__CFFILEDESCRIPTOR_INVALID_DESCRIPTOR != fd && fdInBounds && FD_ISSET(fd, state->_watches._read)) {
			CFArraySetValueAtIndex(state->_selected._read._descriptors, state->_selected._read._index, f);
			state->_selected._read._index++;
			/* descriptor is removed from fds here, will be restored in read handling or in perform function */
			if (!tempfds) tempfds = (fd_set *)CFDataGetMutableBytePtr(__sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors);
			FD_CLR(fd, tempfds);
		}
	}

	__CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	// Dispatch and handle descriptors ready for writing

	__CFFileDescriptorHandleReadyDescriptors(state->_selected._write._descriptors,
											 state->_selected._write._index,
											 __CFFileDescriptorHandleWrite,
											 FALSE);

	state->_selected._write._index = 0;

	// Dispatch and handle descriptors ready for reading

	__CFFileDescriptorHandleReadyDescriptors(state->_selected._read._descriptors,
											 state->_selected._read._index,
											 __CFFileDescriptorHandleRead,
											 causedByTimeout);

	state->_selected._read._index = 0;
}

/* static */ void
__CFFileDescriptorManagerHandleTimeout(struct __CFFileDescriptorManagerSelectState *state, const struct timeval *elapsed) {
	CFArrayRef array;
	CFIndex    count;
	CFIndex    index;

	__CFFileDescriptorEnter();

	__CFFileDescriptorMaybeLog("file descriptor manager received timeout - "
							   "kicking off expired reads (expired delta %ld, %ld)\n",
							   elapsed->tv_sec, elapsed->tv_usec);

	__CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	array = __sCFFileDescriptorManager.mReadFileDescriptors;
	count = CFArrayGetCount(array);

	for (index = 0; index < count; index++) {
		CFFileDescriptorRef f = (CFFileDescriptorRef)CFArrayGetValueAtIndex(array, index);
#if 0
		if (timerisset(&f->_readBufferTimeout) || f->_leftoverBytes) {
			CFFileDescriptorNativeDescriptor fd = f->_descriptor;
			fd_set *   tempfds = NULL;
			// We might have an new element in __sCFFileDescriptorManager.mReadFileDescriptors that we
			// weren't listening to, in which case we must be sure not
			// to test a bit in the fdset that is outside our mask
			// size.
			const Boolean fdInBounds = (0 <= fd && fd < max);
			/* if this sockets timeout is less than or equal elapsed time, then signal it */
			if (__CFFILEDESCRIPTOR_INVALID_DESCRIPTOR != fd && fdInBounds) {
				__CFFileDescriptorMaybeLog("Expiring descriptor %d (delta %d, %d)\n",
										   fd, f->_readBufferTimeout.tv_sec, f->_readBufferTimeout.tv_usec);

				CFArraySetValueAtIndex(state->_selected._read._descriptors, state->_selected._read._index, f);

				state->_selected._read._index++;
				/* descriptor is removed from fds here, will be restored in read handling or in perform function */
				if (!tempfds) tempfds = (fd_set *)CFDataGetMutableBytePtr(__sCFFileDescriptorManager.mReadFileDescriptorNativeDescriptors);
				FD_CLR(fd, tempfds);
			}
		}
#else
		(void)f;
#endif
	}

	__CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);
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
        status = fcntl(__sCFFileDescriptorManager.mWakeupNativeDescriptorPipe[__kWakeupPipeWriterIndex],
                       F_SETFL,
                       O_NONBLOCK);
        __Require(status == 0, done);

        status = fcntl(__sCFFileDescriptorManager.mWakeupNativeDescriptorPipe[__kWakeupPipeReaderIndex],
                       F_SETFL,
                       O_NONBLOCK);
        __Require(status == 0, done);

        __CFFileDescriptorFdSet(__sCFFileDescriptorManager.mWakeupNativeDescriptorPipe[__kWakeupPipeReaderIndex],
                                __sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors);
    }

 done:
    __CFFileDescriptorExit();

    return;
}

/* static */ Boolean
__CFFileDescriptorManagerMaybeAdd_Locked(CFFileDescriptorRef f,
										 Boolean forRead,
										 Boolean forWrite,
										 Boolean force)
{
	Boolean result = FALSE;

    __CFFileDescriptorEnter();

	__CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	if (forWrite) {
		__CFFileDescriptorMaybeLog("Adding descriptor %d for writing.\n", f->_descriptor);

		if (force) {
			CFMutableArrayRef array = __sCFFileDescriptorManager.mWriteFileDescriptors;
			const CFIndex     index = CFArrayGetFirstIndexOfValue(array,
																  CFRangeMake(0, CFArrayGetCount(array)),
																  f);

			if (index == kCFNotFound) {
				CFArrayAppendValue(array, f);
			}
		}

		if (__CFFileDescriptorManagerSetFDForWrite_Locked(f)) {
			result = TRUE;
		}
	}

	if (forRead) {
		__CFFileDescriptorMaybeLog("Adding descriptor %d for reading.\n", f->_descriptor);

		if (force) {
			CFMutableArrayRef array = __sCFFileDescriptorManager.mReadFileDescriptors;
			const CFIndex     index = CFArrayGetFirstIndexOfValue(array,
																  CFRangeMake(0, CFArrayGetCount(array)),
																  f);

			if (index == kCFNotFound) {
				CFArrayAppendValue(array, f);
			}
		}

		if (__CFFileDescriptorManagerSetFDForRead_Locked(f)) {
			result = TRUE;
		}
	}

	if (result && __sCFFileDescriptorManager.mThread == NULL) {
		__CFFileDescriptorMaybeLog("Starting manager thread...\n");
		__sCFFileDescriptorManager.mThread = __CFStartSimpleThread((void*)__CFFileDescriptorManager, 0);
	}

	__CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

    __CFFileDescriptorExit();

	return result;
}

/* static */ void
__CFFileDescriptorManagerPrepareWatchesMaybeLog(void)
{
#if LOG_CFFILEDESCRIPTOR
	CFArrayRef array;
	CFDataRef  data;

    __CFFileDescriptorMaybeLog("file descriptor manager iteration %u "
							   "looking at read descriptors ",
							   __sCFFileDescriptorManager.mGeneration);

	array = __sCFFileDescriptorManager.mReadFileDescriptors;
	data  = __sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors;

	__CFFileDescriptorMaybeLogFileDescriptorList(array, data, FALSE);

	array = __sCFFileDescriptorManager.mWriteFileDescriptors;
	data  = __sCFFileDescriptorManager.mWriteFileDescriptorsNativeDescriptors;

	if (CFArrayGetCount(array) > 0) {
		__CFFileDescriptorMaybeLog(", and write descriptors");
		__CFFileDescriptorMaybeLogFileDescriptorList(array, data, FALSE);

#if DEPLOYMENT_TARGET_WINDOWS
		array = __sCFFileDescriptorManager.mWriteFileDescriptors;
		data  = __sCFFileDescriptorManager.mExceptFileDescriptorsNativeDescriptors;

		__CFFileDescriptorMaybeLog(", and except descriptors");
		__CFFileDescriptorMaybeLogFileDescriptorList(array, data, TRUE);
#endif /* DEPLOYMENT_TARGET_WINDOWS */
	}

	__CFFileDescriptorMaybeLog("\n");
#endif /* LOG_CFFILEDESCRIPTOR */
}

/* static */ CFIndex
__CFFileDescriptorManagerPrepareWatches(struct __CFFileDescriptorManagerWatchedDescriptors * watches, struct timeval *timeout) {
    CFIndex fdentries = 1;
    CFIndex rfds, wfds;
    struct timeval tv;
	CFIndex maxnrfds = 0;

    __CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	__sCFFileDescriptorManager.mGeneration++;

	__CFFileDescriptorManagerPrepareWatchesMaybeLog();

	rfds     = __CFFileDescriptorFdGetSize(__sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors);
	wfds     = __CFFileDescriptorFdGetSize(__sCFFileDescriptorManager.mWriteFileDescriptorsNativeDescriptors);
	maxnrfds = __CFMax(rfds, wfds);

#if !DEPLOYMENT_TARGET_WINDOWS
	if (maxnrfds > fdentries * (int)NFDBITS) {
		fdentries = (maxnrfds + NFDBITS - 1) / NFDBITS;
		watches->_write = (fd_set *)CFAllocatorReallocate(kCFAllocatorSystemDefault, watches->_write, fdentries * sizeof(fd_mask), 0);
		watches->_read  = (fd_set *)CFAllocatorReallocate(kCFAllocatorSystemDefault, watches->_read, fdentries * sizeof(fd_mask), 0);
	}

	memset(watches->_write, 0, fdentries * sizeof(fd_mask));
	memset(watches->_read,  0, fdentries * sizeof(fd_mask));
#else
	if (maxnrfds > fdentries) {
		fdentries = maxnrfds;
		watches->_except = (fd_set *)CFAllocatorReallocate(kCFAllocatorSystemDefault, watches->_except, fdentries * sizeof(HANDLE) + sizeof(u_int), 0);
		watches->_write  = (fd_set *)CFAllocatorReallocate(kCFAllocatorSystemDefault, watches->_write, fdentries * sizeof(HANDLE) + sizeof(u_int), 0);
		watches->_read   = (fd_set *)CFAllocatorReallocate(kCFAllocatorSystemDefault, watches->_read, fdentries * sizeof(HANDLE) + sizeof(u_int), 0);
	}

	memset(watches->_except, 0, fdentries * sizeof(HANDLE) + sizeof(u_int));
	memset(watches->_write,  0, fdentries * sizeof(HANDLE) + sizeof(u_int));
	memset(watches->_read,   0, fdentries * sizeof(HANDLE) + sizeof(u_int));

	CFDataGetBytes(__sCFFileDescriptorManager.mExceptFileDescriptorsNativeDescriptors,
				   CFRangeMake(0, __CFFileDescriptorFdGetSize(__sCFFileDescriptorManager.mExceptFileDescriptorsNativeDescriptors) * sizeof(HANDLE) + sizeof(u_int)),
				   (UInt8 *)watches->_except);
#endif /* !DEPLOYMENT_TARGET_WINDOWS */

	CFDataGetBytes(__sCFFileDescriptorManager.mWriteFileDescriptorsNativeDescriptors,
				   CFRangeMake(0, CFDataGetLength(__sCFFileDescriptorManager.mWriteFileDescriptorsNativeDescriptors)),
				   (UInt8 *)watches->_write);
	CFDataGetBytes(__sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors,
				   CFRangeMake(0, CFDataGetLength(__sCFFileDescriptorManager.mReadFileDescriptorsNativeDescriptors)),
				   (UInt8 *)watches->_read);

	if (__sCFFileDescriptorManager.mReadFileDescriptorsTimeoutInvalid) {
		struct timeval* minTimeout = NULL;

		__sCFFileDescriptorManager.mReadFileDescriptorsTimeoutInvalid = false;
		__CFFileDescriptorMaybeLog("Figuring out which file descriptors have timeouts...\n");
		CFArrayApplyFunction(__sCFFileDescriptorManager.mReadFileDescriptors,
							 CFRangeMake(0, CFArrayGetCount(__sCFFileDescriptorManager.mReadFileDescriptors)),
							 __CFFileDescriptorCalculateMinTimeout_Locked,
							 (void *)&minTimeout);

		if (minTimeout == NULL) {
			__CFFileDescriptorMaybeLog("No one wants a timeout!\n");

			timeout = NULL;
		} else {
			__CFFileDescriptorMaybeLog("timeout will be %ld, %ld!\n",
									   minTimeout->tv_sec,
									   minTimeout->tv_usec);

			tv = *minTimeout;
			*timeout = tv;
		}
	}

	if (timeout) {
		__CFFileDescriptorMaybeLog("select will have a %ld, %ld timeout\n",
								   timeout->tv_sec,
								   timeout->tv_usec);
	}

    __CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	return maxnrfds;
}

/* static */ void
__CFFileDescriptorManagerProcessState(struct __CFFileDescriptorManagerSelectState * state, CFIndex count, CFIndex max, const struct timeval *elapsed) {
	const Boolean kDidTimeout = (count == 0);
	const Boolean kHadError   = (count < 0);

	if (kDidTimeout) {
		__CFFileDescriptorManagerHandleTimeout(state, elapsed);

	} else if (kHadError) {
		__CFFileDescriptorManagerHandleError();

	}

	if (FD_ISSET(__sCFFileDescriptorManager.mWakeupNativeDescriptorPipe[__kWakeupPipeReaderIndex], state->_watches._read)) {
		uint8_t       buffer[256];
		int           status;

		status = read(__sCFFileDescriptorManager.mWakeupNativeDescriptorPipe[__kWakeupPipeReaderIndex], buffer, sizeof(buffer));
		__Verify(status == sizeof(char));

		__CFFileDescriptorMaybeLog("file descriptor manager received reason '%c' on wakeup pipe\n",
								   buffer[0]);
	}

	__CFFileDescriptorManagerHandleReadyDescriptors(state, max, kDidTimeout);
}

/* static */ void
__CFFileDescriptorManagerRemoveInvalidFileDescriptors(void) {
	CFMutableArrayRef invalidFileDescriptors = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
	CFIndex count;
	CFIndex index;

	__CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	__CFFileDescriptorFindAndAppendInvalidDescriptors(__sCFFileDescriptorManager.mWriteFileDescriptors,
													  invalidFileDescriptors,
													  "write");

	__CFFileDescriptorFindAndAppendInvalidDescriptors(__sCFFileDescriptorManager.mReadFileDescriptors,
													  invalidFileDescriptors,
													  "read");

	__CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

	count = CFArrayGetCount(invalidFileDescriptors);
	for (index = 0; index < count; index++) {
		CFFileDescriptorInvalidate(((CFFileDescriptorRef)CFArrayGetValueAtIndex(invalidFileDescriptors, index)));
	}
	CFRelease(invalidFileDescriptors);
}

/* static */ void
__CFFileDescriptorManagerRemove_Locked(CFFileDescriptorRef f) {
    CFMutableArrayRef array;
    CFIndex index;

    __CFFileDescriptorEnter();

    __CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

    // Handle removing the descriptor from the active write descriptor collection.

    array = __sCFFileDescriptorManager.mWriteFileDescriptors;

    index = CFArrayGetFirstIndexOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), f);
    if (index >= 0) {
        CFArrayRemoveValueAtIndex(array, index);
        __CFFileDescriptorManagerClearFDForWrite_Locked(f);
#if DEPLOYMENT_TARGET_WINDOWS
        __CFFileDescriptorFdClr(f->_descriptor, __sCFFileDescriptorManager.mExceptFileDescriptorsNativeDescriptors);
#endif
    }

    // Handle removing the descriptor from the active read descriptor collection.

    array = __sCFFileDescriptorManager.mReadFileDescriptors;

    index = CFArrayGetFirstIndexOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), f);
    if (index >= 0) {
        CFArrayRemoveValueAtIndex(array, index);
        __CFFileDescriptorManagerClearFDForRead_Locked(f);
    }

    __CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

    __CFFileDescriptorExit();
}

/* static */ Boolean
__CFFileDescriptorManagerShouldWake_Locked(CFFileDescriptorRef f,
                                           CFOptionFlags callBackTypes) {
    Boolean result = FALSE;

    __CFSpinLock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

    if ((callBackTypes & kCFFileDescriptorWriteCallBack) != 0) {
        if (__CFFileDescriptorManagerClearFDForWrite_Locked(f)) {
            const CFOptionFlags writeCallBacksAvailable = callBackTypes & kCFFileDescriptorWriteCallBack;

                // do not wake up the file descriptor manager thread
                // if all relevant write callbacks are disabled

                if ((f->_flags.disabled & writeCallBacksAvailable) != writeCallBacksAvailable) {
                    result = true;
                }
            }
        }

        if ((callBackTypes & kCFFileDescriptorReadCallBack) != 0) {
            if (__CFFileDescriptorManagerClearFDForRead_Locked(f)) {
                // do not wake up the file descriptor manager thread
                // if callback type is read

                if ((callBackTypes & kCFFileDescriptorReadCallBack) != kCFFileDescriptorReadCallBack) {
                    result = true;
                }
            }
        }

        __CFSpinUnlock(&__sCFFileDescriptorManager.mActiveFileDescriptorsLock);

    return result;
}

/* static */ void
__CFFileDescriptorManagerWakeup(char reason)
{
    int status;

    __CFFileDescriptorEnter();

    __CFFileDescriptorMaybeLog("Waking up the file descriptor manager w/ reason '%c'\n", reason);

    status = write(__sCFFileDescriptorManager.mWakeupNativeDescriptorPipe[__kWakeupPipeWriterIndex],
                   &reason,
                   sizeof(reason));
    __Verify(status == sizeof(reason));

    __CFFileDescriptorExit();
}

#if LOG_CFFILEDESCRIPTOR
/* static */ void
__CFFileDescriptorMaybeLogFileDescriptorList(CFArrayRef descriptors, CFDataRef fdSet, Boolean onlyIfSet) {
    const fd_set * const tempfds = (const fd_set *)CFDataGetBytePtr(fdSet);
    CFIndex index, count;
    for (index = 0, count = CFArrayGetCount(descriptors); index < count; index++) {
        CFFileDescriptorRef f = (CFFileDescriptorRef)CFArrayGetValueAtIndex(descriptors, index);
        if (FD_ISSET(f->_descriptor, tempfds)) {
            __CFFileDescriptorMaybeLog("%d ", f->_descriptor);
        } else if (!onlyIfSet) {
            __CFFileDescriptorMaybeLog("(%d) ", f->_descriptor);
        }
    }
}
#endif /* LOG_CFFILEDESCRIPTOR */

/* static */ const char *
__CFFileDescriptorNameForSymbol(void *address) {
    static const char * const kUnknownName = "???";
	const char *result;

#if DEPLOYMENT_TARGET_WINDOWS
#warning "Windows portability issue!"
    // FIXME:  Get name using win32 analog of dladdr?
    result = kUnknownName;
#else
    Dl_info info;
    result = (dladdr(address, &info) && info.dli_saddr == address && info.dli_sname) ? info.dli_sname : kUnknownName;
#endif

	return result;
}

/* static */ void *
__CFFileDescriptorReallocateWatchedDescriptors(struct __CFFileDescriptorManagerWatchedDescriptors *watches, CFIndex count)
{
	void *result = NULL;

	return result;
}

/* static */ Boolean
__CFNativeFileDescriptorIsValid(CFFileDescriptorNativeDescriptor fd) {
#if DEPLOYMENT_TARGET_WINDOWS
    SInt32 flags = ioctlsocket (fd, FIONREAD, 0);
    return (0 == flags);
#else
    SInt32 flags = fcntl(fd, F_GETFL, 0);
    return !(0 > flags && EBADF == thread_errno());
#endif
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
	static const char * const                          kClassName = "CFFileDescriptor";
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
	name = __CFFileDescriptorNameForSymbol(addr);

    CFStringAppendFormat(result,
                         NULL,
                         CFSTR("<%s %p [%p]>{valid = %s, fd = %d source = %p, callout = %s (%p) context = "),
						 kClassName,
                         cf,
                         CFGetAllocator(f),
                         (__CFFileDescriptorIsValid(f) ? "Yes" : "No"),
                         f->_descriptor,
                         f->_rlsource,
                         name,
                         addr);

    contextInfo = f->_context.info;
    contextCopyDescriptionCallBack = f->_context.copyDescription;

    __CFFileDescriptorUnlock(f);

    if (contextInfo != NULL && contextCopyDescriptionCallBack != NULL) {
        contextDesc = contextCopyDescriptionCallBack(contextInfo);
    }
    if (NULL == contextDesc) {
        contextDesc = CFStringCreateWithFormat(CFGetAllocator(f), NULL, CFSTR("<%s context %p>"), kClassName, contextInfo);
    }

    CFStringAppend(result, contextDesc);
    CFStringAppend(result, CFSTR("}"));

    CFRelease(contextDesc);

 done:
    return result;
}

// MARK: CFRunLoopSource Functions

/* static */ void
__CFFileDescriptorRunLoopSchedule(void *info, CFRunLoopRef rl, CFStringRef mode) {
    CFFileDescriptorRef f = (CFFileDescriptorRef)(info);
    Boolean             valid;

    __CFFileDescriptorEnter();

    __CFFileDescriptorLock(f);

    valid = __CFFileDescriptorIsValid(f);

    if (valid) {
        CFArrayAppendValue(f->_rloops, rl);

        f->_fileDescriptorSetCount++;

        if (f->_fileDescriptorSetCount == 1) {
            __CFFileDescriptorMaybeLog("scheduling descriptor %d\n", f->_descriptor);

            __CFFileDescriptorEnableCallBacks_LockedAndUnlock(f, __CFFileDescriptorCallBackTypes(f), TRUE, __kWakeupReasonSchedule);
        } else {
            __CFFileDescriptorUnlock(f);
        }
    } else {
        __CFFileDescriptorUnlock(f);
    }

    __CFFileDescriptorExit();
}

/* static */ void
__CFFileDescriptorRunLoopCancel(void *info, CFRunLoopRef rl, CFStringRef mode) {
    CFFileDescriptorRef f = (CFFileDescriptorRef)(info);
    CFIndex             index;

    __CFFileDescriptorEnter();

    __CFFileDescriptorLock(f);

    f->_fileDescriptorSetCount--;
    if (f->_fileDescriptorSetCount == 0) {
        __CFFileDescriptorManagerRemove_Locked(f);
    }

    if (f->_rloops != NULL) {
        index = CFArrayGetFirstIndexOfValue(f->_rloops, CFRangeMake(0, CFArrayGetCount(f->_rloops)), rl);
        if (0 <= index) CFArrayRemoveValueAtIndex(f->_rloops, index);
    }

    __CFFileDescriptorUnlock(f);

    __CFFileDescriptorExit();
}

/* static */ void
__CFFileDescriptorRunLoopPerform(void *info) {
    CFFileDescriptorRef f = (CFFileDescriptorRef)(info);
    CFOptionFlags       callBacksSignaled = 0;
    CFRunLoopRef        rl = NULL;

    __CFFileDescriptorEnter();

    __CFFileDescriptorLock(f);

    if (!__CFFileDescriptorIsValid(f)) {
        __CFFileDescriptorUnlock(f);
        __CFFileDescriptorExit();

        return;
    }

    if (__CFFileDescriptorIsReadSignaled(f)) {
        callBacksSignaled |= kCFFileDescriptorReadCallBack;
    }

    if (__CFFileDescriptorIsWriteSignaled(f)) {
        callBacksSignaled |= kCFFileDescriptorWriteCallBack;
    }

    __CFFileDescriptorDoCallback_LockedAndUnlock(f);

    __CFFileDescriptorLock(f);

    __CFFileDescriptorEnableCallBacks_LockedAndUnlock(f,
                                                      callBacksSignaled & f->_flags.enabled,
                                                      FALSE,
                                                      __kWakeupReasonPerform);

    if (rl != NULL) {
        CFRunLoopWakeUp(rl);
        CFRelease(rl);
    }

    __CFFileDescriptorExit();
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

    __CFFileDescriptorLock(f);

    __CFFileDescriptorEnableCallBacks_LockedAndUnlock(f, callBackTypes, TRUE, __kWakeupReasonEnable);

    __CFFileDescriptorExit();
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
    Boolean wakeup = FALSE;

    __CFFileDescriptorEnter();

    __Require(callBackTypes != 0, done);

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    __CFFileDescriptorLock(f);

    wakeup = __CFFileDescriptorDisableCallBacks_Locked(f, callBackTypes);

    __CFFileDescriptorUnlock(f);

    if (wakeup && __sCFFileDescriptorManager.mThread != NULL) {
        __CFFileDescriptorManagerWakeup(__kWakeupReasonDisable);
    }

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

    __CFFileDescriptorMaybeLog("invalidating file descriptor %d\n", f->_descriptor);

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

/**
 *  @brief
 *    Creates a new runloop source for a given CFFileDescriptor.
 *
 *  The context for the new runloop is the same as the context passed
 *  in when the CFFileDescriptor was created (see
 *  #CFFileDescriptorCreate).
 *
 *  @param[in]  allocator  The allocator to use to allocate memory for
 *                         the new runloop. Pass NULL or
 *                         kCFAllocatorDefault to use the current
 *                         default allocator.
 *  @param[in]  f          A CFFileDescriptor.
 *  @param[in]  order      The order for the new run loop.
 *
 *  @returns
 *    A new runloop source for @ f, or NULL if there was a problem
 *    creating the object. Ownership follows the "The Create Rule".
 *
 */
CFRunLoopSourceRef
CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator, CFFileDescriptorRef f, CFIndex order) {
    CHECK_FOR_FORK();
    Boolean            valid;
    CFRunLoopSourceRef result = NULL;

    __CFFileDescriptorEnter();

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    __CFFileDescriptorLock(f);

    valid = __CFFileDescriptorIsValid(f);
    __Require(valid, unlock);

    // If this descriptor does not have a runloop source, create and
    // attach one. Otherwise, we will just use and return the one
    // already attached with the retain count concommitantly
    // increased.

    if (f->_rlsource == NULL) {
        CFRunLoopSourceContext context;

        context.version         = 0;
        context.info            = f;
        context.retain          = CFRetain;
        context.release         = CFRelease;
        context.copyDescription = CFCopyDescription;
        context.equal           = CFEqual;
        context.hash            = CFHash;
        context.schedule        = __CFFileDescriptorRunLoopSchedule;
        context.cancel          = __CFFileDescriptorRunLoopCancel;
        context.perform         = __CFFileDescriptorRunLoopPerform;

        f->_rlsource = CFRunLoopSourceCreate(allocator, order, &context);
    }

    // The following retain is for the receiver (caller) which is
    // bound to observe "The Create Rule" for runloop object
    // ownership.

    CFRetain(f->_rlsource);

    result = f->_rlsource;

 unlock:
    __CFFileDescriptorUnlock(f);

    __CFFileDescriptorExit();

    return result;
}
