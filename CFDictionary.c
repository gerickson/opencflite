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

/*	CFDictionary.c
	Copyright (c) 1998-2011, Apple Inc. All rights reserved.
	Responsibility: Christopher Kane
	Machine generated from Notes/HashingCode.template
*/





#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CoreFoundation_Prefix.h>
#include "CFInternal.h"
#include "CFBasicHash.h"
#include <CoreFoundation/CFString.h>

#define CFDictionary 0
#define CFSet 0
#define CFBag 0
#undef CFDictionary
#define CFDictionary 1

#if CFDictionary
const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
const CFDictionaryKeyCallBacks kCFCopyStringDictionaryKeyCallBacks = {0, __CFStringCollectionCopy, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual};
__private_extern__ const CFDictionaryValueCallBacks kCFTypeDictionaryValueCompactableCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual};
static const CFDictionaryKeyCallBacks __kCFNullDictionaryKeyCallBacks = {0, NULL, NULL, NULL, NULL, NULL};
static const CFDictionaryValueCallBacks __kCFNullDictionaryValueCallBacks = {0, NULL, NULL, NULL, NULL};

#define CFHashRef CFDictionaryRef
#define CFMutableHashRef CFMutableDictionaryRef
#define CFHashKeyCallBacks CFDictionaryKeyCallBacks
#define CFHashValueCallBacks CFDictionaryValueCallBacks
#endif

#if CFSet
const CFDictionaryCallBacks kCFTypeDictionaryCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
const CFDictionaryCallBacks kCFCopyStringDictionaryCallBacks = {0, __CFStringCollectionCopy, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
static const CFDictionaryCallBacks __kCFNullDictionaryCallBacks = {0, NULL, NULL, NULL, NULL, NULL};

#define CFDictionaryKeyCallBacks CFDictionaryCallBacks
#define CFDictionaryValueCallBacks CFDictionaryCallBacks
#define kCFTypeDictionaryKeyCallBacks kCFTypeDictionaryCallBacks
#define kCFTypeDictionaryValueCallBacks kCFTypeDictionaryCallBacks
#define __kCFNullDictionaryKeyCallBacks __kCFNullDictionaryCallBacks
#define __kCFNullDictionaryValueCallBacks __kCFNullDictionaryCallBacks

#define CFHashRef CFSetRef
#define CFMutableHashRef CFMutableSetRef
#define CFHashKeyCallBacks CFDictionaryCallBacks
#define CFHashValueCallBacks CFDictionaryCallBacks
#endif

#if CFBag
const CFDictionaryCallBacks kCFTypeDictionaryCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
const CFDictionaryCallBacks kCFCopyStringDictionaryCallBacks = {0, __CFStringCollectionCopy, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
static const CFDictionaryCallBacks __kCFNullDictionaryCallBacks = {0, NULL, NULL, NULL, NULL, NULL};

#define CFDictionaryKeyCallBacks CFDictionaryCallBacks
#define CFDictionaryValueCallBacks CFDictionaryCallBacks
#define kCFTypeDictionaryKeyCallBacks kCFTypeDictionaryCallBacks
#define kCFTypeDictionaryValueCallBacks kCFTypeDictionaryCallBacks
#define __kCFNullDictionaryKeyCallBacks __kCFNullDictionaryCallBacks
#define __kCFNullDictionaryValueCallBacks __kCFNullDictionaryCallBacks

#define CFHashRef CFBagRef
#define CFMutableHashRef CFMutableBagRef
#define CFHashKeyCallBacks CFDictionaryCallBacks
#define CFHashValueCallBacks CFDictionaryCallBacks
#endif


typedef uintptr_t any_t;
typedef const void * const_any_pointer_t;
typedef void * any_pointer_t;

static Boolean __CFDictionaryEqual(CFTypeRef cf1, CFTypeRef cf2) {
    return __CFBasicHashEqual((CFBasicHashRef)cf1, (CFBasicHashRef)cf2);
}

static CFHashCode __CFDictionaryHash(CFTypeRef cf) {
    return __CFBasicHashHash((CFBasicHashRef)cf);
}

static CFStringRef __CFDictionaryCopyDescription(CFTypeRef cf) {
    return __CFBasicHashCopyDescription((CFBasicHashRef)cf);
}

static void __CFDictionaryDeallocate(CFTypeRef cf) {
    __CFBasicHashDeallocate((CFBasicHashRef)cf);
}

static CFTypeID __kCFDictionaryTypeID = _kCFRuntimeNotATypeID;

static const CFRuntimeClass __CFDictionaryClass = {
    _kCFRuntimeScannedObject,
    "CFDictionary",
    NULL,        // init
    NULL,        // copy
    __CFDictionaryDeallocate,
    __CFDictionaryEqual,
    __CFDictionaryHash,
    NULL,        //
    __CFDictionaryCopyDescription
};

CFTypeID CFDictionaryGetTypeID(void) {
    if (_kCFRuntimeNotATypeID == __kCFDictionaryTypeID) __kCFDictionaryTypeID = _CFRuntimeRegisterClass(&__CFDictionaryClass);
    return __kCFDictionaryTypeID;
}

#define GCRETAIN(A, B) kCFTypeSetCallBacks.retain(A, B)
#define GCRELEASE(A, B) kCFTypeSetCallBacks.release(A, B)

static uintptr_t __CFDictionaryStandardRetainValue(CFConstBasicHashRef ht, uintptr_t stack_value) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0100) return stack_value;
    return (CFBasicHashHasStrongValues(ht)) ? (uintptr_t)GCRETAIN(kCFAllocatorSystemDefault, (CFTypeRef)stack_value) : (uintptr_t)CFRetain((CFTypeRef)stack_value);
}

static uintptr_t __CFDictionaryStandardRetainKey(CFConstBasicHashRef ht, uintptr_t stack_key) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0001) return stack_key;
    return (CFBasicHashHasStrongKeys(ht)) ? (uintptr_t)GCRETAIN(kCFAllocatorSystemDefault, (CFTypeRef)stack_key) : (uintptr_t)CFRetain((CFTypeRef)stack_key);
}

static void __CFDictionaryStandardReleaseValue(CFConstBasicHashRef ht, uintptr_t stack_value) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0200) return;
    if (CFBasicHashHasStrongValues(ht)) GCRELEASE(kCFAllocatorSystemDefault, (CFTypeRef)stack_value); else CFRelease((CFTypeRef)stack_value);
}

static void __CFDictionaryStandardReleaseKey(CFConstBasicHashRef ht, uintptr_t stack_key) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0002) return;
    if (CFBasicHashHasStrongKeys(ht)) GCRELEASE(kCFAllocatorSystemDefault, (CFTypeRef)stack_key); else CFRelease((CFTypeRef)stack_key);
}

static Boolean __CFDictionaryStandardEquateValues(CFConstBasicHashRef ht, uintptr_t coll_value1, uintptr_t stack_value2) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0400) return coll_value1 == stack_value2;
    return CFEqual((CFTypeRef)coll_value1, (CFTypeRef)stack_value2);
}

static Boolean __CFDictionaryStandardEquateKeys(CFConstBasicHashRef ht, uintptr_t coll_key1, uintptr_t stack_key2) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0004) return coll_key1 == stack_key2;
    return CFEqual((CFTypeRef)coll_key1, (CFTypeRef)stack_key2);
}

static uintptr_t __CFDictionaryStandardHashKey(CFConstBasicHashRef ht, uintptr_t stack_key) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0008) return stack_key;
    return (uintptr_t)CFHash((CFTypeRef)stack_key);
}

static uintptr_t __CFDictionaryStandardGetIndirectKey(CFConstBasicHashRef ht, uintptr_t coll_value) {
    return 0;
}

static CFStringRef __CFDictionaryStandardCopyValueDescription(CFConstBasicHashRef ht, uintptr_t stack_value) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0800) return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<%p>"), (void *)stack_value);
    return CFCopyDescription((CFTypeRef)stack_value);
}

static CFStringRef __CFDictionaryStandardCopyKeyDescription(CFConstBasicHashRef ht, uintptr_t stack_key) {
    if (CFBasicHashGetSpecialBits(ht) & 0x0010) return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<%p>"), (void *)stack_key);
    return CFCopyDescription((CFTypeRef)stack_key);
}

static CFBasicHashCallbacks *__CFDictionaryStandardCopyCallbacks(CFConstBasicHashRef ht, CFAllocatorRef allocator, CFBasicHashCallbacks *cb);
static void __CFDictionaryStandardFreeCallbacks(CFConstBasicHashRef ht, CFAllocatorRef allocator, CFBasicHashCallbacks *cb);

static const CFBasicHashCallbacks CFDictionaryStandardCallbacks = {
    __CFDictionaryStandardCopyCallbacks,
    __CFDictionaryStandardFreeCallbacks,
    __CFDictionaryStandardRetainValue,
    __CFDictionaryStandardRetainKey,
    __CFDictionaryStandardReleaseValue,
    __CFDictionaryStandardReleaseKey,
    __CFDictionaryStandardEquateValues,
    __CFDictionaryStandardEquateKeys,
    __CFDictionaryStandardHashKey,
    __CFDictionaryStandardGetIndirectKey,
    __CFDictionaryStandardCopyValueDescription,
    __CFDictionaryStandardCopyKeyDescription
};

static CFBasicHashCallbacks *__CFDictionaryStandardCopyCallbacks(CFConstBasicHashRef ht, CFAllocatorRef allocator, CFBasicHashCallbacks *cb) {
    return (CFBasicHashCallbacks *)&CFDictionaryStandardCallbacks;
}

static void __CFDictionaryStandardFreeCallbacks(CFConstBasicHashRef ht, CFAllocatorRef allocator, CFBasicHashCallbacks *cb) {
}
    

static CFBasicHashCallbacks *__CFDictionaryCopyCallbacks(CFConstBasicHashRef ht, CFAllocatorRef allocator, CFBasicHashCallbacks *cb) {
    CFBasicHashCallbacks *newcb = NULL;
    if (CF_IS_COLLECTABLE_ALLOCATOR(allocator)) {
        newcb = (CFBasicHashCallbacks *)auto_zone_allocate_object(objc_collectableZone(), sizeof(CFBasicHashCallbacks) + 10 * sizeof(void *), AUTO_MEMORY_UNSCANNED, false, false);
    } else {
        newcb = (CFBasicHashCallbacks *)CFAllocatorAllocate(allocator, sizeof(CFBasicHashCallbacks) + 10 * sizeof(void *), 0);
    }
    if (!newcb) HALT;
    memmove(newcb, (void *)cb, sizeof(CFBasicHashCallbacks) + 10 * sizeof(void *));
    return newcb;
}

static void __CFDictionaryFreeCallbacks(CFConstBasicHashRef ht, CFAllocatorRef allocator, CFBasicHashCallbacks *cb) {
    if (CF_IS_COLLECTABLE_ALLOCATOR(allocator)) {
    } else {
       CFAllocatorDeallocate(allocator, cb);
    }
}
    
static uintptr_t __CFDictionaryRetainValue(CFConstBasicHashRef ht, uintptr_t stack_value) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    const_any_pointer_t (*value_retain)(CFAllocatorRef, const_any_pointer_t) = (const_any_pointer_t (*)(CFAllocatorRef, const_any_pointer_t))cb->context[0];
    if (NULL == value_retain) return stack_value;
    return (uintptr_t)INVOKE_CALLBACK2(value_retain, CFGetAllocator(ht), (const_any_pointer_t)stack_value);
}

static uintptr_t __CFDictionaryRetainKey(CFConstBasicHashRef ht, uintptr_t stack_key) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    const_any_pointer_t (*key_retain)(CFAllocatorRef, const_any_pointer_t) = (const_any_pointer_t (*)(CFAllocatorRef, const_any_pointer_t))cb->context[1];
    if (NULL == key_retain) return stack_key;
    return (uintptr_t)INVOKE_CALLBACK2(key_retain, CFGetAllocator(ht), (const_any_pointer_t)stack_key);
}

static void __CFDictionaryReleaseValue(CFConstBasicHashRef ht, uintptr_t stack_value) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    void (*value_release)(CFAllocatorRef, const_any_pointer_t) = (void (*)(CFAllocatorRef, const_any_pointer_t))cb->context[2];
    if (NULL != value_release) INVOKE_CALLBACK2(value_release, CFGetAllocator(ht), (const_any_pointer_t) stack_value);
}

static void __CFDictionaryReleaseKey(CFConstBasicHashRef ht, uintptr_t stack_key) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    void (*key_release)(CFAllocatorRef, const_any_pointer_t) = (void (*)(CFAllocatorRef, const_any_pointer_t))cb->context[3];
    if (NULL != key_release) INVOKE_CALLBACK2(key_release, CFGetAllocator(ht), (const_any_pointer_t) stack_key);
}

static Boolean __CFDictionaryEquateValues(CFConstBasicHashRef ht, uintptr_t coll_value1, uintptr_t stack_value2) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    Boolean (*value_equal)(const_any_pointer_t, const_any_pointer_t) = (Boolean (*)(const_any_pointer_t, const_any_pointer_t))cb->context[4];
    if (NULL == value_equal) return (coll_value1 == stack_value2);
    return INVOKE_CALLBACK2(value_equal, (const_any_pointer_t) coll_value1, (const_any_pointer_t) stack_value2) ? 1 : 0;
}

static Boolean __CFDictionaryEquateKeys(CFConstBasicHashRef ht, uintptr_t coll_key1, uintptr_t stack_key2) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    Boolean (*key_equal)(const_any_pointer_t, const_any_pointer_t) = (Boolean (*)(const_any_pointer_t, const_any_pointer_t))cb->context[5];
    if (NULL == key_equal) return (coll_key1 == stack_key2);
    return INVOKE_CALLBACK2(key_equal, (const_any_pointer_t) coll_key1, (const_any_pointer_t) stack_key2) ? 1 : 0;
}

static uintptr_t __CFDictionaryHashKey(CFConstBasicHashRef ht, uintptr_t stack_key) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    CFHashCode (*hash)(const_any_pointer_t) = (CFHashCode (*)(const_any_pointer_t))cb->context[6];
    if (NULL == hash) return stack_key;
    return (uintptr_t)INVOKE_CALLBACK1(hash, (const_any_pointer_t) stack_key);
}

static uintptr_t __CFDictionaryGetIndirectKey(CFConstBasicHashRef ht, uintptr_t coll_value) {
    return 0;
}

static CFStringRef __CFDictionaryCopyValueDescription(CFConstBasicHashRef ht, uintptr_t stack_value) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    CFStringRef (*value_describe)(const_any_pointer_t) = (CFStringRef (*)(const_any_pointer_t))cb->context[8];
    if (NULL == value_describe) return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<%p>"), (const_any_pointer_t) stack_value);
    return (CFStringRef)INVOKE_CALLBACK1(value_describe, (const_any_pointer_t) stack_value);
}

static CFStringRef __CFDictionaryCopyKeyDescription(CFConstBasicHashRef ht, uintptr_t stack_key) {
    const CFBasicHashCallbacks *cb = CFBasicHashGetCallbacks(ht);
    CFStringRef (*key_describe)(const_any_pointer_t) = (CFStringRef (*)(const_any_pointer_t))cb->context[9];
    if (NULL == key_describe) return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<%p>"), (const_any_pointer_t) stack_key);
    return (CFStringRef)INVOKE_CALLBACK1(key_describe, (const_any_pointer_t) stack_key);
}

static CFBasicHashRef __CFDictionaryCreateGeneric(CFAllocatorRef allocator, const CFHashKeyCallBacks *keyCallBacks, const CFHashValueCallBacks *valueCallBacks, Boolean useValueCB) {
    CFOptionFlags flags = kCFBasicHashLinearHashing; // kCFBasicHashExponentialHashing
    flags |= (CFDictionary ? kCFBasicHashHasKeys : 0) | (CFBag ? kCFBasicHashHasCounts : 0);

    CFBasicHashCallbacks *cb = NULL;
    Boolean std_cb = false;
    uint16_t specialBits = 0;
    const_any_pointer_t (*key_retain)(CFAllocatorRef, const_any_pointer_t) = NULL;
    void (*key_release)(CFAllocatorRef, const_any_pointer_t) = NULL;
    const_any_pointer_t (*value_retain)(CFAllocatorRef, const_any_pointer_t) = NULL;
    void (*value_release)(CFAllocatorRef, const_any_pointer_t) = NULL;

    if ((NULL == keyCallBacks || 0 == keyCallBacks->version) && (!useValueCB || NULL == valueCallBacks || 0 == valueCallBacks->version)) {
        Boolean keyRetainNull = NULL == keyCallBacks || NULL == keyCallBacks->retain;
        Boolean keyReleaseNull = NULL == keyCallBacks || NULL == keyCallBacks->release;
        Boolean keyEquateNull = NULL == keyCallBacks || NULL == keyCallBacks->equal;
        Boolean keyHashNull = NULL == keyCallBacks || NULL == keyCallBacks->hash;
        Boolean keyDescribeNull = NULL == keyCallBacks || NULL == keyCallBacks->copyDescription;

        Boolean valueRetainNull = (useValueCB && (NULL == valueCallBacks || NULL == valueCallBacks->retain)) || (!useValueCB && keyRetainNull);
        Boolean valueReleaseNull = (useValueCB && (NULL == valueCallBacks || NULL == valueCallBacks->release)) || (!useValueCB && keyReleaseNull);
        Boolean valueEquateNull = (useValueCB && (NULL == valueCallBacks || NULL == valueCallBacks->equal)) || (!useValueCB && keyEquateNull);
        Boolean valueDescribeNull = (useValueCB && (NULL == valueCallBacks || NULL == valueCallBacks->copyDescription)) || (!useValueCB && keyDescribeNull);

        Boolean keyRetainStd = keyRetainNull || __CFTypeCollectionRetain == keyCallBacks->retain;
        Boolean keyReleaseStd = keyReleaseNull || __CFTypeCollectionRelease == keyCallBacks->release;
        Boolean keyEquateStd = keyEquateNull || CFEqual == keyCallBacks->equal;
        Boolean keyHashStd = keyHashNull || CFHash == keyCallBacks->hash;
        Boolean keyDescribeStd = keyDescribeNull || CFCopyDescription == keyCallBacks->copyDescription;

        Boolean valueRetainStd = (useValueCB && (valueRetainNull || __CFTypeCollectionRetain == valueCallBacks->retain)) || (!useValueCB && keyRetainStd);
        Boolean valueReleaseStd = (useValueCB && (valueReleaseNull || __CFTypeCollectionRelease == valueCallBacks->release)) || (!useValueCB && keyReleaseStd);
        Boolean valueEquateStd = (useValueCB && (valueEquateNull || CFEqual == valueCallBacks->equal)) || (!useValueCB && keyEquateStd);
        Boolean valueDescribeStd = (useValueCB && (valueDescribeNull || CFCopyDescription == valueCallBacks->copyDescription)) || (!useValueCB && keyDescribeStd);

        if (keyRetainStd && keyReleaseStd && keyEquateStd && keyHashStd && keyDescribeStd && valueRetainStd && valueReleaseStd && valueEquateStd && valueDescribeStd) {
            cb = (CFBasicHashCallbacks *)&CFDictionaryStandardCallbacks;
            if (!(keyRetainNull || keyReleaseNull || keyEquateNull || keyHashNull || keyDescribeNull || valueRetainNull || valueReleaseNull || valueEquateNull || valueDescribeNull)) {
                std_cb = true;
            } else {
                // just set these to tickle the GC Strong logic below in a way that mimics past practice
                key_retain = keyCallBacks ? keyCallBacks->retain : NULL;
                key_release = keyCallBacks ? keyCallBacks->release : NULL;
                if (useValueCB) {
                    value_retain = valueCallBacks ? valueCallBacks->retain : NULL;
                    value_release = valueCallBacks ? valueCallBacks->release : NULL;
                } else {
                    value_retain = key_retain;
                    value_release = key_release;
                }
            }
            if (keyRetainNull) specialBits |= 0x0001;
            if (keyReleaseNull) specialBits |= 0x0002;
            if (keyEquateNull) specialBits |= 0x0004;
            if (keyHashNull) specialBits |= 0x0008;
            if (keyDescribeNull) specialBits |= 0x0010;
            if (valueRetainNull) specialBits |= 0x0100;
            if (valueReleaseNull) specialBits |= 0x0200;
            if (valueEquateNull) specialBits |= 0x0400;
            if (valueDescribeNull) specialBits |= 0x0800;
        }
    }

    if (!cb) {
        Boolean (*key_equal)(const_any_pointer_t, const_any_pointer_t) = NULL;
        Boolean (*value_equal)(const_any_pointer_t, const_any_pointer_t) = NULL;
        CFStringRef (*key_describe)(const_any_pointer_t) = NULL;
        CFStringRef (*value_describe)(const_any_pointer_t) = NULL;
        CFHashCode (*hash_key)(const_any_pointer_t) = NULL;
        key_retain = keyCallBacks ? keyCallBacks->retain : NULL;
        key_release = keyCallBacks ? keyCallBacks->release : NULL;
        key_equal = keyCallBacks ? keyCallBacks->equal : NULL;
        key_describe = keyCallBacks ? keyCallBacks->copyDescription : NULL;
        if (useValueCB) {
            value_retain = valueCallBacks ? valueCallBacks->retain : NULL;
            value_release = valueCallBacks ? valueCallBacks->release : NULL;
            value_equal = valueCallBacks ? valueCallBacks->equal : NULL;
            value_describe = valueCallBacks ? valueCallBacks->copyDescription : NULL;
        } else {
            value_retain = key_retain;
            value_release = key_release;
            value_equal = key_equal;
            value_describe = key_describe;
        }
        hash_key = keyCallBacks ? keyCallBacks->hash : NULL;

        CFBasicHashCallbacks *newcb = NULL;
        if (CF_IS_COLLECTABLE_ALLOCATOR(allocator)) {
            newcb = (CFBasicHashCallbacks *)auto_zone_allocate_object(objc_collectableZone(), sizeof(CFBasicHashCallbacks) + 10 * sizeof(void *), AUTO_MEMORY_UNSCANNED, false, false);
        } else {
            newcb = (CFBasicHashCallbacks *)CFAllocatorAllocate(allocator, sizeof(CFBasicHashCallbacks) + 10 * sizeof(void *), 0);
        }
        if (!newcb) HALT;
        newcb->copyCallbacks = __CFDictionaryCopyCallbacks;
        newcb->freeCallbacks = __CFDictionaryFreeCallbacks;
        newcb->retainValue = __CFDictionaryRetainValue;
        newcb->retainKey = __CFDictionaryRetainKey;
        newcb->releaseValue = __CFDictionaryReleaseValue;
        newcb->releaseKey = __CFDictionaryReleaseKey;
        newcb->equateValues = __CFDictionaryEquateValues;
        newcb->equateKeys = __CFDictionaryEquateKeys;
        newcb->hashKey = __CFDictionaryHashKey;
        newcb->getIndirectKey = __CFDictionaryGetIndirectKey;
        newcb->copyValueDescription = __CFDictionaryCopyValueDescription;
        newcb->copyKeyDescription = __CFDictionaryCopyKeyDescription;
        newcb->context[0] = (uintptr_t)value_retain;
        newcb->context[1] = (uintptr_t)key_retain;
        newcb->context[2] = (uintptr_t)value_release;
        newcb->context[3] = (uintptr_t)key_release;
        newcb->context[4] = (uintptr_t)value_equal;
        newcb->context[5] = (uintptr_t)key_equal;
        newcb->context[6] = (uintptr_t)hash_key;
        newcb->context[8] = (uintptr_t)value_describe;
        newcb->context[9] = (uintptr_t)key_describe;
        cb = newcb;
    }

    if (CF_IS_COLLECTABLE_ALLOCATOR(allocator)) {
        if (std_cb || value_retain != NULL || value_release != NULL) {
            flags |= kCFBasicHashStrongValues;
        }
        if (std_cb || key_retain != NULL || key_release != NULL) {
            flags |= kCFBasicHashStrongKeys;
        }
#if CFDictionary
        if (valueCallBacks == &kCFTypeDictionaryValueCompactableCallBacks) {
            // Foundation allocated collections will have compactable values
            flags |= kCFBasicHashCompactableValues;
        }
#endif
    }

    CFBasicHashRef ht = CFBasicHashCreate(allocator, flags, cb);
    CFBasicHashSetSpecialBits(ht, specialBits);
    return ht;
}

#if CFDictionary
__private_extern__ CFHashRef __CFDictionaryCreateTransfer(CFAllocatorRef allocator, const_any_pointer_t *klist, const_any_pointer_t *vlist, CFIndex numValues) {
#endif
#if CFSet || CFBag
__private_extern__ CFHashRef __CFDictionaryCreateTransfer(CFAllocatorRef allocator, const_any_pointer_t *klist, CFIndex numValues) {
    const_any_pointer_t *vlist = klist;
#endif
    CFTypeID typeID = CFDictionaryGetTypeID();
    CFAssert2(0 <= numValues, __kCFLogAssertion, "%s(): numValues (%ld) cannot be less than zero", __PRETTY_FUNCTION__, numValues);
    CFOptionFlags flags = kCFBasicHashLinearHashing; // kCFBasicHashExponentialHashing
    flags |= (CFDictionary ? kCFBasicHashHasKeys : 0) | (CFBag ? kCFBasicHashHasCounts : 0);
    CFBasicHashCallbacks *cb = (CFBasicHashCallbacks *)&CFDictionaryStandardCallbacks;
    CFBasicHashRef ht = CFBasicHashCreate(allocator, flags, cb);
    CFBasicHashSetSpecialBits(ht, 0x0303);
    if (0 < numValues) CFBasicHashSetCapacity(ht, numValues);
    for (CFIndex idx = 0; idx < numValues; idx++) {
        CFBasicHashAddValue(ht, (uintptr_t)klist[idx], (uintptr_t)vlist[idx]);
    }
    CFBasicHashSetSpecialBits(ht, 0x0000);
    CFBasicHashMakeImmutable(ht);
    *(uintptr_t *)ht = __CFISAForTypeID(typeID);
    _CFRuntimeSetInstanceTypeID(ht, typeID);
    if (__CFOASafe) __CFSetLastAllocationEventName(ht, "CFDictionary (immutable)");
    return (CFHashRef)ht;
}

#if CFDictionary
CFHashRef CFDictionaryCreate(CFAllocatorRef allocator, const_any_pointer_t *klist, const_any_pointer_t *vlist, CFIndex numValues, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks) {
#endif
#if CFSet || CFBag
CFHashRef CFDictionaryCreate(CFAllocatorRef allocator, const_any_pointer_t *klist, CFIndex numValues, const CFDictionaryKeyCallBacks *keyCallBacks) {
    const_any_pointer_t *vlist = klist;
    const CFDictionaryValueCallBacks *valueCallBacks = 0;
#endif
    CFTypeID typeID = CFDictionaryGetTypeID();
    CFAssert2(0 <= numValues, __kCFLogAssertion, "%s(): numValues (%ld) cannot be less than zero", __PRETTY_FUNCTION__, numValues);
    CFBasicHashRef ht = __CFDictionaryCreateGeneric(allocator, keyCallBacks, valueCallBacks, CFDictionary);
    if (0 < numValues) CFBasicHashSetCapacity(ht, numValues);
    for (CFIndex idx = 0; idx < numValues; idx++) {
        CFBasicHashAddValue(ht, (uintptr_t)klist[idx], (uintptr_t)vlist[idx]);
    }
    CFBasicHashMakeImmutable(ht);
    *(uintptr_t *)ht = __CFISAForTypeID(typeID);
    _CFRuntimeSetInstanceTypeID(ht, typeID);
    if (__CFOASafe) __CFSetLastAllocationEventName(ht, "CFDictionary (immutable)");
    return (CFHashRef)ht;
}

#if CFDictionary
CFMutableHashRef CFDictionaryCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks) {
#endif
#if CFSet || CFBag
CFMutableHashRef CFDictionaryCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFDictionaryKeyCallBacks *keyCallBacks) {
    const CFDictionaryValueCallBacks *valueCallBacks = 0;
#endif
    CFTypeID typeID = CFDictionaryGetTypeID();
    CFAssert2(0 <= capacity, __kCFLogAssertion, "%s(): capacity (%ld) cannot be less than zero", __PRETTY_FUNCTION__, capacity);
    CFBasicHashRef ht = __CFDictionaryCreateGeneric(allocator, keyCallBacks, valueCallBacks, CFDictionary);
    *(uintptr_t *)ht = __CFISAForTypeID(typeID);
    _CFRuntimeSetInstanceTypeID(ht, typeID);
    if (__CFOASafe) __CFSetLastAllocationEventName(ht, "CFDictionary (mutable)");
    return (CFMutableHashRef)ht;
}

CFHashRef CFDictionaryCreateCopy(CFAllocatorRef allocator, CFHashRef other) {
    CFTypeID typeID = CFDictionaryGetTypeID();
    CFAssert1(other, __kCFLogAssertion, "%s(): other CFDictionary cannot be NULL", __PRETTY_FUNCTION__);
    __CFGenericValidateType(other, typeID);
    CFBasicHashRef ht = NULL;
    if (CF_IS_OBJC(typeID, other)) {
        CFIndex numValues = CFDictionaryGetCount(other);
        const_any_pointer_t vbuffer[256], kbuffer[256];
        const_any_pointer_t *vlist = (numValues <= 256) ? vbuffer : (const_any_pointer_t *)CFAllocatorAllocate(kCFAllocatorSystemDefault, numValues * sizeof(const_any_pointer_t), 0);
#if CFSet || CFBag
        const_any_pointer_t *klist = vlist;
        CFDictionaryGetValues(other, vlist);
#endif
#if CFDictionary
        const_any_pointer_t *klist = (numValues <= 256) ? kbuffer : (const_any_pointer_t *)CFAllocatorAllocate(kCFAllocatorSystemDefault, numValues * sizeof(const_any_pointer_t), 0);
        CFDictionaryGetKeysAndValues(other, klist, vlist);
#endif
        ht = __CFDictionaryCreateGeneric(allocator, & kCFTypeDictionaryKeyCallBacks, CFDictionary ? & kCFTypeDictionaryValueCallBacks : NULL, CFDictionary);
        if (0 < numValues) CFBasicHashSetCapacity(ht, numValues);
        for (CFIndex idx = 0; idx < numValues; idx++) {
            CFBasicHashAddValue(ht, (uintptr_t)klist[idx], (uintptr_t)vlist[idx]);
        }
        if (klist != kbuffer && klist != vlist) CFAllocatorDeallocate(kCFAllocatorSystemDefault, klist);
        if (vlist != vbuffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, vlist);
    } else {
        ht = CFBasicHashCreateCopy(allocator, (CFBasicHashRef)other);
    }
    CFBasicHashMakeImmutable(ht);
    *(uintptr_t *)ht = __CFISAForTypeID(typeID);
    _CFRuntimeSetInstanceTypeID(ht, typeID);
    if (__CFOASafe) __CFSetLastAllocationEventName(ht, "CFDictionary (immutable)");
    return (CFHashRef)ht;
}

CFMutableHashRef CFDictionaryCreateMutableCopy(CFAllocatorRef allocator, CFIndex capacity, CFHashRef other) {
    CFTypeID typeID = CFDictionaryGetTypeID();
    CFAssert1(other, __kCFLogAssertion, "%s(): other CFDictionary cannot be NULL", __PRETTY_FUNCTION__);
    __CFGenericValidateType(other, typeID);
    CFAssert2(0 <= capacity, __kCFLogAssertion, "%s(): capacity (%ld) cannot be less than zero", __PRETTY_FUNCTION__, capacity);
    CFBasicHashRef ht = NULL;
    if (CF_IS_OBJC(typeID, other)) {
        CFIndex numValues = CFDictionaryGetCount(other);
        const_any_pointer_t vbuffer[256], kbuffer[256];
        const_any_pointer_t *vlist = (numValues <= 256) ? vbuffer : (const_any_pointer_t *)CFAllocatorAllocate(kCFAllocatorSystemDefault, numValues * sizeof(const_any_pointer_t), 0);
#if CFSet || CFBag
        const_any_pointer_t *klist = vlist;
        CFDictionaryGetValues(other, vlist);
#endif
#if CFDictionary
        const_any_pointer_t *klist = (numValues <= 256) ? kbuffer : (const_any_pointer_t *)CFAllocatorAllocate(kCFAllocatorSystemDefault, numValues * sizeof(const_any_pointer_t), 0);
        CFDictionaryGetKeysAndValues(other, klist, vlist);
#endif
        ht = __CFDictionaryCreateGeneric(allocator, & kCFTypeDictionaryKeyCallBacks, CFDictionary ? & kCFTypeDictionaryValueCallBacks : NULL, CFDictionary);
        if (0 < numValues) CFBasicHashSetCapacity(ht, numValues);
        for (CFIndex idx = 0; idx < numValues; idx++) {
            CFBasicHashAddValue(ht, (uintptr_t)klist[idx], (uintptr_t)vlist[idx]);
        }
        if (klist != kbuffer && klist != vlist) CFAllocatorDeallocate(kCFAllocatorSystemDefault, klist);
        if (vlist != vbuffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, vlist);
    } else {
        ht = CFBasicHashCreateCopy(allocator, (CFBasicHashRef)other);
    }
    *(uintptr_t *)ht = __CFISAForTypeID(typeID);
    _CFRuntimeSetInstanceTypeID(ht, typeID);
    if (__CFOASafe) __CFSetLastAllocationEventName(ht, "CFDictionary (mutable)");
    return (CFMutableHashRef)ht;
}

CFIndex CFDictionaryGetCount(CFHashRef hc) {
    if (CFDictionary) CF_OBJC_FUNCDISPATCH0(__kCFDictionaryTypeID, CFIndex, hc, "count");
    if (CFSet) CF_OBJC_FUNCDISPATCH0(__kCFDictionaryTypeID, CFIndex, hc, "count");
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    return CFBasicHashGetCount((CFBasicHashRef)hc);
}

#if CFDictionary
CFIndex CFDictionaryGetCountOfKey(CFHashRef hc, const_any_pointer_t key) {
#endif
#if CFSet || CFBag
CFIndex CFDictionaryGetCountOfValue(CFHashRef hc, const_any_pointer_t key) {
#endif
    if (CFDictionary) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, CFIndex, hc, "countForKey:", key);
    if (CFSet) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, CFIndex, hc, "countForObject:", key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    return CFBasicHashGetCountOfKey((CFBasicHashRef)hc, (uintptr_t)key);
}

#if CFDictionary
Boolean CFDictionaryContainsKey(CFHashRef hc, const_any_pointer_t key) {
#endif
#if CFSet || CFBag
Boolean CFDictionaryContainsValue(CFHashRef hc, const_any_pointer_t key) {
#endif
    if (CFDictionary) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, char, hc, "containsKey:", key);
    if (CFSet) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, char, hc, "containsObject:", key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    return (0 < CFBasicHashGetCountOfKey((CFBasicHashRef)hc, (uintptr_t)key));
}

const_any_pointer_t CFDictionaryGetValue(CFHashRef hc, const_any_pointer_t key) {
    if (CFDictionary) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, const_any_pointer_t, hc, "objectForKey:", key);
    if (CFSet) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, const_any_pointer_t, hc, "member:", key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFBasicHashBucket bkt = CFBasicHashFindBucket((CFBasicHashRef)hc, (uintptr_t)key);
    return (0 < bkt.count ? (const_any_pointer_t)bkt.weak_value : 0);
}

Boolean CFDictionaryGetValueIfPresent(CFHashRef hc, const_any_pointer_t key, const_any_pointer_t *value) {
    if (CFDictionary) CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, Boolean, hc, "__getValue:forKey:", (any_t *)value, key);
    if (CFSet) CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, Boolean, hc, "__getValue:forObj:", (any_t *)value, key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFBasicHashBucket bkt = CFBasicHashFindBucket((CFBasicHashRef)hc, (uintptr_t)key);
    if (0 < bkt.count) {
        if (value) {
            if (kCFUseCollectableAllocator && (CFBasicHashGetFlags((CFBasicHashRef)hc) & kCFBasicHashStrongValues)) {
                __CFAssignWithWriteBarrier((void **)value, (void *)bkt.weak_value);
            } else {
                *value = (const_any_pointer_t)bkt.weak_value;
            }
        }
        return true;
    }
    return false;
}

#if CFDictionary
CFIndex CFDictionaryGetCountOfValue(CFHashRef hc, const_any_pointer_t value) {
    CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, CFIndex, hc, "countForObject:", value);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    return CFBasicHashGetCountOfValue((CFBasicHashRef)hc, (uintptr_t)value);
}

Boolean CFDictionaryContainsValue(CFHashRef hc, const_any_pointer_t value) {
    CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, char, hc, "containsObject:", value);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    return (0 < CFBasicHashGetCountOfValue((CFBasicHashRef)hc, (uintptr_t)value));
}

CF_EXPORT Boolean CFDictionaryGetKeyIfPresent(CFHashRef hc, const_any_pointer_t key, const_any_pointer_t *actualkey) {
    CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, Boolean, hc, "getActualKey:forKey:", actualkey, key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFBasicHashBucket bkt = CFBasicHashFindBucket((CFBasicHashRef)hc, (uintptr_t)key);
    if (0 < bkt.count) {
        if (actualkey) {
            if (kCFUseCollectableAllocator && (CFBasicHashGetFlags((CFBasicHashRef)hc) & kCFBasicHashStrongKeys)) {
                __CFAssignWithWriteBarrier((void **)actualkey, (void *)bkt.weak_key);
            } else {
                *actualkey = (const_any_pointer_t)bkt.weak_key;
            }
        }
        return true;
    }
    return false;
}
#endif

#if CFDictionary
void CFDictionaryGetKeysAndValues(CFHashRef hc, const_any_pointer_t *keybuf, const_any_pointer_t *valuebuf) {
#endif
#if CFSet || CFBag
void CFDictionaryGetValues(CFHashRef hc, const_any_pointer_t *keybuf) {
    const_any_pointer_t *valuebuf = 0;
#endif
    if (CFDictionary) CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, void, hc, "getObjects:andKeys:", (any_t *)valuebuf, (any_t *)keybuf);
    if (CFSet) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, void, hc, "getObjects:", (any_t *)keybuf);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
#if __BLOCKS__
    if (kCFUseCollectableAllocator) {
        CFOptionFlags flags = CFBasicHashGetFlags((CFBasicHashRef)hc);
        __block const_any_pointer_t *keys = keybuf;
        __block const_any_pointer_t *values = valuebuf;
        CFBasicHashApply((CFBasicHashRef)hc, ^(CFBasicHashBucket bkt) {
                for (CFIndex cnt = bkt.count; cnt--;) {
                    if (keybuf && (flags & kCFBasicHashStrongKeys)) { __CFAssignWithWriteBarrier((void **)keys, (void *)bkt.weak_key); keys++; }
                    if (keybuf && !(flags & kCFBasicHashStrongKeys)) { *keys++ = (const_any_pointer_t)bkt.weak_key; }
                    if (valuebuf && (flags & kCFBasicHashStrongValues)) { __CFAssignWithWriteBarrier((void **)values, (void *)bkt.weak_value); values++; }
                    if (valuebuf && !(flags & kCFBasicHashStrongValues)) { *values++ = (const_any_pointer_t)bkt.weak_value; }
                }
                return (Boolean)true;
            });
    } else {
#else
	{
#endif
        CFBasicHashGetElements((CFBasicHashRef)hc, CFDictionaryGetCount(hc), (uintptr_t *)valuebuf, (uintptr_t *)keybuf);
    }
}

void CFDictionaryApplyFunction(CFHashRef hc, CFDictionaryApplierFunction applier, any_pointer_t context) {
    FAULT_CALLBACK((void **)&(applier));
    if (CFDictionary) CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, void, hc, "__apply:context:", applier, context);
    if (CFSet) CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, void, hc, "__applyValues:context:", applier, context);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
#if __BLOCKS__
    CFBasicHashApply((CFBasicHashRef)hc, ^(CFBasicHashBucket bkt) {
#if CFDictionary
            INVOKE_CALLBACK3(applier, (const_any_pointer_t)bkt.weak_key, (const_any_pointer_t)bkt.weak_value, context);
#endif
#if CFSet
            INVOKE_CALLBACK2(applier, (const_any_pointer_t)bkt.weak_value, context);
#endif
#if CFBag
            for (CFIndex cnt = bkt.count; cnt--;) {
                INVOKE_CALLBACK2(applier, (const_any_pointer_t)bkt.weak_value, context);
            }
#endif
            return (Boolean)true;
        });
#else // blockless!

#if CFDictionary
    CFIndex cnt = CFDictionaryGetCount(hc);
    STACK_BUFFER_DECL(any_t, keys, cnt);
    STACK_BUFFER_DECL(any_t, values, cnt);
    CFDictionaryGetKeysAndValues(hc, (const void **)keys, (const void **)values);
#elif CFSet
    CFIndex cnt = CFSetGetCount(hc);
    STACK_BUFFER_DECL(any_t, values, cnt);
    CFSetGetValues(hc, (const void **)values);
#elif CFBag
    CFIndex cnt = CFBagGetCount(hc);
    STACK_BUFFER_DECL(any_t, values, cnt);
    CFBagGetValues(hc, (const void **)values);
#endif

    for (CFIndex idx = 0; idx < cnt; idx++) {
#if CFDictionary
        INVOKE_CALLBACK3(applier, (const_any_pointer_t)keys[idx], (const_any_pointer_t)values[idx], context);
#endif
#if CFSet
        INVOKE_CALLBACK2(applier, (const_any_pointer_t)values[idx], context);
#elif CFBag
        for (CFIndex cntVal = CFBagGetCountOfValue(hc, (const void*)values[idx]); cntVal--;) {
            INVOKE_CALLBACK2(applier, (const_any_pointer_t)values[idx], context);
        }
#endif
    }
#endif
}

// This function is for Foundation's benefit; no one else should use it.
CF_EXPORT unsigned long _CFDictionaryFastEnumeration(CFHashRef hc, struct __objcFastEnumerationStateEquivalent *state, void *stackbuffer, unsigned long count) {
    if (CF_IS_OBJC(__kCFDictionaryTypeID, hc)) return 0;
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    return __CFBasicHashFastEnumeration((CFBasicHashRef)hc, (struct __objcFastEnumerationStateEquivalent2 *)state, stackbuffer, count);
}

// This function is for Foundation's benefit; no one else should use it.
CF_EXPORT Boolean _CFDictionaryIsMutable(CFHashRef hc) {
    if (CF_IS_OBJC(__kCFDictionaryTypeID, hc)) return false;
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    return CFBasicHashIsMutable((CFBasicHashRef)hc);
}

// This function is for Foundation's benefit; no one else should use it.
CF_EXPORT void _CFDictionarySetCapacity(CFMutableHashRef hc, CFIndex cap) {
    if (CF_IS_OBJC(__kCFDictionaryTypeID, hc)) return;
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFAssert2(CFBasicHashIsMutable((CFBasicHashRef)hc), __kCFLogAssertion, "%s(): immutable collection %p passed to mutating operation", __PRETTY_FUNCTION__, hc);
    CFAssert3(CFDictionaryGetCount(hc) <= cap, __kCFLogAssertion, "%s(): desired capacity (%ld) is less than count (%ld)", __PRETTY_FUNCTION__, cap, CFDictionaryGetCount(hc));
    CFBasicHashSetCapacity((CFBasicHashRef)hc, cap);
}

CF_INLINE CFIndex __CFDictionaryGetKVOBit(CFHashRef hc) {
    return __CFBitfieldGetValue(((CFRuntimeBase *)hc)->_cfinfo[CF_INFO_BITS], 0, 0);
}

CF_INLINE void __CFDictionarySetKVOBit(CFHashRef hc, CFIndex bit) {
    __CFBitfieldSetValue(((CFRuntimeBase *)hc)->_cfinfo[CF_INFO_BITS], 0, 0, ((uintptr_t)bit & 0x1));
}

// This function is for Foundation's benefit; no one else should use it.
CF_EXPORT CFIndex _CFDictionaryGetKVOBit(CFHashRef hc) {
    return __CFDictionaryGetKVOBit(hc);
}

// This function is for Foundation's benefit; no one else should use it.
CF_EXPORT void _CFDictionarySetKVOBit(CFHashRef hc, CFIndex bit) {
    __CFDictionarySetKVOBit(hc, bit);
}


#if !defined(CF_OBJC_KVO_WILLCHANGE)
#define CF_OBJC_KVO_WILLCHANGE(obj, key)
#define CF_OBJC_KVO_DIDCHANGE(obj, key)
#define CF_OBJC_KVO_WILLCHANGEALL(obj)
#define CF_OBJC_KVO_DIDCHANGEALL(obj)
#endif

#if CFDictionary
void CFDictionaryAddValue(CFMutableHashRef hc, const_any_pointer_t key, const_any_pointer_t value) {
#endif
#if CFSet || CFBag
void CFDictionaryAddValue(CFMutableHashRef hc, const_any_pointer_t key) {
    const_any_pointer_t value = key;
#endif
    if (CFDictionary) CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, void, hc, "addObject:forKey:", value, key);
    if (CFSet) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, void, hc, "addObject:", key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFAssert2(CFBasicHashIsMutable((CFBasicHashRef)hc), __kCFLogAssertion, "%s(): immutable collection %p passed to mutating operation", __PRETTY_FUNCTION__, hc);
    if (!CFBasicHashIsMutable((CFBasicHashRef)hc)) {
        CFLog(kCFLogLevelError, CFSTR("%s(): immutable collection %p given to mutating function"), __PRETTY_FUNCTION__, hc);
    }
    CF_OBJC_KVO_WILLCHANGE(hc, key);
    CFBasicHashAddValue((CFBasicHashRef)hc, (uintptr_t)key, (uintptr_t)value);
    CF_OBJC_KVO_DIDCHANGE(hc, key);
}

#if CFDictionary
void CFDictionaryReplaceValue(CFMutableHashRef hc, const_any_pointer_t key, const_any_pointer_t value) {
#endif
#if CFSet || CFBag
void CFDictionaryReplaceValue(CFMutableHashRef hc, const_any_pointer_t key) {
    const_any_pointer_t value = key;
#endif
    if (CFDictionary) CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, void, hc, "replaceObject:forKey:", value, key);
    if (CFSet) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, void, hc, "replaceObject:", key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFAssert2(CFBasicHashIsMutable((CFBasicHashRef)hc), __kCFLogAssertion, "%s(): immutable collection %p passed to mutating operation", __PRETTY_FUNCTION__, hc);
    if (!CFBasicHashIsMutable((CFBasicHashRef)hc)) {
        CFLog(kCFLogLevelError, CFSTR("%s(): immutable collection %p given to mutating function"), __PRETTY_FUNCTION__, hc);
    }
    CF_OBJC_KVO_WILLCHANGE(hc, key);
    CFBasicHashReplaceValue((CFBasicHashRef)hc, (uintptr_t)key, (uintptr_t)value);
    CF_OBJC_KVO_DIDCHANGE(hc, key);
}

#if CFDictionary
void CFDictionarySetValue(CFMutableHashRef hc, const_any_pointer_t key, const_any_pointer_t value) {
#endif
#if CFSet || CFBag
void CFDictionarySetValue(CFMutableHashRef hc, const_any_pointer_t key) {
    const_any_pointer_t value = key;
#endif
    if (CFDictionary) CF_OBJC_FUNCDISPATCH2(__kCFDictionaryTypeID, void, hc, "setObject:forKey:", value, key);
    if (CFSet) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, void, hc, "setObject:", key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFAssert2(CFBasicHashIsMutable((CFBasicHashRef)hc), __kCFLogAssertion, "%s(): immutable collection %p passed to mutating operation", __PRETTY_FUNCTION__, hc);
    if (!CFBasicHashIsMutable((CFBasicHashRef)hc)) {
        CFLog(kCFLogLevelError, CFSTR("%s(): immutable collection %p given to mutating function"), __PRETTY_FUNCTION__, hc);
    }
    CF_OBJC_KVO_WILLCHANGE(hc, key);
//#warning this for a dictionary used to not replace the key
    CFBasicHashSetValue((CFBasicHashRef)hc, (uintptr_t)key, (uintptr_t)value);
    CF_OBJC_KVO_DIDCHANGE(hc, key);
}

void CFDictionaryRemoveValue(CFMutableHashRef hc, const_any_pointer_t key) {
    if (CFDictionary) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, void, hc, "removeObjectForKey:", key);
    if (CFSet) CF_OBJC_FUNCDISPATCH1(__kCFDictionaryTypeID, void, hc, "removeObject:", key);
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFAssert2(CFBasicHashIsMutable((CFBasicHashRef)hc), __kCFLogAssertion, "%s(): immutable collection %p passed to mutating operation", __PRETTY_FUNCTION__, hc);
    if (!CFBasicHashIsMutable((CFBasicHashRef)hc)) {
        CFLog(kCFLogLevelError, CFSTR("%s(): immutable collection %p given to mutating function"), __PRETTY_FUNCTION__, hc);
    }
    CF_OBJC_KVO_WILLCHANGE(hc, key);
    CFBasicHashRemoveValue((CFBasicHashRef)hc, (uintptr_t)key);
    CF_OBJC_KVO_DIDCHANGE(hc, key);
}

void CFDictionaryRemoveAllValues(CFMutableHashRef hc) {
    if (CFDictionary) CF_OBJC_FUNCDISPATCH0(__kCFDictionaryTypeID, void, hc, "removeAllObjects");
    if (CFSet) CF_OBJC_FUNCDISPATCH0(__kCFDictionaryTypeID, void, hc, "removeAllObjects");
    __CFGenericValidateType(hc, __kCFDictionaryTypeID);
    CFAssert2(CFBasicHashIsMutable((CFBasicHashRef)hc), __kCFLogAssertion, "%s(): immutable collection %p passed to mutating operation", __PRETTY_FUNCTION__, hc);
    if (!CFBasicHashIsMutable((CFBasicHashRef)hc)) {
        CFLog(kCFLogLevelError, CFSTR("%s(): immutable collection %p given to mutating function"), __PRETTY_FUNCTION__, hc);
    }
    CF_OBJC_KVO_WILLCHANGEALL(hc);
    CFBasicHashRemoveAllValues((CFBasicHashRef)hc);
    CF_OBJC_KVO_DIDCHANGEALL(hc);
}

#undef CF_OBJC_KVO_WILLCHANGE
#undef CF_OBJC_KVO_DIDCHANGE
#undef CF_OBJC_KVO_WILLCHANGEALL
#undef CF_OBJC_KVO_DIDCHANGEALL

