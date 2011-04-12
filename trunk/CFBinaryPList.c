/*
 * Copyright (c) 2008-2011 Brent Fulgham <bfulgham@gmail.org>.  All rights reserved.
 * Copyright (c) 2009 Stuart Crook <stuart@echus.demon.co.uk>.  All rights reserved.
 *
 * This source code is a modified version of the CoreFoundation sources released by Apple Inc. under
 * the terms of the APSL version 2.0 (see below).
 *
 * For information about changes from the original Apple source release can be found by reviewing the
 * source control system for the project at https://sourceforge.net/svn/?group_id=246198.
 *
 * The original license information is as follows:
 * 
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

/*	CFBinaryPList.c
	Copyright (c) 2000-2009, Apple Inc. All rights reserved.
	Responsibility: Tony Parker
*/


#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFByteOrder.h>
#include <CoreFoundation/CFRuntime.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "CFInternal.h"

typedef struct {
    int64_t high;
    uint64_t low;
} CFSInt128Struct;

enum {
    kCFNumberSInt128Type = 17
};

CF_EXPORT CFNumberType _CFNumberGetType2(CFNumberRef number);
__private_extern__ CFErrorRef __CFPropertyListCreateError(CFAllocatorRef allocator, CFIndex code, CFStringRef debugString, ...);

enum {
	CF_NO_ERROR = 0,
	CF_OVERFLOW_ERROR = (1 << 0),
};

CF_INLINE uint32_t __check_uint32_add_unsigned_unsigned(uint32_t x, uint32_t y, int32_t* err) {
   if((UINT_MAX - y) < x)
        *err = *err | CF_OVERFLOW_ERROR;
   return x + y;
};

CF_INLINE uint64_t __check_uint64_add_unsigned_unsigned(uint64_t x, uint64_t y, int32_t* err) {
   if((ULLONG_MAX - y) < x)
        *err = *err | CF_OVERFLOW_ERROR;
   return x + y;
};

CF_INLINE uint32_t __check_uint32_mul_unsigned_unsigned(uint32_t x, uint32_t y, int32_t* err) {
   uint64_t tmp = (uint64_t) x * (uint64_t) y;
   /* If any of the upper 32 bits touched, overflow */
   if(tmp & 0xffffffff00000000ULL)
        *err = *err | CF_OVERFLOW_ERROR;
   return (uint32_t) tmp;
};

CF_INLINE uint64_t __check_uint64_mul_unsigned_unsigned(uint64_t x, uint64_t y, int32_t* err) {
  if(x == 0) return 0;
  if(ULLONG_MAX/x < y)
     *err = *err | CF_OVERFLOW_ERROR;
  return x * y;
};

#if __LP64__
#define check_ptr_add(p, a, err)	(const uint8_t *)__check_uint64_add_unsigned_unsigned((uintptr_t)p, (uintptr_t)a, err)
#define check_size_t_mul(b, a, err)	(size_t)__check_uint64_mul_unsigned_unsigned((size_t)b, (size_t)a, err)
#else
#define check_ptr_add(p, a, err)	(const uint8_t *)__check_uint32_add_unsigned_unsigned((uintptr_t)p, (uintptr_t)a, err)
#define check_size_t_mul(b, a, err)	(size_t)__check_uint32_mul_unsigned_unsigned((size_t)b, (size_t)a, err)
#endif


CF_INLINE CFTypeID __CFGenericTypeID_genericobj_inline(const void *cf) {
    CFTypeID typeID = (*(uint32_t *)(((CFRuntimeBase *)cf)->_cfinfo) >> 8) & 0xFFFF;
    return CF_IS_OBJC(typeID, cf) ? CFGetTypeID(cf) : typeID;
}

struct __CFKeyedArchiverUID {
    CFRuntimeBase _base;
    uint32_t _value;
};

static CFStringRef __CFKeyedArchiverUIDCopyDescription(CFTypeRef cf) {
    CFKeyedArchiverUIDRef uid = (CFKeyedArchiverUIDRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFKeyedArchiverUID %p [%p]>{value = %u}"), cf, CFGetAllocator(cf), uid->_value);
}

static CFStringRef __CFKeyedArchiverUIDCopyFormattingDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    CFKeyedArchiverUIDRef uid = (CFKeyedArchiverUIDRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("@%u@"), uid->_value);
}

static CFTypeID __kCFKeyedArchiverUIDTypeID = _kCFRuntimeNotATypeID;

static const CFRuntimeClass __CFKeyedArchiverUIDClass = {
    0,
    "CFKeyedArchiverUID",
    NULL,	// init
    NULL,	// copy
    NULL,	// finalize
    NULL,	// equal -- pointer equality only
    NULL,	// hash -- pointer hashing only
    __CFKeyedArchiverUIDCopyFormattingDescription,
    __CFKeyedArchiverUIDCopyDescription
};

__private_extern__ void __CFKeyedArchiverUIDInitialize(void) {
    __kCFKeyedArchiverUIDTypeID = _CFRuntimeRegisterClass(&__CFKeyedArchiverUIDClass);
}

CFTypeID _CFKeyedArchiverUIDGetTypeID(void) {
    return __kCFKeyedArchiverUIDTypeID;
}

CFKeyedArchiverUIDRef _CFKeyedArchiverUIDCreate(CFAllocatorRef allocator, uint32_t value) {
    CFKeyedArchiverUIDRef uid;
    uid = (CFKeyedArchiverUIDRef)_CFRuntimeCreateInstance(allocator, __kCFKeyedArchiverUIDTypeID, sizeof(struct __CFKeyedArchiverUID) - sizeof(CFRuntimeBase), NULL);
    if (NULL == uid) {
	return NULL;
    }
    ((struct __CFKeyedArchiverUID *)uid)->_value = value;
    return uid;
}


uint32_t _CFKeyedArchiverUIDGetValue(CFKeyedArchiverUIDRef uid) {
    return uid->_value;
}


typedef struct {
    CFTypeRef stream;
    CFErrorRef error;
    uint64_t written;
    int32_t used;
    bool streamIsData;
    uint8_t buffer[8192 - 32];
} __CFBinaryPlistWriteBuffer;

static void writeBytes(__CFBinaryPlistWriteBuffer *buf, const UInt8 *bytes, CFIndex length) {
    if (0 == length) return;
    if (buf->error) return;
    if (buf->streamIsData) {
        CFDataAppendBytes((CFMutableDataRef)buf->stream, bytes, length);
        buf->written += length;
    } else {
        /* SystemConfiguration relies on being able to serialize a plist to a write stream.
         * There seems no reason why this isn't supported. The old code read:
        CFAssert(false, __kCFLogAssertion, "Streams are not supported on this platform");
         */
        CFIndex lengthWritten = CFWriteStreamWrite((CFWriteStreamRef)buf->stream, bytes, length);
        buf->written += lengthWritten;
    }
}

static void bufferWrite(__CFBinaryPlistWriteBuffer *buf, const uint8_t *buffer, CFIndex count) {
    if (0 == count) return;
    if ((CFIndex)sizeof(buf->buffer) <= count) {
	writeBytes(buf, buf->buffer, buf->used);
	buf->used = 0;
	writeBytes(buf, buffer, count);
	return;
    }
    CFIndex copyLen = __CFMin(count, (CFIndex)sizeof(buf->buffer) - buf->used);
    memmove(buf->buffer + buf->used, buffer, copyLen);
    buf->used += copyLen;
    if (sizeof(buf->buffer) == buf->used) {
	writeBytes(buf, buf->buffer, sizeof(buf->buffer));
	memmove(buf->buffer, buffer + copyLen, count - copyLen);
	buf->used = count - copyLen;
    }
}

static void bufferFlush(__CFBinaryPlistWriteBuffer *buf) {
    writeBytes(buf, buf->buffer, buf->used);
    buf->used = 0;
}

/*
HEADER
	magic number ("bplist")
	file format version

OBJECT TABLE
	variable-sized objects

	Object Formats (marker byte followed by additional info in some cases)
	null	0000 0000
	bool	0000 1000			// false
	bool	0000 1001			// true
	fill	0000 1111			// fill byte
	int	0001 nnnn	...		// # of bytes is 2^nnnn, big-endian bytes
	real	0010 nnnn	...		// # of bytes is 2^nnnn, big-endian bytes
	date	0011 0011	...		// 8 byte float follows, big-endian bytes
	data	0100 nnnn	[int]	...	// nnnn is number of bytes unless 1111 then int count follows, followed by bytes
	string	0101 nnnn	[int]	...	// ASCII string, nnnn is # of chars, else 1111 then int count, then bytes
	string	0110 nnnn	[int]	...	// Unicode string, nnnn is # of chars, else 1111 then int count, then big-endian 2-byte uint16_t
		0111 xxxx			// unused
	uid	1000 nnnn	...		// nnnn+1 is # of bytes
		1001 xxxx			// unused
	array	1010 nnnn	[int]	objref*	// nnnn is count, unless '1111', then int count follows
		1011 xxxx			// unused
	set	1100 nnnn	[int]	objref* // nnnn is count, unless '1111', then int count follows
	dict	1101 nnnn	[int]	keyref* objref*	// nnnn is count, unless '1111', then int count follows
		1110 xxxx			// unused
		1111 xxxx			// unused

OFFSET TABLE
	list of ints, byte size of which is given in trailer
	-- these are the byte offsets into the file
	-- number of these is in the trailer

TRAILER
	byte size of offset ints in offset table
	byte size of object refs in arrays and dicts
	number of offsets in offset table (also is number of objects)
	element # in offset table which is top level object
	offset table offset

*/


static CFTypeID stringtype = -1, datatype = -1, numbertype = -1, datetype = -1;
static CFTypeID booltype = -1, nulltype = -1, dicttype = -1, arraytype = -1, settype = -1;

static void initStatics() {
    if ((CFTypeID)-1 == stringtype) {
        stringtype = CFStringGetTypeID();
    }
    if ((CFTypeID)-1 == datatype) {
        datatype = CFDataGetTypeID();
    }
    if ((CFTypeID)-1 == numbertype) {
        numbertype = CFNumberGetTypeID();
    }
    if ((CFTypeID)-1 == booltype) {
        booltype = CFBooleanGetTypeID();
    }
    if ((CFTypeID)-1 == datetype) {
        datetype = CFDateGetTypeID();
    }
    if ((CFTypeID)-1 == dicttype) {
        dicttype = CFDictionaryGetTypeID();
    }
    if ((CFTypeID)-1 == arraytype) {
        arraytype = CFArrayGetTypeID();
    }
    if ((CFTypeID)-1 == settype) {
        settype = CFSetGetTypeID();
    }
    if ((CFTypeID)-1 == nulltype) {
        nulltype = CFNullGetTypeID();
    }
}

static void _appendInt(__CFBinaryPlistWriteBuffer *buf, uint64_t bigint) {
    uint8_t marker;
    uint8_t *bytes;
    CFIndex nbytes;
    if (bigint <= (uint64_t)0xff) {
	nbytes = 1;
	marker = kCFBinaryPlistMarkerInt | 0;
    } else if (bigint <= (uint64_t)0xffff) {
	nbytes = 2;
	marker = kCFBinaryPlistMarkerInt | 1;
    } else if (bigint <= (uint64_t)0xffffffff) {
	nbytes = 4;
	marker = kCFBinaryPlistMarkerInt | 2;
    } else {
	nbytes = 8;
	marker = kCFBinaryPlistMarkerInt | 3;
    }
    bigint = CFSwapInt64HostToBig(bigint);
    bytes = (uint8_t *)&bigint + sizeof(bigint) - nbytes;
    bufferWrite(buf, &marker, 1);
    bufferWrite(buf, bytes, nbytes);
}

static void _appendUID(__CFBinaryPlistWriteBuffer *buf, CFKeyedArchiverUIDRef uid) {
    uint8_t marker;
    uint8_t *bytes;
    CFIndex nbytes;
    uint64_t bigint = _CFKeyedArchiverUIDGetValue(uid);
    if (bigint <= (uint64_t)0xff) {
	nbytes = 1;
    } else if (bigint <= (uint64_t)0xffff) {
	nbytes = 2;
    } else if (bigint <= (uint64_t)0xffffffff) {
	nbytes = 4;
    } else {
	nbytes = 8;
    }
    marker = kCFBinaryPlistMarkerUID | (uint8_t)(nbytes - 1);
    bigint = CFSwapInt64HostToBig(bigint);
    bytes = (uint8_t *)&bigint + sizeof(bigint) - nbytes;
    bufferWrite(buf, &marker, 1);
    bufferWrite(buf, bytes, nbytes);
}

static void _flattenPlist(CFPropertyListRef plist, CFMutableArrayRef objlist, CFMutableDictionaryRef objtable, CFMutableSetRef uniquingset) {
    CFPropertyListRef unique;
    uint32_t refnum;
    CFTypeID type = __CFGenericTypeID_genericobj_inline(plist);
    CFIndex idx;
    CFPropertyListRef *list, buffer[256];

    // Do not unique dictionaries or arrays, because: they
    // are slow to compare, and have poor hash codes.
    // Uniquing bools is unnecessary.
    if (stringtype == type || numbertype == type || datetype == type || datatype == type) {
	CFIndex before = CFSetGetCount(uniquingset);
	CFSetAddValue(uniquingset, plist);
	CFIndex after = CFSetGetCount(uniquingset);
	if (after == before) {	// already in set
	    unique = CFSetGetValue(uniquingset, plist);
	    if (unique != plist) {
		refnum = (uint32_t)(uintptr_t)CFDictionaryGetValue(objtable, unique);
		CFDictionaryAddValue(objtable, plist, (const void *)(uintptr_t)refnum);
	    }
	    return;
	}
    }
    refnum = CFArrayGetCount(objlist);
    CFArrayAppendValue(objlist, plist);
    CFDictionaryAddValue(objtable, plist, (const void *)(uintptr_t)refnum);
    if (dicttype == type) {
	CFIndex count = CFDictionaryGetCount((CFDictionaryRef)plist);
	list = (count <= 128) ? buffer : (CFPropertyListRef *)CFAllocatorAllocate(kCFAllocatorSystemDefault, 2 * count * sizeof(CFTypeRef), 0);
        CFDictionaryGetKeysAndValues((CFDictionaryRef)plist, list, list + count);
        for (idx = 0; idx < 2 * count; idx++) {
            _flattenPlist(list[idx], objlist, objtable, uniquingset);
        }
        if (list != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
    } else if (arraytype == type) {
	CFIndex count = CFArrayGetCount((CFArrayRef)plist);
	list = (count <= 256) ? buffer : (CFPropertyListRef *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count * sizeof(CFTypeRef), 0);
        CFArrayGetValues((CFArrayRef)plist, CFRangeMake(0, count), list);
        for (idx = 0; idx < count; idx++) {
            _flattenPlist(list[idx], objlist, objtable, uniquingset);
        }
        if (list != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
    }
}

/* Get the number of bytes required to hold the value in 'count'. Will return a power of 2 value big enough to hold 'count'.
 */
CF_INLINE uint8_t _byteCount(uint64_t count) {
    uint64_t mask = ~(uint64_t)0;
    uint8_t size = 0;
    
    // Find something big enough to hold 'count'
    while (count & mask) {
        size++;
        mask = mask << 8;
    }
    
    // Ensure that 'count' is a power of 2
    // For sizes bigger than 8, just use the required count
    while ((size != 1 && size != 2 && size != 4 && size != 8) && size <= 8) {
        size++;
    }
    
    return size;
}


// stream must be a CFMutableDataRef
/* Write a property list to a stream, in binary format. plist is the property list to write (one of the basic property list types), stream is the destination of the property list, and estimate is a best-guess at the total number of objects in the property list. The estimate parameter is for efficiency in pre-allocating memory for the uniquing step. Pass in a 0 if no estimate is available. The options flag specifies sort options. If the error parameter is non-NULL and an error occurs, it will be used to return a CFError explaining the problem. It is the callers responsibility to release the error. */
CFIndex __CFBinaryPlistWrite(CFPropertyListRef plist, CFTypeRef stream, uint64_t estimate, CFOptionFlags options, CFErrorRef *error) {
    CFMutableDictionaryRef objtable;
    CFMutableArrayRef objlist;
    CFMutableSetRef uniquingset;
    CFBinaryPlistTrailer trailer;
    uint64_t *offsets, length_so_far;
    uint64_t refnum;
    int64_t idx, idx2, cnt;
    __CFBinaryPlistWriteBuffer *buf;
    
    initStatics();

    objtable = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, NULL);
    objlist = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
    uniquingset = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
#if DEPLOYMENT_TARGET_MACOSX
    _CFDictionarySetCapacity(objtable, estimate ? estimate : 650);
    _CFArraySetCapacity(objlist, estimate ? estimate : 650);
    _CFSetSetCapacity(uniquingset, estimate ? estimate : 1000);
#endif

    _flattenPlist(plist, objlist, objtable, uniquingset);

    CFRelease(uniquingset);
    
    cnt = CFArrayGetCount(objlist);
    offsets = (uint64_t *)CFAllocatorAllocate(kCFAllocatorSystemDefault, (CFIndex)(cnt * sizeof(*offsets)), 0);

    buf = (__CFBinaryPlistWriteBuffer *)CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(__CFBinaryPlistWriteBuffer), 0);
    buf->stream = stream;
    buf->error = NULL;
    buf->streamIsData = (CFGetTypeID(stream) == CFDataGetTypeID());
    buf->written = 0;
    buf->used = 0;
    bufferWrite(buf, (uint8_t *)"bplist00", 8);	// header

    memset(&trailer, 0, sizeof(trailer));
    trailer._numObjects = CFSwapInt64HostToBig(cnt);
    trailer._topObject = 0;	// true for this implementation
    trailer._objectRefSize = _byteCount(cnt);    
    for (idx = 0; idx < cnt; idx++) {
	CFPropertyListRef obj = CFArrayGetValueAtIndex(objlist, (CFIndex)idx);
	CFTypeID type = __CFGenericTypeID_genericobj_inline(obj);
	offsets[idx] = buf->written + buf->used;
	if (stringtype == type) {
	    CFIndex ret, count = CFStringGetLength((CFStringRef)obj);
	    CFIndex needed;
	    uint8_t *bytes, buffer[1024];
	    bytes = (count <= 1024) ? buffer : (uint8_t *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count, 0);
	    // presumption, believed to be true, is that ASCII encoding may need
	    // less bytes, but will not need greater, than the # of unichars
	    ret = CFStringGetBytes((CFStringRef)obj, CFRangeMake(0, count), kCFStringEncodingASCII, 0, false, bytes, count, &needed);
	    if (ret == count) {
		uint8_t marker = (uint8_t)(kCFBinaryPlistMarkerASCIIString | (needed < 15 ? needed : 0xf));
		bufferWrite(buf, &marker, 1);
		if (15 <= needed) {
		    _appendInt(buf, (uint64_t)needed);
		}
		bufferWrite(buf, bytes, needed);
	    } else {
		UniChar *chars;
		uint8_t marker = (uint8_t)(kCFBinaryPlistMarkerUnicode16String | (count < 15 ? count : 0xf));
		bufferWrite(buf, &marker, 1);
		if (15 <= count) {
		    _appendInt(buf, (uint64_t)count);
		}
		chars = (UniChar *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count * sizeof(UniChar), 0);
		CFStringGetCharacters((CFStringRef)obj, CFRangeMake(0, count), chars);
		for (idx2 = 0; idx2 < count; idx2++) {
		    chars[idx2] = CFSwapInt16HostToBig(chars[idx2]);
		}
		bufferWrite(buf, (uint8_t *)chars, count * sizeof(UniChar));
		CFAllocatorDeallocate(kCFAllocatorSystemDefault, chars);
	    }
	    if (bytes != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, bytes);
	} else if (numbertype == type) {
	    uint8_t marker;
	    uint64_t bigint;
	    uint8_t *bytes;
	    CFIndex nbytes;
	    if (CFNumberIsFloatType((CFNumberRef)obj)) {
		CFSwappedFloat64 swapped64;
		CFSwappedFloat32 swapped32;
		if (CFNumberGetByteSize((CFNumberRef)obj) <= (CFIndex)sizeof(float)) {
		    float v;
		    CFNumberGetValue((CFNumberRef)obj, kCFNumberFloat32Type, &v);
		    swapped32 = CFConvertFloat32HostToSwapped(v);
		    bytes = (uint8_t *)&swapped32;
		    nbytes = sizeof(float);
		    marker = kCFBinaryPlistMarkerReal | 2;
		} else {
		    double v;
		    CFNumberGetValue((CFNumberRef)obj, kCFNumberFloat64Type, &v);
		    swapped64 = CFConvertFloat64HostToSwapped(v);
		    bytes = (uint8_t *)&swapped64;
		    nbytes = sizeof(double);
		    marker = kCFBinaryPlistMarkerReal | 3;
		}
		bufferWrite(buf, &marker, 1);
		bufferWrite(buf, bytes, nbytes);
	    } else {
		CFNumberType type = _CFNumberGetType2((CFNumberRef)obj);
		if (kCFNumberSInt128Type == type) {
		    CFSInt128Struct s;
		    CFNumberGetValue((CFNumberRef)obj, kCFNumberSInt128Type, &s);
		    struct {
			int64_t high;
			uint64_t low;
		    } storage;
		    storage.high = CFSwapInt64HostToBig(s.high);
		    storage.low = CFSwapInt64HostToBig(s.low);
		    uint8_t *bytes = (uint8_t *)&storage;
		    uint8_t marker = kCFBinaryPlistMarkerInt | 4;
		    CFIndex nbytes = 16;
		    bufferWrite(buf, &marker, 1);
		    bufferWrite(buf, bytes, nbytes);
		} else {
		    CFNumberGetValue((CFNumberRef)obj, kCFNumberSInt64Type, &bigint);
		    _appendInt(buf, bigint);
		}
	    }
	} else if (_CFKeyedArchiverUIDGetTypeID() == type) {
	    _appendUID(buf, (CFKeyedArchiverUIDRef)obj);
	} else if (booltype == type) {
	    uint8_t marker = CFBooleanGetValue((CFBooleanRef)obj) ? kCFBinaryPlistMarkerTrue : kCFBinaryPlistMarkerFalse;
	    bufferWrite(buf, &marker, 1);
	} else if (datatype == type) {
	    CFIndex count = CFDataGetLength((CFDataRef)obj);
	    uint8_t marker = (uint8_t)(kCFBinaryPlistMarkerData | (count < 15 ? count : 0xf));
	    bufferWrite(buf, &marker, 1);
	    if (15 <= count) {
		_appendInt(buf, (uint64_t)count);
	    }
	    bufferWrite(buf, CFDataGetBytePtr((CFDataRef)obj), count);
	} else if (datetype == type) {
	    CFSwappedFloat64 swapped;
	    uint8_t marker = kCFBinaryPlistMarkerDate;
	    bufferWrite(buf, &marker, 1);
	    swapped = CFConvertFloat64HostToSwapped(CFDateGetAbsoluteTime((CFDateRef)obj));
	    bufferWrite(buf, (uint8_t *)&swapped, sizeof(swapped));
	} else if (dicttype == type) {
            CFIndex count = CFDictionaryGetCount((CFDictionaryRef)obj);

            uint8_t marker = (uint8_t)(kCFBinaryPlistMarkerDict | (count < 15 ? count : 0xf));
            bufferWrite(buf, &marker, 1);
            if (15 <= count) {
                _appendInt(buf, (uint64_t)count);
            }
            
            CFPropertyListRef *list, buffer[512];

            list = (count <= 256) ? buffer : (CFPropertyListRef *)CFAllocatorAllocate(kCFAllocatorSystemDefault, 2 * count * sizeof(CFTypeRef), 0);
            CFDictionaryGetKeysAndValues((CFDictionaryRef)obj, list, list + count);
            for (idx2 = 0; idx2 < 2 * count; idx2++) {
                CFPropertyListRef value = list[idx2];
                uint32_t swapped = 0;
                uint8_t *source = (uint8_t *)&swapped;
                refnum = (uint32_t)(uintptr_t)CFDictionaryGetValue(objtable, value);
                swapped = CFSwapInt32HostToBig((uint32_t)refnum);
                bufferWrite(buf, source + sizeof(swapped) - trailer._objectRefSize, trailer._objectRefSize);
            }
            if (list != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
	} else if (arraytype == type) {
	    CFIndex count = CFArrayGetCount((CFArrayRef)obj);
	    CFPropertyListRef *list, buffer[256];
	    uint8_t marker = (uint8_t)(kCFBinaryPlistMarkerArray | (count < 15 ? count : 0xf));
	    bufferWrite(buf, &marker, 1);
	    if (15 <= count) {
		_appendInt(buf, (uint64_t)count);
	    }
	    list = (count <= 256) ? buffer : (CFPropertyListRef *)CFAllocatorAllocate(kCFAllocatorSystemDefault, count * sizeof(CFTypeRef), 0);
	    CFArrayGetValues((CFArrayRef)obj, CFRangeMake(0, count), list);
	    for (idx2 = 0; idx2 < count; idx2++) {
		CFPropertyListRef value = list[idx2];
		uint32_t swapped = 0;
		uint8_t *source = (uint8_t *)&swapped;
                refnum = (uint32_t)(uintptr_t)CFDictionaryGetValue(objtable, value);
                swapped = CFSwapInt32HostToBig((uint32_t)refnum);
		bufferWrite(buf, source + sizeof(swapped) - trailer._objectRefSize, trailer._objectRefSize);
	    }
	    if (list != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
	} else {
	    CFRelease(objtable);
	    CFRelease(objlist);
	    if (error && buf->error) {
		// caller will release error
		*error = buf->error;
	    } else if (buf->error) {
		// caller is not interested in error, release it here
		CFRelease(buf->error);
	    }
	    CFAllocatorDeallocate(kCFAllocatorSystemDefault, buf);
            CFAllocatorDeallocate(kCFAllocatorSystemDefault, offsets);
	    return 0;
	}
    }
    CFRelease(objtable);
    CFRelease(objlist);
    
    length_so_far = buf->written + buf->used;
    trailer._offsetTableOffset = CFSwapInt64HostToBig(length_so_far);
    trailer._offsetIntSize = _byteCount(length_so_far);
    
    for (idx = 0; idx < cnt; idx++) {
	uint64_t swapped = CFSwapInt64HostToBig(offsets[idx]);
	uint8_t *source = (uint8_t *)&swapped;
	bufferWrite(buf, source + sizeof(*offsets) - trailer._offsetIntSize, trailer._offsetIntSize);
    }
    length_so_far += cnt * trailer._offsetIntSize;

    bufferWrite(buf, (uint8_t *)&trailer, sizeof(trailer));
    bufferFlush(buf);
    length_so_far += sizeof(trailer);
    if (buf->error) {
	if (error) {
	    // caller will release error
	    *error = buf->error;
	} else {
	    CFRelease(buf->error);
	}
	return 0;
    }
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, buf);
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, offsets);
    return (CFIndex)length_so_far;
}


CFIndex __CFBinaryPlistWriteToStream(CFPropertyListRef plist, CFTypeRef stream) {
    return __CFBinaryPlistWrite(plist, stream, 0, 0, NULL);
}

// to be removed soon
CFIndex __CFBinaryPlistWriteToStreamWithEstimate(CFPropertyListRef plist, CFTypeRef stream, uint64_t estimate) {
    return __CFBinaryPlistWrite(plist, stream, estimate, 0, NULL);
}

// to be removed soon
CFIndex __CFBinaryPlistWriteToStreamWithOptions(CFPropertyListRef plist, CFTypeRef stream, uint64_t estimate, CFOptionFlags options) {
    return __CFBinaryPlistWrite(plist, stream, estimate, options, NULL);
}

#define FAIL_FALSE	do { return false; } while (0)
#define FAIL_MAXOFFSET	do { return UINT64_MAX; } while (0)

/* Grab a valSize-bytes integer out of the buffer pointed at by data and return it.
 */
CF_INLINE uint64_t _getSizedInt(const uint8_t *data, uint8_t valSize) {
    if (valSize == 1) {
        return (uint64_t)*data;
    } else if (valSize == 2) {
        uint16_t val = *(uint16_t *)data;
        return (uint64_t)CFSwapInt16BigToHost(val);
    } else if (valSize == 4) {
        uint32_t val = *(uint32_t *)data;
        return (uint64_t)CFSwapInt32BigToHost(val);
    } else if (valSize == 8) {
        uint64_t val = *(uint64_t *)data;
        return CFSwapInt64BigToHost(val);
    } else {
        // Compatability with existing archives, including anything with a non-power-of-2 size and 16-byte values
        uint64_t res = 0;
        for (CFIndex idx = 0; idx < valSize; idx++) {
            res = (res << 8) + data[idx];
        }
        return res;
    }
    // shouldn't get here
    return 0;
}

bool __CFBinaryPlistGetTopLevelInfo(const uint8_t *databytes, uint64_t datalen, uint8_t *marker, uint64_t *offset, CFBinaryPlistTrailer *trailer) {
    CFBinaryPlistTrailer trail;

    initStatics();

    if (!databytes || datalen < sizeof(trail) + 8 + 1) FAIL_FALSE;
    // Tiger and earlier will parse "bplist00"
    // Leopard will parse "bplist00" or "bplist01"
    // SnowLeopard will parse "bplist0?" where ? is any one character
    if (0 != memcmp("bplist0", databytes, 7)) {
	return false;
    }
    memmove(&trail, databytes + datalen - sizeof(trail), sizeof(trail));
    // In Leopard, the unused bytes in the trailer must be 0 or the parse will fail
    // This check is not present in Tiger and earlier or after Leopard
    trail._numObjects = CFSwapInt64BigToHost(trail._numObjects);
    trail._topObject = CFSwapInt64BigToHost(trail._topObject);
    trail._offsetTableOffset = CFSwapInt64BigToHost(trail._offsetTableOffset);
    if (LONG_MAX < trail._numObjects) FAIL_FALSE;
    if (LONG_MAX < trail._offsetTableOffset) FAIL_FALSE;
    if (trail._numObjects < 1) FAIL_FALSE;
    if (trail._numObjects <= trail._topObject) FAIL_FALSE;
    if (trail._offsetTableOffset < 9) FAIL_FALSE;
    if (datalen - sizeof(trail) <= trail._offsetTableOffset) FAIL_FALSE;
    if (trail._offsetIntSize < 1) FAIL_FALSE;
    if (trail._objectRefSize < 1) FAIL_FALSE;
    int32_t err = CF_NO_ERROR;
    uint64_t offsetIntSize = trail._offsetIntSize;
    uint64_t offsetTableSize = __check_uint64_mul_unsigned_unsigned(trail._numObjects, offsetIntSize, &err);
    if (CF_NO_ERROR!= err) FAIL_FALSE;
    if (offsetTableSize < 1) FAIL_FALSE;
    uint64_t objectDataSize = trail._offsetTableOffset - 8;
    uint64_t tmpSum = __check_uint64_add_unsigned_unsigned(8, objectDataSize, &err);
    tmpSum = __check_uint64_add_unsigned_unsigned(tmpSum, offsetTableSize, &err);
    tmpSum = __check_uint64_add_unsigned_unsigned(tmpSum, sizeof(trail), &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;
    if (datalen != tmpSum) FAIL_FALSE;
    if (trail._objectRefSize < 8 && (1ULL << (8 * trail._objectRefSize)) <= trail._numObjects) FAIL_FALSE;
    if (trail._offsetIntSize < 8 && (1ULL << (8 * trail._offsetIntSize)) <= trail._offsetTableOffset) FAIL_FALSE;
    const uint8_t *objectsFirstByte;
    objectsFirstByte = check_ptr_add(databytes, 8, &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;
    const uint8_t *offsetsFirstByte = check_ptr_add(databytes, trail._offsetTableOffset, &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;
    const uint8_t *offsetsLastByte;
    offsetsLastByte = check_ptr_add(offsetsFirstByte, offsetTableSize - 1, &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;

    const uint8_t *bytesptr = databytes + trail._offsetTableOffset;
    uint64_t maxOffset = trail._offsetTableOffset - 1;
    for (CFIndex idx = 0; idx < trail._numObjects; idx++) {
	uint64_t off = _getSizedInt(bytesptr, trail._offsetIntSize);
	if (maxOffset < off) FAIL_FALSE;
	bytesptr += trail._offsetIntSize;
    }

    bytesptr = databytes + trail._offsetTableOffset + trail._topObject * trail._offsetIntSize;
    uint64_t off = _getSizedInt(bytesptr, trail._offsetIntSize);
    if (off < 8 || trail._offsetTableOffset <= off) FAIL_FALSE;
    if (trailer) *trailer = trail;
    if (offset) *offset = off;
    if (marker) *marker = *(databytes + off);
    return true;
}

CF_INLINE Boolean _plistIsPrimitive(CFPropertyListRef pl) {
    CFTypeID type = __CFGenericTypeID_genericobj_inline(pl);
    if (dicttype == type || arraytype == type || settype == type) FAIL_FALSE;
    return true;
}

CF_INLINE bool _readInt(const uint8_t *ptr, const uint8_t *end_byte_ptr, uint64_t *bigint, const uint8_t **newptr) {
    if (end_byte_ptr < ptr) FAIL_FALSE;
    uint8_t marker = *ptr++;
    if ((marker & 0xf0) != kCFBinaryPlistMarkerInt) FAIL_FALSE;
    uint64_t cnt = 1 << (marker & 0x0f);
    int32_t err = CF_NO_ERROR;
    const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
    if (CF_NO_ERROR != err) FAIL_FALSE;
    if (end_byte_ptr < extent) FAIL_FALSE;
    // integers are not required to be in the most compact possible representation, but only the last 64 bits are significant currently
    *bigint = _getSizedInt(ptr, cnt);
    ptr += cnt;
    if (newptr) *newptr = ptr;
    return true;
}

// bytesptr points at a ref
CF_INLINE uint64_t _getOffsetOfRefAt(const uint8_t *databytes, const uint8_t *bytesptr, const CFBinaryPlistTrailer *trailer) {
    // *trailer contents are trusted, even for overflows -- was checked when the trailer was parsed;
    // this pointer arithmetic and the multiplication was also already done once and checked,
    // and the offsetTable was already validated.
    const uint8_t *objectsFirstByte = databytes + 8;
    const uint8_t *offsetsFirstByte = databytes + trailer->_offsetTableOffset;
    if (bytesptr < objectsFirstByte || offsetsFirstByte - trailer->_objectRefSize < bytesptr) FAIL_MAXOFFSET;

    uint64_t ref = _getSizedInt(bytesptr, trailer->_objectRefSize);
    if (trailer->_numObjects <= ref) FAIL_MAXOFFSET;

    bytesptr = databytes + trailer->_offsetTableOffset + ref * trailer->_offsetIntSize;
    uint64_t off = _getSizedInt(bytesptr, trailer->_offsetIntSize);
    return off;
}

bool __CFBinaryPlistGetOffsetForValueFromArray2(const uint8_t *databytes, uint64_t datalen, uint64_t startOffset, const CFBinaryPlistTrailer *trailer, CFIndex idx, uint64_t *offset, CFMutableDictionaryRef objects) {
    uint64_t objectsRangeStart = 8, objectsRangeEnd = trailer->_offsetTableOffset - 1;
    if (startOffset < objectsRangeStart || objectsRangeEnd < startOffset) FAIL_FALSE;
    const uint8_t *ptr = databytes + startOffset;
    uint8_t marker = *ptr;
    if ((marker & 0xf0) != kCFBinaryPlistMarkerArray) FAIL_FALSE;
    int32_t err = CF_NO_ERROR;
    ptr = check_ptr_add(ptr, 1, &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;
    uint64_t cnt = (marker & 0x0f);
    if (0xf == cnt) {
	uint64_t bigint;
	if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr)) FAIL_FALSE;
	if (LONG_MAX < bigint) FAIL_FALSE;
	cnt = bigint;
    }
    if (cnt <= idx) FAIL_FALSE;
    size_t byte_cnt = check_size_t_mul(cnt, trailer->_objectRefSize, &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;
    const uint8_t *extent = check_ptr_add(ptr, byte_cnt, &err) - 1;
    if (CF_NO_ERROR != err) FAIL_FALSE;
    if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
    uint64_t off = _getOffsetOfRefAt(databytes, ptr + idx * trailer->_objectRefSize, trailer);
    if (offset) *offset = off;
    return true;
}

// Compatibility method, to be removed soon
CF_EXPORT bool __CFBinaryPlistGetOffsetForValueFromDictionary2(const uint8_t *databytes, uint64_t datalen, uint64_t startOffset, const CFBinaryPlistTrailer *trailer, CFTypeRef key, uint64_t *koffset, uint64_t *voffset, CFMutableDictionaryRef objects) {
    return __CFBinaryPlistGetOffsetForValueFromDictionary3(databytes, datalen, startOffset, trailer, key, koffset, voffset, false, objects);
}

/* Get the offset for a value in a dictionary in a binary property list.
 @param databytes A pointer to the start of the binary property list data.
 @param datalen The length of the data.
 @param startOffset The offset at which the dictionary starts.
 @param trailer A pointer to a filled out trailer structure (use __CFBinaryPlistGetTopLevelInfo).
 @param key A string key in the dictionary that should be searched for.
 @param koffset Will be filled out with the offset to the key in the data bytes.
 @param voffset Will be filled out with the offset to the value in the data bytes.
 @param unused Unused parameter.
 @param objects Used for caching objects. Should be a valid CFMutableDictionaryRef.
 @return True if the key was found, false otherwise.
*/
bool __CFBinaryPlistGetOffsetForValueFromDictionary3(const uint8_t *databytes, uint64_t datalen, uint64_t startOffset, const CFBinaryPlistTrailer *trailer, CFTypeRef key, uint64_t *koffset, uint64_t *voffset, Boolean unused, CFMutableDictionaryRef objects) {
    
    // Require a key that is a plist primitive
    if (!key || !_plistIsPrimitive(key)) FAIL_FALSE;
    
    // Require that startOffset is in the range of the object table
    uint64_t objectsRangeStart = 8, objectsRangeEnd = trailer->_offsetTableOffset - 1;
    if (startOffset < objectsRangeStart || objectsRangeEnd < startOffset) FAIL_FALSE;
    
    // ptr is the start of the dictionary we are reading
    const uint8_t *ptr = databytes + startOffset;
    
    // Check that the data pointer actually points to a dictionary
    uint8_t marker = *ptr;
    if ((marker & 0xf0) != kCFBinaryPlistMarkerDict) FAIL_FALSE;
    
    // Get the number of objects in this dictionary
    int32_t err = CF_NO_ERROR;
    ptr = check_ptr_add(ptr, 1, &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;
    uint64_t cnt = (marker & 0x0f);
    if (0xf == cnt) {
	uint64_t bigint = 0;
	if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr)) FAIL_FALSE;
	if (LONG_MAX < bigint) FAIL_FALSE;
	cnt = bigint;
    }
    
    // Total number of objects (keys + values) is cnt * 2
    cnt = check_size_t_mul(cnt, 2, &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;
    size_t byte_cnt = check_size_t_mul(cnt, trailer->_objectRefSize, &err);
    if (CF_NO_ERROR != err) FAIL_FALSE;
    
    // Find the end of the dictionary
    const uint8_t *extent = check_ptr_add(ptr, byte_cnt, &err) - 1;
    if (CF_NO_ERROR != err) FAIL_FALSE;
    
    // Check that we didn't overflow the size of the dictionary
    if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
    
    // For short keys (15 bytes or less) in ASCII form, we can do a quick comparison check
    // We get the pointer or copy the buffer here, outside of the loop
    CFIndex stringKeyLen = -1;
    if (__CFGenericTypeID_genericobj_inline(key) == stringtype) {
	stringKeyLen = CFStringGetLength((CFStringRef)key);
    }
    
    // Find the object in the dictionary with this key
    cnt = cnt / 2;
    uint64_t totalKeySize = cnt * trailer->_objectRefSize;
    uint64_t off;
    Boolean match = false;
    CFPropertyListRef keyInData = NULL;
    
    char keyBuffer[16];
    const char *keyBufferPtr = keyBuffer;
    
    if (stringKeyLen < 0xf) {
	// Since we will only be comparing ASCII strings, we can attempt to get a pointer using MacRoman encoding
	// (this is cheaper than a copy)
	if (!(keyBufferPtr = CFStringGetCStringPtr((CFStringRef)key, kCFStringEncodingMacRoman))) {
	    CFStringGetCString((CFStringRef)key, keyBuffer, 16, kCFStringEncodingMacRoman);
	    // The pointer should now point to our keyBuffer instead of the original string buffer, since we've copied it
	    keyBufferPtr = keyBuffer;
	}
    }
    
    // Perform linear search of the keys
    for (CFIndex idx = 0; idx < cnt; idx++) {
	off = _getOffsetOfRefAt(databytes, ptr, trailer);
	marker = *(databytes + off);
	CFIndex len = marker & 0x0f;
	// if it is a short ascii string in the data, and the key is a string
	if (stringKeyLen != -1 && len < 0xf && (marker & 0xf0) == kCFBinaryPlistMarkerASCIIString) {
	    if (len == stringKeyLen) {                
		err = CF_NO_ERROR;
		const uint8_t *ptr2 = databytes + off;
		extent = check_ptr_add(ptr2, len, &err);
		if (CF_NO_ERROR != err) FAIL_FALSE;
		
		if (databytes + trailer->_offsetTableOffset <= extent) FAIL_FALSE;
		
		// Compare the key to this potential match (ptr2 + 1 moves past the marker)
		if (memcmp(ptr2 + 1, keyBufferPtr, stringKeyLen) == 0) {
		    match = true;
		}
	    }
	} else {
	    keyInData = NULL;
	    if (!__CFBinaryPlistCreateObject2(databytes, datalen, off, trailer, kCFAllocatorSystemDefault, kCFPropertyListImmutable, objects, NULL, 0, &keyInData) || !_plistIsPrimitive(keyInData)) {
		if (keyInData) CFRelease(keyInData);
		return false;
	    }
	    
	    match = CFEqual(key, keyInData);            
	    CFRelease(keyInData);
	}            
	
	if (match) {
	    if (koffset) *koffset = off;
	    if (voffset) *voffset = _getOffsetOfRefAt(databytes, ptr + totalKeySize, trailer);
	    return true;
	}
	 
	ptr += trailer->_objectRefSize;
    }
    
    return false;
}

CF_EXPORT bool __CFBinaryPlistCreateObject2(const uint8_t *databytes, uint64_t datalen, uint64_t startOffset, const CFBinaryPlistTrailer *trailer, CFAllocatorRef allocator, CFOptionFlags mutabilityOption, CFMutableDictionaryRef objects, CFMutableSetRef set, CFIndex curDepth, CFPropertyListRef *plist) {

    if (objects) {
	*plist = CFDictionaryGetValue(objects, (const void *)(uintptr_t)startOffset);
	if (*plist) {
	    CFRetain(*plist);
	    return true;
	}
    }

    // at any one invocation of this function, set should contain the offsets in the "path" down to this object
    if (set && CFSetContainsValue(set, (const void *)(uintptr_t)startOffset)) return false;

    // databytes is trusted to be at least datalen bytes long
    // *trailer contents are trusted, even for overflows -- was checked when the trailer was parsed
    uint64_t objectsRangeStart = 8, objectsRangeEnd = trailer->_offsetTableOffset - 1;
    if (startOffset < objectsRangeStart || objectsRangeEnd < startOffset) FAIL_FALSE;

    uint64_t off;
    CFPropertyListRef *list, buffer[256];
    CFAllocatorRef listAllocator;

    uint8_t marker = *(databytes + startOffset);
    switch (marker & 0xf0) {
    case kCFBinaryPlistMarkerNull:
	switch (marker) {
	case kCFBinaryPlistMarkerNull:
	    *plist = kCFNull;
	    return true;
	case kCFBinaryPlistMarkerFalse:
	    *plist = CFRetain(kCFBooleanFalse);
	    return true;
	case kCFBinaryPlistMarkerTrue:
	    *plist = CFRetain(kCFBooleanTrue);
	    return true;
	}
	FAIL_FALSE;
    case kCFBinaryPlistMarkerInt:
    {
	const uint8_t *ptr = (databytes + startOffset);
	int32_t err = CF_NO_ERROR;
	ptr = check_ptr_add(ptr, 1, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	uint64_t cnt = 1 << (marker & 0x0f);
	const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
	if (CF_NO_ERROR != err) FAIL_FALSE;
	if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	if (16 < cnt) FAIL_FALSE;
	// in format version '00', 1, 2, and 4-byte integers have to be interpreted as unsigned,
	// whereas 8-byte integers are signed (and 16-byte when available)
	// negative 1, 2, 4-byte integers are always emitted as 8 bytes in format '00'
	// integers are not required to be in the most compact possible representation, but only the last 64 bits are significant currently
	uint64_t bigint = _getSizedInt(ptr, cnt);
	ptr += cnt;
	if (8 < cnt) {
	    CFSInt128Struct val;
	    val.high = 0;
	    val.low = bigint;
	    *plist = CFNumberCreate(allocator, kCFNumberSInt128Type, &val);
	} else {
	    *plist = CFNumberCreate(allocator, kCFNumberSInt64Type, &bigint);
	}
	// these are always immutable
	if (objects && *plist) {
	    CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	}
	return (*plist) ? true : false;
    }
    case kCFBinaryPlistMarkerReal:
	switch (marker & 0x0f) {
	case 2: {
	    const uint8_t *ptr = (databytes + startOffset);
	    int32_t err = CF_NO_ERROR;
	    ptr = check_ptr_add(ptr, 1, &err);
	    if (CF_NO_ERROR != err) FAIL_FALSE;
	    const uint8_t *extent = check_ptr_add(ptr, 4, &err) - 1;
	    if (CF_NO_ERROR != err) FAIL_FALSE;
	    if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	    CFSwappedFloat32 swapped32;
	    memmove(&swapped32, ptr, 4);
	    float f = CFConvertFloat32SwappedToHost(swapped32);
	    *plist = CFNumberCreate(allocator, kCFNumberFloat32Type, &f);
	    // these are always immutable
	    if (objects && *plist) {
		CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	    }
	    return (*plist) ? true : false;
	}
	case 3: {
	    const uint8_t *ptr = (databytes + startOffset);
	    int32_t err = CF_NO_ERROR;
	    ptr = check_ptr_add(ptr, 1, &err);
	    if (CF_NO_ERROR != err) FAIL_FALSE;
	    const uint8_t *extent = check_ptr_add(ptr, 8, &err) - 1;
	    if (CF_NO_ERROR != err) FAIL_FALSE;
	    if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	    CFSwappedFloat64 swapped64;
	    memmove(&swapped64, ptr, 8);
	    double d = CFConvertFloat64SwappedToHost(swapped64);
	    *plist = CFNumberCreate(allocator, kCFNumberFloat64Type, &d);
	    // these are always immutable
	    if (objects && *plist) {
		CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	    }
	    return (*plist) ? true : false;
	}
	}
	FAIL_FALSE;
    case kCFBinaryPlistMarkerDate & 0xf0:
	switch (marker) {
	case kCFBinaryPlistMarkerDate: {
	    const uint8_t *ptr = (databytes + startOffset);
	    int32_t err = CF_NO_ERROR;
	    ptr = check_ptr_add(ptr, 1, &err);
	    if (CF_NO_ERROR != err) FAIL_FALSE;
	    const uint8_t *extent = check_ptr_add(ptr, 8, &err) - 1;
	    if (CF_NO_ERROR != err) FAIL_FALSE;
	    if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	    CFSwappedFloat64 swapped64;
	    memmove(&swapped64, ptr, 8);
	    double d = CFConvertFloat64SwappedToHost(swapped64);
	    *plist = CFDateCreate(allocator, d);
	    // these are always immutable
	    if (objects && *plist) {
		CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	    }
	    return (*plist) ? true : false;
	}
	}
	FAIL_FALSE;
    case kCFBinaryPlistMarkerData: {
	const uint8_t *ptr = databytes + startOffset;
	int32_t err = CF_NO_ERROR;
	ptr = check_ptr_add(ptr, 1, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	CFIndex cnt = marker & 0x0f;
	if (0xf == cnt) {
	    uint64_t bigint = 0;
	    if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr)) FAIL_FALSE;
	    if (LONG_MAX < bigint) FAIL_FALSE;
	    cnt = (CFIndex)bigint;
	}
	const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
	if (CF_NO_ERROR != err) FAIL_FALSE;
	if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	if (mutabilityOption == kCFPropertyListMutableContainersAndLeaves) {
	    *plist = CFDataCreateMutable(allocator, 0);
	    if (*plist) CFDataAppendBytes((CFMutableDataRef)*plist, ptr, cnt);
	} else {
	    *plist = CFDataCreate(allocator, ptr, cnt);
	}
        if (objects && *plist && (mutabilityOption != kCFPropertyListMutableContainersAndLeaves)) {
	    CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	}
	return (*plist) ? true : false;
	}
    case kCFBinaryPlistMarkerASCIIString: {
	const uint8_t *ptr = databytes + startOffset;
	int32_t err = CF_NO_ERROR;
	ptr = check_ptr_add(ptr, 1, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	CFIndex cnt = marker & 0x0f;
	if (0xf == cnt) {
            uint64_t bigint = 0;
	    if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr)) FAIL_FALSE;
	    if (LONG_MAX < bigint) FAIL_FALSE;
	    cnt = (CFIndex)bigint;
	}
	const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
	if (CF_NO_ERROR != err) FAIL_FALSE;
	if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	if (mutabilityOption == kCFPropertyListMutableContainersAndLeaves) {
	    CFStringRef str = CFStringCreateWithBytes(allocator, ptr, cnt, kCFStringEncodingASCII, false);
	    *plist = str ? CFStringCreateMutableCopy(allocator, 0, str) : NULL;
	    if (str) CFRelease(str);
	} else {
	    *plist = CFStringCreateWithBytes(allocator, ptr, cnt, kCFStringEncodingASCII, false);
	}
        if (objects && *plist && (mutabilityOption != kCFPropertyListMutableContainersAndLeaves)) {
	    CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	}
	return (*plist) ? true : false;
	}
    case kCFBinaryPlistMarkerUnicode16String: {
	const uint8_t *ptr = databytes + startOffset;
	int32_t err = CF_NO_ERROR;
	ptr = check_ptr_add(ptr, 1, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	CFIndex cnt = marker & 0x0f;
	if (0xf == cnt) {
            uint64_t bigint = 0;
	    if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr)) FAIL_FALSE;
	    if (LONG_MAX < bigint) FAIL_FALSE;
	    cnt = (CFIndex)bigint;
	}
	const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
	extent = check_ptr_add(extent, cnt, &err);	// 2 bytes per character
	if (CF_NO_ERROR != err) FAIL_FALSE;
	if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	size_t byte_cnt = check_size_t_mul(cnt, sizeof(UniChar), &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	UniChar *chars = (UniChar *)CFAllocatorAllocate(allocator, byte_cnt, 0);
	if (!chars) FAIL_FALSE;
	memmove(chars, ptr, byte_cnt);
	for (CFIndex idx = 0; idx < cnt; idx++) {
	    chars[idx] = CFSwapInt16BigToHost(chars[idx]);
	}
	if (mutabilityOption == kCFPropertyListMutableContainersAndLeaves) {
	    CFStringRef str = CFStringCreateWithCharactersNoCopy(allocator, chars, cnt, allocator);
	    *plist = str ? CFStringCreateMutableCopy(allocator, 0, str) : NULL;
	    if (str) CFRelease(str);
	} else {
	    *plist = CFStringCreateWithCharactersNoCopy(allocator, chars, cnt, allocator);
	}
        if (objects && *plist && (mutabilityOption != kCFPropertyListMutableContainersAndLeaves)) {
	    CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	}
	return (*plist) ? true : false;
	}
    case kCFBinaryPlistMarkerUID: {
	const uint8_t *ptr = databytes + startOffset;
	int32_t err = CF_NO_ERROR;
	ptr = check_ptr_add(ptr, 1, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	CFIndex cnt = (marker & 0x0f) + 1;
	const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
	if (CF_NO_ERROR != err) FAIL_FALSE;
	if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	// uids are not required to be in the most compact possible representation, but only the last 64 bits are significant currently
	uint64_t bigint = _getSizedInt(ptr, cnt);
	ptr += cnt;
	if (UINT32_MAX < bigint) FAIL_FALSE;
	*plist = _CFKeyedArchiverUIDCreate(allocator, (uint32_t)bigint);
	// these are always immutable
	if (objects && *plist) {
	    CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	}
	return (*plist) ? true : false;
	}
    case kCFBinaryPlistMarkerArray:
    case kCFBinaryPlistMarkerSet: {
	const uint8_t *ptr = databytes + startOffset;
	int32_t err = CF_NO_ERROR;
	ptr = check_ptr_add(ptr, 1, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	CFIndex cnt = marker & 0x0f;
	if (0xf == cnt) {
	    uint64_t bigint = 0;
	    if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr)) FAIL_FALSE;
	    if (LONG_MAX < bigint) FAIL_FALSE;
	    cnt = (CFIndex)bigint;
	}
	size_t byte_cnt = check_size_t_mul(cnt, trailer->_objectRefSize, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	const uint8_t *extent = check_ptr_add(ptr, byte_cnt, &err) - 1;
	if (CF_NO_ERROR != err) FAIL_FALSE;
	if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	byte_cnt = check_size_t_mul(cnt, sizeof(CFPropertyListRef), &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	list = (cnt <= 256) ? buffer : (CFPropertyListRef *)CFAllocatorAllocate(kCFAllocatorSystemDefault, byte_cnt, 0);
	listAllocator = (list == buffer ? kCFAllocatorNull : kCFAllocatorSystemDefault);
	if (!list) FAIL_FALSE;
	Boolean madeSet = false;
	if (!set && 15 < curDepth) {
	    set = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
	    madeSet = set ? true : false;
	}
	if (set) CFSetAddValue(set, (const void *)(uintptr_t)startOffset);
	for (CFIndex idx = 0; idx < cnt; idx++) {
	    CFPropertyListRef pl;
	    off = _getOffsetOfRefAt(databytes, ptr, trailer);
	    if (!__CFBinaryPlistCreateObject2(databytes, datalen, off, trailer, allocator, mutabilityOption, objects, set, curDepth + 1, &pl)) {
		while (idx--) {
		    CFRelease(list[idx]);
		}	    
		if (list != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
		FAIL_FALSE;
	    }
	    list[idx] = pl;
	    ptr += trailer->_objectRefSize;
	}
	if (set) CFSetRemoveValue(set, (const void *)(uintptr_t)startOffset);
	if (madeSet) {
	    CFRelease(set);
	    set = NULL;
	}
	if ((marker & 0xf0) == kCFBinaryPlistMarkerArray) {
	    if (mutabilityOption != kCFPropertyListImmutable) {
		*plist = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
		CFArrayReplaceValues((CFMutableArrayRef)*plist, CFRangeMake(0, 0), list, cnt);
	    } else {
		*plist = CFArrayCreate(allocator, list, cnt, &kCFTypeArrayCallBacks);
	    }
	} else {
	    if (mutabilityOption != kCFPropertyListImmutable) {
		*plist = CFSetCreateMutable(allocator, 0, &kCFTypeSetCallBacks);
	        for (CFIndex idx = 0; idx < cnt; idx++) {
		    CFSetAddValue((CFMutableSetRef)*plist, list[idx]);
	        }
	    } else {
		*plist = CFSetCreate(allocator, list, cnt, &kCFTypeSetCallBacks);
	    }
	}
	for (CFIndex idx = 0; idx < cnt; idx++) {
	    CFRelease(list[idx]);
	}
	if (objects && *plist && (mutabilityOption == kCFPropertyListImmutable)) {
	    CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	}
	if (list != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
	return (*plist) ? true : false;
	}
    case kCFBinaryPlistMarkerDict: {
	const uint8_t *ptr = databytes + startOffset;
	int32_t err = CF_NO_ERROR;
	ptr = check_ptr_add(ptr, 1, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	CFIndex cnt = marker & 0x0f;
	if (0xf == cnt) {
	    uint64_t bigint = 0;
	    if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr)) FAIL_FALSE;
	    if (LONG_MAX < bigint) FAIL_FALSE;
	    cnt = (CFIndex)bigint;
	}
	cnt = check_size_t_mul(cnt, 2, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	size_t byte_cnt = check_size_t_mul(cnt, trailer->_objectRefSize, &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	const uint8_t *extent = check_ptr_add(ptr, byte_cnt, &err) - 1;
	if (CF_NO_ERROR != err) FAIL_FALSE;
	if (databytes + objectsRangeEnd < extent) FAIL_FALSE;
	byte_cnt = check_size_t_mul(cnt, sizeof(CFPropertyListRef), &err);
	if (CF_NO_ERROR != err) FAIL_FALSE;
	list = (cnt <= 256) ? buffer : (CFPropertyListRef *)CFAllocatorAllocate(kCFAllocatorSystemDefault, byte_cnt, 0);
	listAllocator = (list == buffer ? kCFAllocatorNull : kCFAllocatorSystemDefault);
	if (!list) FAIL_FALSE;
	Boolean madeSet = false;
	if (!set && 15 < curDepth) {
	    set = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
	    madeSet = set ? true : false;
	}
	if (set) CFSetAddValue(set, (const void *)(uintptr_t)startOffset);
	for (CFIndex idx = 0; idx < cnt; idx++) {
	    CFPropertyListRef pl = NULL;
	    off = _getOffsetOfRefAt(databytes, ptr, trailer);
	    if (!__CFBinaryPlistCreateObject2(databytes, datalen, off, trailer, allocator, mutabilityOption, objects, set, curDepth + 1, &pl) || (idx < cnt / 2 && !_plistIsPrimitive(pl))) {
		if (pl) CFRelease(pl);
		while (idx--) {
		    CFRelease(list[idx]);
		}
		if (list != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
		FAIL_FALSE;
	    }
	    list[idx] = pl;
	    ptr += trailer->_objectRefSize;
	}
	if (set) CFSetRemoveValue(set, (const void *)(uintptr_t)startOffset);
	if (madeSet) {
	    CFRelease(set);
	    set = NULL;
	}
	if (mutabilityOption != kCFPropertyListImmutable) {
	    *plist = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	    for (CFIndex idx = 0; idx < cnt / 2; idx++) {
		CFDictionaryAddValue((CFMutableDictionaryRef)*plist, list[idx], list[idx + cnt / 2]);
	    }
	} else {
	    *plist = CFDictionaryCreate(allocator, list, list + cnt / 2, cnt / 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}
	for (CFIndex idx = 0; idx < cnt; idx++) {
		CFRelease(list[idx]);
	}
	if (objects && *plist && (mutabilityOption == kCFPropertyListImmutable)) {
	    CFDictionarySetValue(objects, (const void *)(uintptr_t)startOffset, *plist);
	}
	if (list != buffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
	return (*plist) ? true : false;
	}
    }
    FAIL_FALSE;
}

bool __CFBinaryPlistCreateObject(const uint8_t *databytes, uint64_t datalen, uint64_t startOffset, const CFBinaryPlistTrailer *trailer, CFAllocatorRef allocator, CFOptionFlags mutabilityOption, CFMutableDictionaryRef objects, CFPropertyListRef *plist) {
	// for compatibility with Foundation's use, need to leave this here
    return __CFBinaryPlistCreateObject2(databytes, datalen, startOffset, trailer, allocator, mutabilityOption, objects, NULL, 0, plist);
}

__private_extern__ bool __CFTryParseBinaryPlist(CFAllocatorRef allocator, CFDataRef data, CFOptionFlags option, CFPropertyListRef *plist, CFStringRef *errorString) {
    uint8_t marker;    
    CFBinaryPlistTrailer trailer;
    uint64_t offset;
    const uint8_t *databytes = CFDataGetBytePtr(data);
    uint64_t datalen = CFDataGetLength(data);

    if (8 <= datalen && __CFBinaryPlistGetTopLevelInfo(databytes, datalen, &marker, &offset, &trailer)) {
	// FALSE: We know for binary plist parsing that the result objects will be retained
	// by their containing collections as the parsing proceeds, so we do not need
	// to use retaining callbacks for the objects map in this case. WHY: the file might
	// be malformed and contain hash-equal keys for the same dictionary (for example)
	// and the later key will cause the previous one to be released when we set the second
	// in the dictionary.
	CFMutableDictionaryRef objects = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
	_CFDictionarySetCapacity(objects, trailer._numObjects);
	CFPropertyListRef pl = NULL;
        if (__CFBinaryPlistCreateObject2(databytes, datalen, offset, &trailer, allocator, option, objects, NULL, 0, &pl)) {
	    if (plist) *plist = pl;
        } else {
	    if (plist) *plist = NULL;
            if (errorString) *errorString = (CFStringRef)CFRetain(CFSTR("binary data is corrupt"));
	}
	CFRelease(objects);
        return true;
    }
    return false;
}
