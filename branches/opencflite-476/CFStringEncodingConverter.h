/*
 * Copyright (c) 2008-2009 Brent Fulgham <bfulgham@gmail.org>.  All rights reserved.
 *
 * This source code is a modified version of the CoreFoundation sources released by Apple Inc. under
 * the terms of the APSL version 2.0 (see below).
 *
 * For information about changes from the original Apple source release can be found by reviewing the
 * source control system for the project at https://sourceforge.net/svn/?group_id=246198.
 *
 * The original license information is as follows:
 * 
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
/*	CFStringEncodingConverter.h
	Copyright (c) 1998-2007, Apple Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFSTRINGENCODINGCONVERTER__)
#define __COREFOUNDATION_CFSTRINGENCODINGCONVERTER__ 1

#include <CoreFoundation/CFString.h>


CF_EXTERN_C_BEGIN

/* Values for flags argument for the conversion functions below.  These can be combined, but the three NonSpacing behavior flags are exclusive.
*/
enum {
    kCFStringEncodingAllowLossyConversion = 1, // Uses fallback functions to substitutes non mappable chars
    kCFStringEncodingBasicDirectionLeftToRight = (1 << 1), // Converted with original direction left-to-right.
    kCFStringEncodingBasicDirectionRightToLeft = (1 << 2), // Converted with original direction right-to-left.
    kCFStringEncodingSubstituteCombinings = (1 << 3), // Uses fallback function to combining chars.
    kCFStringEncodingComposeCombinings = (1 << 4), // Checks mappable precomposed equivalents for decomposed sequences.  This is the default behavior.
    kCFStringEncodingIgnoreCombinings = (1 << 5), // Ignores combining chars.
    kCFStringEncodingUseCanonical = (1 << 6), // Always use canonical form
    kCFStringEncodingUseHFSPlusCanonical = (1 << 7), // Always use canonical form but leaves 0x2000 ranges
    kCFStringEncodingPrependBOM = (1 << 8), // Prepend BOM sequence (i.e. ISO2022KR)
    kCFStringEncodingDisableCorporateArea = (1 << 9), // Disable the usage of 0xF8xx area for Apple proprietary chars in converting to UniChar, resulting loosely mapping.
    kCFStringEncodingASCIICompatibleConversion = (1 << 10), // This flag forces strict ASCII compatible converion. i.e. MacJapanese 0x5C maps to Unicode 0x5C.
    kCFStringEncodingLenientUTF8Conversion = (1 << 11) // 10.1 (Puma) compatible lenient UTF-8 conversion.
};

/* Return values for CFStringEncodingUnicodeToBytes & CFStringEncodingBytesToUnicode functions
*/
enum {
    kCFStringEncodingConversionSuccess = 0,
    kCFStringEncodingInvalidInputStream = 1,
    kCFStringEncodingInsufficientOutputBufferLength = 2,
    kCFStringEncodingConverterUnavailable = 3
};

/* Macro to shift lossByte argument.
*/
#define CFStringEncodingLossyByteToMask(lossByte)	((uint32_t)(lossByte << 24)|kCFStringEncodingAllowLossyConversion)
#define CFStringEncodingMaskToLossyByte(flags)		((uint8_t)(flags >> 24))

/* Converts characters into the specified encoding.  Returns the constants defined above.
If maxByteLen is 0, bytes is ignored. You can pass lossyByte by passing the value in flags argument.
i.e. CFStringEncodingUnicodeToBytes(encoding, CFStringEncodingLossyByteToMask(lossByte), ....)
*/
extern uint32_t CFStringEncodingUnicodeToBytes(uint32_t encoding, uint32_t flags, const UniChar *characters, CFIndex numChars, CFIndex *usedCharLen, uint8_t *bytes, CFIndex maxByteLen, CFIndex *usedByteLen);

/* Converts bytes in the specified encoding into unicode.  Returns the constants defined above.
maxCharLen & usdCharLen are in UniChar length, not byte length.
If maxCharLen is 0, characters is ignored.
*/
extern uint32_t CFStringEncodingBytesToUnicode(uint32_t encoding, uint32_t flags, const uint8_t *bytes, CFIndex numBytes, CFIndex *usedByteLen, UniChar *characters, CFIndex maxCharLen, CFIndex *usedCharLen);

/* Fallback functions used when allowLossy
*/
typedef CFIndex (*CFStringEncodingToBytesFallbackProc)(const UniChar *characters, CFIndex numChars, uint8_t *bytes, CFIndex maxByteLen, CFIndex *usedByteLen);
typedef CFIndex (*CFStringEncodingToUnicodeFallbackProc)(const uint8_t *bytes, CFIndex numBytes, UniChar *characters, CFIndex maxCharLen, CFIndex *usedCharLen);

extern bool CFStringEncodingIsValidEncoding(uint32_t encoding);

/* Returns kCFStringEncodingInvalidId terminated encoding list
*/
extern const uint32_t *CFStringEncodingListOfAvailableEncodings(void);

extern const char *CFStringEncodingName(uint32_t encoding);

/* Returns NULL-terminated list of IANA registered canonical names
*/
extern const char **CFStringEncodingCanonicalCharsetNames(uint32_t encoding);

/* Returns required length of destination buffer for conversion.  These functions are faster than specifying 0 to maxByteLen (maxCharLen), but unnecessarily optimal length
*/
extern CFIndex CFStringEncodingCharLengthForBytes(uint32_t encoding, uint32_t flags, const uint8_t *bytes, CFIndex numBytes);
extern CFIndex CFStringEncodingByteLengthForCharacters(uint32_t encoding, uint32_t flags, const UniChar *characters, CFIndex numChars);

/* Can register functions used for lossy conversion.  Reregisters default procs if NULL
*/
extern void CFStringEncodingRegisterFallbackProcedures(uint32_t encoding, CFStringEncodingToBytesFallbackProc toBytes, CFStringEncodingToUnicodeFallbackProc toUnicode);

CF_EXTERN_C_END

#endif /* ! __COREFOUNDATION_CFSTRINGENCODINGCONVERTER__ */

