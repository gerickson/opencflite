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
 *     This file implements a demonstration of the CoreFoundation
 *     CFTimeZone object by effecting a set of functional examples
 *     based on the time zone tests found in CFTest
 *     <https://github.com/davecotter/cftest> by Dave M. Cotter
 *     <dave@kjams.com>.
 *
 */

#define DEBUG 1

#include <memory>

#include <unistd.h>

#include <AssertMacros.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFTimeZone.h>


using namespace std;


/* Preprocessor Definitions */

#if !defined(LOG_CFTIMEZONE)
#define LOG_CFTIMEZONE 0
#endif

#define __CFTimeZoneLog(format, ...)       do { fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr); } while (0)

#if LOG_CFTIMEZONE
#define __CFTimeZoneMaybeLog(format, ...)  __CFTimeZoneLog(format, ##__VA_ARGS__)
#else
#define __CFTimeZoneMaybeLog(format, ...)
#endif

#define __CFTimeZoneMaybeTraceWithFormat(dir, name, format, ...)	  \
	__CFTimeZoneMaybeLog(dir " %s" format, name, ##__VA_ARGS__)
#define __CFTimeZoneTraceEnterWithFormat(format, ...)               \
	__CFTimeZoneMaybeTraceWithFormat("-->", __func__, " " format, ##__VA_ARGS__)
#define __CFTimeZoneTraceExitWithFormat(format, ...)                \
	__CFTimeZoneMaybeTraceWithFormat("<--", __func__, " " format, ##__VA_ARGS__)
#define __CFTimeZoneTraceEnter()                                    \
	__CFTimeZoneTraceEnterWithFormat("\n")
#define __CFTimeZoneTraceExit()                                     \
	__CFTimeZoneTraceExitWithFormat("\n")

/* Function Prototypes */

static void __CFTimeZoneExampleShow(CFStringRef formatString, ...);
static int  __CFTimeZoneExampleShow(CFDictionaryRef dictRef);
static int  __CFTimeZoneExampleShow(CFTimeZoneRef tzRef);

static int _CFTimeZoneExampleReport(CFStringRef utf8z, const int32_t &err);
static int _CFTimeZoneExampleReport(CFStringRef tzStr, CFStringRef key, const int32_t err);
static int _CFTimeZoneExampleDisplayKnownNames(void);
static int _CFTimeZoneExampleDisplayAbbreviations(void);
static int _CFTimeZoneExampleDisplayGMTOffsets(void);
static CFDictionaryRef _CFTimeZoneExampleCopyDict(CFTimeZoneRef tz);
static int _CFTimeZoneExampleCreateTimeZones(void);
static int _CFTimeZoneExampleResetSystemAddExample(CFMutableArrayRef testArray,
                                                   CFStringRef zoneName,
                                                   CFStringRef displayName,
                                                   CFStringRef zoneAbbrev,
                                                   CFStringRef zoneDSTAbbrev,
                                                   CFStringRef localizedName,
                                                   CFStringRef localizedDSTName,
                                                   double zoneOffset,
                                                   double zoneDSTOffset);
static int _CFTimeZoneExampleResetSystem(CFDictionaryRef testDict);
static int _CFTimeZoneExampleResetSystem(CFArrayRef testArray);
static int _CFTimeZoneExampleResetSystem(void);

/* Global Variables */

static CFStringRef kCFTimeZoneDictKey_Name                 = CFSTR("name");
static CFStringRef kCFTimeZoneDictKey_DisplayName          = CFSTR("display_name");
static CFStringRef kCFTimeZoneDictKey_LocalizedName        = CFSTR("localized_name");
static CFStringRef kCFTimeZoneDictKey_LocalizedName_Dst    = CFSTR("localized_dst_name");
static CFStringRef kCFTimeZoneDictKey_Abbreviation         = CFSTR("abbrev");
static CFStringRef kCFTimeZoneDictKey_Abbreviation_Dst     = CFSTR("abbrev_dst");
static CFStringRef kCFTimeZoneDictKey_ZoneOffset           = CFSTR("zone_offset");
static CFStringRef kCFTimeZoneDictKey_IsDst                = CFSTR("is_dst");
static CFStringRef kCFTimeZoneDictKey_DstOffset            = CFSTR("dst_offset");

/* static */ void
__CFTimeZoneExampleShow(CFStringRef formatString, ...) {
    CFStringRef resultString;
    CFDataRef data;
    va_list argList;

    va_start(argList, formatString);
    resultString = CFStringCreateWithFormatAndArguments(NULL, NULL, formatString, argList);
    va_end(argList);

    data = CFStringCreateExternalRepresentation(NULL, resultString, CFStringGetSystemEncoding(), '?');

    if (data != NULL) {
        __CFTimeZoneLog("%.*s\n", (int)CFDataGetLength(data), CFDataGetBytePtr(data));
        CFRelease(data);
    }

    CFRelease(resultString);
}

/* static */ int
__CFTimeZoneExampleShow(CFDictionaryRef dictRef) {
    CFIndex         dictCount;
    CFStringRef *   keys = NULL;
    CFStringRef *   values = NULL;
    int             retval = 0;

    dictCount = CFDictionaryGetCount(dictRef);
    __Require_Action(dictCount > 0, done, retval++);

    keys = new (std::nothrow) CFStringRef[dictCount];
    __Require_Action(keys != NULL, done, retval++);

    values = new (std::nothrow) CFStringRef[dictCount];
    __Require_Action(values != NULL, done, retval++);

    CFDictionaryGetKeysAndValues(dictRef, (const void **)keys, (const void **)values);

    __CFTimeZoneExampleShow(CFSTR("CFDictionary:\n{"));

    for (CFIndex i = 0; i < dictCount; i++) {
        CFStringRef key   = keys[i];
        CFStringRef value = values[i];

        __Require_Action(key != NULL, done, retval++);
        __Require_Action(value != NULL, done, retval++);

        __CFTimeZoneExampleShow(CFSTR("	%@: %@"), key, value);
    }

    __CFTimeZoneExampleShow(CFSTR("}"));

 done:
    if (keys != NULL) {
        delete [] keys;
    }

    if (values != NULL) {
        delete [] values;
    }

    return (retval);
}

/* static */ int
__CFTimeZoneExampleShow(CFTimeZoneRef tzRef) {

    CFStringRef descIDStr = CFCopyTypeIDDescription(CFGetTypeID(tzRef));
    CFStringRef descStr   = CFCopyDescription(tzRef);
    int         retval    = 0;

    __Require_Action(descIDStr != NULL, done, retval++);
    __Require_Action(descStr != NULL, done, retval++);

    __CFTimeZoneExampleShow(CFSTR("%@:\n%@"), descIDStr, descStr);

 done:
    if (descIDStr != NULL) {
        CFRelease(descIDStr);
    }

    if (descStr != NULL) {
        CFRelease(descStr);
    }

    return (retval);
}

/* static */ int
_CFTimeZoneExampleReport(CFStringRef utf8z, const int32_t &err) {
    if (err) {
        __CFTimeZoneExampleShow(CFSTR("FAIL: (%d) %@"),
                                err,
                                utf8z);
    } else {
        __CFTimeZoneExampleShow(CFSTR("PASS: %@"),
                                utf8z);
    }

    return (err != 0);
}

/* static */ int
_CFTimeZoneExampleReport(CFStringRef tzStr, CFStringRef key, const int32_t err) {
    CFStringRef temp   = NULL;
    int         retval = 0;

    temp = CFStringCreateWithFormat(kCFAllocatorDefault,
                                    NULL,
                                    CFSTR("CFTimeZone: %@: %@"),
                                    tzStr,
                                    key);
    __Require_Action(temp != NULL, done, retval++);

    _CFTimeZoneExampleReport(temp, err);

 done:
    if (err) {
        retval++;
    }

    if (temp != NULL) {
        CFRelease(temp);
    }

    return (retval);
}

/* static */ int
_CFTimeZoneExampleDisplayKnownNames(void) {
    CFArrayRef namesArray = NULL;
    CFIndex    arrayCount;
    int        retval = 0;

    namesArray = CFTimeZoneCopyKnownNames();
    __Require_Action(namesArray != NULL, done, retval++);

    arrayCount = CFArrayGetCount(namesArray);
    __Require_Action(arrayCount > 0, done, retval++);

    for (CFIndex i = 0; i < arrayCount; i++) {
        CFStringRef name = NULL;

        name = static_cast<CFStringRef>(CFArrayGetValueAtIndex(namesArray, i));
        __Require_Action(name != NULL, done, retval++);

        __CFTimeZoneExampleShow(CFSTR("%@"), name);
    }

    __CFTimeZoneExampleShow(CFSTR("------------------------"));

 done:
    if (namesArray != NULL) {
        CFRelease(namesArray);
    }

    return (retval);
}

/* static */ int
_CFTimeZoneExampleDisplayAbbreviations(void) {
    CFDictionaryRef abbrevDict = NULL;
    int             retval = 0;

    abbrevDict = CFTimeZoneCopyAbbreviationDictionary();
    __Require_Action(abbrevDict != NULL, done, retval++);

    __CFTimeZoneExampleShow(abbrevDict);

    __CFTimeZoneExampleShow(CFSTR("------------------------"));

 done:
    if (abbrevDict != NULL) {
        CFRelease(abbrevDict);
    }

    return (retval);
}

/*
 * At present, this test functions on no CoreFoundation platform,
 * including macOS.
 *
 */
/* static */ int
_CFTimeZoneExampleDisplayGMTOffsets(void) {
#if 0
    static const double _kSecondsPerHour = 3600.0;
    static const double _kSecondsPerDay = 86400.0;
    CFAbsoluteTime  absT       = CFAbsoluteTimeGetCurrent();
    CFTimeZoneRef   curTz      = CFTimeZoneCopyDefault();
    CFTimeInterval  intervalTz = CFTimeZoneGetSecondsFromGMT(curTz, absT);
    int             retval     = 0;

    for (CFIndex i = 0; i < 24; i++) {
        CFTimeZoneRef      timeZone      = NULL;
        CFStringRef        zoneName      = NULL;
        CFLocaleRef        localeRef     = NULL;
        CFDateFormatterRef dateFormatter = NULL;
        CFStringRef        dateString    = NULL;

        timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(kCFAllocatorDefault, intervalTz);
        __Require_Action(timeZone != NULL, done, retval++);

        zoneName = CFTimeZoneGetName(timeZone);
        __Require_Action(zoneName != NULL, done, retval++);

        localeRef = CFLocaleCopyCurrent();
        __Require_Action(localeRef != NULL, done, retval++);

        dateFormatter = CFDateFormatterCreate(kCFAllocatorDefault, localeRef, kCFDateFormatterFullStyle, kCFDateFormatterFullStyle);
        __Require_Action(dateFormatter != NULL, done, retval++);

        CFDateFormatterSetFormat(dateFormatter, CFSTR("EEE, d MMM y H:mm:ss z"));

        CFDateFormatterSetProperty(dateFormatter, kCFDateFormatterTimeZone, zoneName);

        dateString = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, dateFormatter, absT);
        __Require_Action(dateString != NULL, done, retval++);

        __CFTimeZoneExampleShow(CFSTR("%@ (%@)"), dateString, zoneName);

        intervalTz += _kSecondsPerHour;
        if (intervalTz > (_kSecondsPerDay / 2)) {
            intervalTz -= _kSecondsPerDay;
        }

        if (timeZone != NULL) {
            CFRelease(timeZone);
        }

        if (localeRef != NULL) {
            CFRelease(localeRef);
        }

        if (dateFormatter != NULL) {
            CFRelease(dateFormatter);
        }

        if (dateString != NULL) {
            CFRelease(dateString);
        }
    }

    __CFTimeZoneExampleShow(CFSTR("------------------------"));

done:
    if (curTz != NULL) {
        CFRelease(curTz);
    }

    return (retval);
#else
    return (0);
#endif
}

/* static */ CFDictionaryRef
_CFTimeZoneExampleCopyDict(CFTimeZoneRef tz) {
    static const double _kSecondsPerHour = 3600.0;
    CFMutableDictionaryRef   dict = NULL;
    CFAbsoluteTime           absT = CFAbsoluteTimeGetCurrent();
    CFTimeInterval           intervalF;
    Boolean                  is_dstB;
    CFNumberRef              numberRef = NULL;
    CFBooleanRef             boolRef = NULL;
    CFStringRef              str = NULL;

    dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                     0,
                                     &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    __Require(dict != NULL, done);

    str = CFTimeZoneGetName(tz);
    __Require(str != NULL, done);

    CFDictionarySetValue(dict, kCFTimeZoneDictKey_Name, str);

    str = CFTimeZoneCopyAbbreviation(tz, absT);
    __Require(str != NULL, done);

    CFDictionarySetValue(dict, kCFTimeZoneDictKey_Abbreviation, str);
    CFRelease(str);

    intervalF = CFTimeZoneGetSecondsFromGMT(tz, absT);
    intervalF /= _kSecondsPerHour;

    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &intervalF);
    __Require(numberRef != NULL, done);

    CFDictionarySetValue(dict, kCFTimeZoneDictKey_ZoneOffset, numberRef);
    CFRelease(numberRef);

    is_dstB = CFTimeZoneIsDaylightSavingTime(tz, absT);

    boolRef = (!!is_dstB ? kCFBooleanTrue : kCFBooleanFalse);

    CFDictionarySetValue(dict, kCFTimeZoneDictKey_IsDst, boolRef);

    str = CFTimeZoneCopyLocalizedName(tz,
                                      (is_dstB ? kCFTimeZoneNameStyleDaylightSaving : kCFTimeZoneNameStyleStandard),
                                      CFLocaleGetSystem());
    __Require(str != NULL, done);

    CFDictionarySetValue(dict, kCFTimeZoneDictKey_LocalizedName, str);
    CFRelease(str);

    intervalF = CFTimeZoneGetDaylightSavingTimeOffset(tz, absT);
    intervalF /= _kSecondsPerHour;

    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &intervalF);
    __Require(numberRef != NULL, done);

    CFDictionarySetValue(dict, kCFTimeZoneDictKey_DstOffset, numberRef);
    CFRelease(numberRef);

 done:
    return (dict);
}

/* static */ int
_CFTimeZoneExampleCreateTimeZones(void) {
    static const bool kTryAbbrev = true;
    CFStringRef tzStr = CFSTR("Europe/Helsinki");
    CFTimeZoneRef testTz = NULL;
    CFDictionaryRef testDict = NULL;
    int retval = 0;

    testTz = CFTimeZoneCreateWithName(kCFAllocatorDefault, tzStr, !kTryAbbrev);
    __Require_Action(testTz != NULL, done, retval++);

    retval += _CFTimeZoneExampleReport(CFSTR("Creating Time Zones"), testTz == NULL);

    if (testTz != NULL) {
        __CFTimeZoneExampleShow(testTz);

        testDict = _CFTimeZoneExampleCopyDict(testTz);
        __Require_Action(testDict != NULL, done, retval++);

        __CFTimeZoneExampleShow(testDict);

        __CFTimeZoneExampleShow(CFSTR("------------------------"));
    } else {
        __CFTimeZoneExampleShow(CFSTR("%@ doesn't exist"), tzStr);
    }

 done:
    if (testTz != NULL) {
        CFRelease(testTz);
    }

    return (retval);
}

/* static */ int
_CFTimeZoneExampleResetSystemAddExample(CFMutableArrayRef testArray,
                                        CFStringRef zoneName,
                                        CFStringRef displayName,
                                        CFStringRef zoneAbbrev,
                                        CFStringRef zoneDSTAbbrev,
                                        CFStringRef localizedName,
                                        CFStringRef localizedDSTName,
                                        double zoneOffset,
                                        double zoneDSTOffset) {
    CFMutableDictionaryRef tzDict = NULL;
    CFNumberRef numberRef = NULL;
    int retval = 0;

    tzDict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                       0,
                                       &kCFTypeDictionaryKeyCallBacks,
                                       &kCFTypeDictionaryValueCallBacks);
    __Require_Action(tzDict != NULL, done, retval++);


    CFDictionaryAddValue(tzDict, kCFTimeZoneDictKey_Name, zoneName);
    CFDictionaryAddValue(tzDict, kCFTimeZoneDictKey_DisplayName, displayName);
    CFDictionaryAddValue(tzDict, kCFTimeZoneDictKey_Abbreviation, zoneAbbrev);
    CFDictionaryAddValue(tzDict, kCFTimeZoneDictKey_Abbreviation_Dst,zoneDSTAbbrev);
    CFDictionaryAddValue(tzDict, kCFTimeZoneDictKey_LocalizedName, localizedName);
    CFDictionaryAddValue(tzDict, kCFTimeZoneDictKey_LocalizedName_Dst, localizedDSTName);

    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &zoneOffset);
    __Require_Action(numberRef != NULL, done, retval++);

    CFDictionaryAddValue(tzDict, kCFTimeZoneDictKey_ZoneOffset,	numberRef);
    CFRelease(numberRef);

    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &zoneDSTOffset);
    __Require_Action(numberRef != NULL, done, retval++);

    CFDictionaryAddValue(tzDict, kCFTimeZoneDictKey_DstOffset, numberRef);
    CFRelease(numberRef);

    CFArrayAppendValue(testArray, tzDict);

 done:
    if (tzDict != NULL) {
        CFRelease(tzDict);
    }

    return (retval);
}

/* static */ int
_CFTimeZoneExampleResetSystem(CFDictionaryRef testDict) {
    static const bool kTryAbbrev = true;
    CFStringRef     testZoneName = NULL;
    CFStringRef     testDisplayName = NULL;
    CFStringRef     testZoneAbbrev = NULL;
    CFStringRef     testZoneDSTAbbrev = NULL;
    CFStringRef     testLocalizedName = NULL;
    CFStringRef     testLocalizedDSTName = NULL;
    double          testZoneOffset;
    double          testZoneDSTOffset;
    CFStringRef     curZoneName = NULL;
    CFStringRef     curZoneAbbrev = NULL;
    CFStringRef     curLocalizedName = NULL;
    double          curZoneOffset;
    double          curZoneDSTOffset;
    CFBooleanRef    boolRef = NULL;
    CFNumberRef     numberRef = NULL;
    Boolean         success;
    bool            is_dst;
    CFTimeZoneRef   newTz = NULL;
    CFTimeZoneRef   curTz = NULL;
    CFDictionaryRef curTzDict = NULL;
    int             retval = 0;

    // Get the individual dictionary values from the test time zone
    // dictionary.

    testDisplayName = static_cast<CFStringRef>(CFDictionaryGetValue(testDict, kCFTimeZoneDictKey_DisplayName));
    __Require_Action(testDisplayName != NULL, done, retval++);

    testZoneName = static_cast<CFStringRef>(CFDictionaryGetValue(testDict, kCFTimeZoneDictKey_Name));
    __Require_Action(testZoneName != NULL, done, retval++);

    testZoneAbbrev = static_cast<CFStringRef>(CFDictionaryGetValue(testDict, kCFTimeZoneDictKey_Abbreviation));
    __Require_Action(testZoneAbbrev != NULL, done, retval++);

    testZoneDSTAbbrev = static_cast<CFStringRef>(CFDictionaryGetValue(testDict, kCFTimeZoneDictKey_Abbreviation_Dst));
    __Require_Action(testZoneDSTAbbrev != NULL, done, retval++);

    testLocalizedName = static_cast<CFStringRef>(CFDictionaryGetValue(testDict, kCFTimeZoneDictKey_LocalizedName));
    __Require_Action(testLocalizedName != NULL, done, retval++);

    testLocalizedDSTName = static_cast<CFStringRef>(CFDictionaryGetValue(testDict, kCFTimeZoneDictKey_LocalizedName_Dst));
    __Require_Action(testLocalizedDSTName != NULL, done, retval++);

    numberRef = static_cast<CFNumberRef>(CFDictionaryGetValue(testDict, kCFTimeZoneDictKey_ZoneOffset));
    __Require_Action(numberRef != NULL, done, retval++);

    success = CFNumberGetValue(numberRef, kCFNumberDoubleType, &testZoneOffset);
    __Require_Action(success, done, retval++);

    numberRef = static_cast<CFNumberRef>(CFDictionaryGetValue(testDict, kCFTimeZoneDictKey_DstOffset));
    __Require_Action(numberRef != NULL, done, retval++);

    success = CFNumberGetValue(numberRef, kCFNumberDoubleType, &testZoneDSTOffset);
    __Require_Action(success, done, retval++);

    // Allocate a new time zone with the test time zone name.

    newTz = CFTimeZoneCreateWithName(kCFAllocatorDefault, testZoneName, kTryAbbrev);
    __Require_Action(newTz != NULL, done, retval++);

    // Set that newly-created time zone as the default time zone.

    CFTimeZoneSetDefault(newTz);

    // Copy that newly-set time zone, assuming it has been
    // correctly-set as the default.

    curTz = CFTimeZoneCopyDefault();
    __Require_Action(curTz != NULL, done, retval++);

    // Create a dictionary from the newly-set default time zone.

    curTzDict = _CFTimeZoneExampleCopyDict(curTz);
    __Require_Action(curTzDict != NULL, done, retval++);

    // Get the individual dictionary values from the newly-set and
    // current default time zone dictionary.

    curZoneName = static_cast<CFStringRef>(CFDictionaryGetValue(curTzDict, kCFTimeZoneDictKey_Name));
    __Require_Action(curZoneName != NULL, done, retval++);

    numberRef = static_cast<CFNumberRef>(CFDictionaryGetValue(curTzDict, kCFTimeZoneDictKey_ZoneOffset));
    __Require_Action(numberRef != NULL, done, retval++);

    success = CFNumberGetValue(numberRef, kCFNumberDoubleType, &curZoneOffset);
    __Require_Action(success, done, retval++);

    numberRef = static_cast<CFNumberRef>(CFDictionaryGetValue(curTzDict, kCFTimeZoneDictKey_DstOffset));
    __Require_Action(numberRef != NULL, done, retval++);

    success = CFNumberGetValue(numberRef, kCFNumberDoubleType, &curZoneDSTOffset);
    __Require_Action(success, done, retval++);

    boolRef = static_cast<CFBooleanRef>(CFDictionaryGetValue(curTzDict, kCFTimeZoneDictKey_IsDst));
    __Require_Action(boolRef != NULL, done, retval++);

    is_dst = CFBooleanGetValue(boolRef);

    curZoneAbbrev = (is_dst ? testZoneDSTAbbrev : testZoneAbbrev);
    curLocalizedName = (is_dst ? testLocalizedDSTName : testLocalizedName);

    if (is_dst) {
        testZoneOffset += testZoneDSTOffset;
    } else {
        testZoneDSTOffset = 0;
    }

    __CFTimeZoneExampleShow(CFSTR("-------------------------------------------"));

    __CFTimeZoneExampleShow(CFSTR("%@ "), testDisplayName);

    __CFTimeZoneExampleShow(curTzDict);

    retval += _CFTimeZoneExampleReport(testZoneName, kCFTimeZoneDictKey_Name,		  !CFEqual(testZoneName, curZoneName));
    retval += _CFTimeZoneExampleReport(testZoneName, kCFTimeZoneDictKey_LocalizedName, !CFEqual(testLocalizedName, curLocalizedName));
    retval += _CFTimeZoneExampleReport(testZoneName, kCFTimeZoneDictKey_DstOffset,	  testZoneDSTOffset != curZoneDSTOffset);
    retval += _CFTimeZoneExampleReport(testZoneName, kCFTimeZoneDictKey_ZoneOffset,	  testZoneOffset != curZoneOffset);
    retval += _CFTimeZoneExampleReport(testZoneName, kCFTimeZoneDictKey_Abbreviation,  !CFEqual(testZoneAbbrev, curZoneAbbrev));

    // Return the system time zone setting back to its prior state.

    CFTimeZoneResetSystem();

 done:
    if (newTz != NULL) {
        CFRelease(newTz);
    }

    if (curTz != NULL) {
        CFRelease(curTz);
    }

    if (curTzDict != NULL) {
        CFRelease(curTzDict);
    }

    return (retval);
}

/* static */ int
_CFTimeZoneExampleResetSystem(CFArrayRef testArray) {
    CFIndex arrayCount;
    int retval = 0;

    __Require_Action(testArray != NULL, done, retval++);

    arrayCount = CFArrayGetCount(testArray);
    __Require_Action(arrayCount > 0, done, retval++);

    for (CFIndex i = 0; i < arrayCount; i++) {
        CFDictionaryRef testDict = NULL;

        testDict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(testArray, i));
        __Require_Action(testDict != NULL, done, retval++);

        retval += _CFTimeZoneExampleResetSystem(testDict);
    }

 done:
    return (retval);
}


/* static */ int
_CFTimeZoneExampleResetSystem(void) {
    CFMutableArrayRef testArray = NULL;
    int retval = 0;

    testArray = CFArrayCreateMutable(kCFAllocatorDefault,
                                     0,
                                     &kCFTypeArrayCallBacks);
    __Require_Action(testArray != NULL, done, retval++);

    //	to create a test dict:
    //
    //	set kCFTimeZoneDictKey_Abbreviation to STANDARD time abbrev
    //	set kCFTimeZoneDictKey_Abbreviation_Dst to DAYLIGHT time abbrev (both the same if no DST)
    //	set kCFTimeZoneDictKey_ZoneOffset to STANDARD time offset
    //	set kCFTimeZoneDictKey_DstOffset to DAYLIGHT offset

    // Test 1.1: America/Los_Angeles

    retval += _CFTimeZoneExampleResetSystemAddExample(testArray,
#if TARGET_OS_WIN32
                                                      CFSTR("Pacific Standard Time"),
                                                      CFSTR("Pacific Time (US & Canada)"),

#else
                                                      CFSTR("America/Los_Angeles"),
                                                      CFSTR("Los Angeles, CA - United States"),
#endif // TARGET_OS_WIN32
                                                      CFSTR("PST"),
                                                      CFSTR("PDT"),
                                                      CFSTR("GMT-08:00"),
                                                      CFSTR("GMT-07:00"),
                                                      -8.0,
                                                      1.0);
    __Require(retval == 0, done);

    // Test 1.2: America/Denver

    retval += _CFTimeZoneExampleResetSystemAddExample(testArray,
#if TARGET_OS_WIN32
                                                      CFSTR("Mountain Standard Time"),
                                                      CFSTR("Mountain Time (US & Canada)"),
#else
                                                      CFSTR("America/Denver"),
                                                      CFSTR("Colorado Springs, CO - United States"),
#endif // TARGET_OS_WIN32
                                                      CFSTR("MST"),
                                                      CFSTR("MDT"),
                                                      CFSTR("GMT-07:00"),
                                                      CFSTR("GMT-06:00"),
                                                      -7.0,
                                                      1.0);
    __Require(retval == 0, done);

    // Test 1.3: America/Phoenix

    retval += _CFTimeZoneExampleResetSystemAddExample(testArray,
#if TARGET_OS_WIN32
                                                      CFSTR("Arizona"),
                                                      CFSTR("GMT-0700"), //	value returned by CF476, may need update if newer CF
#else
                                                      CFSTR("America/Phoenix"),
                                                      CFSTR("Phoenix, AZ - United States"),
#endif // TARGET_OS_WIN32
                                                      CFSTR("MST"),
                                                      CFSTR("MST"),
                                                      CFSTR("GMT-07:00"),
                                                      CFSTR("GMT-07:00"),
                                                      -7.0,
                                                      0.0);
    __Require(retval == 0, done);

    // Test 1.4: Europe/Berlin

    retval += _CFTimeZoneExampleResetSystemAddExample(testArray,
#if TARGET_OS_WIN32
                                                      CFSTR("Amsterdam, Berlin, Bern, etc..."),
                                                      CFSTR("W. Europe Standard Time"),
#else
                                                      CFSTR("Europe/Berlin"),
                                                      CFSTR("Berlin - Germany"),
#endif // TARGET_OS_WIN32
                                                      CFSTR("GMT+1"),
                                                      CFSTR("GMT+2"),
                                                      CFSTR("GMT+01:00"),
                                                      CFSTR("GMT+02:00"),
                                                      1.0,
                                                      1.0);
    __Require(retval == 0, done);

    // Test 1.5: Europe/Belgrade

    retval += _CFTimeZoneExampleResetSystemAddExample(testArray,
#if TARGET_OS_WIN32
                                                      CFSTR("Belgrade, Bratislava, Budapest, etc..."),
                                                      CFSTR("Central Europe Standard Time"),
#else
                                                      CFSTR("Europe/Belgrade"),
                                                      CFSTR("Belgrade - Serbia"),
#endif // TARGET_OS_WIN32
                                                      CFSTR("GMT+1"),
                                                      CFSTR("GMT+2"),
                                                      CFSTR("GMT+01:00"),
                                                      CFSTR("GMT+02:00"),
                                                      1.0,
                                                      1.0);
    __Require(retval == 0, done);

    retval += _CFTimeZoneExampleResetSystem(testArray);

 done:
    if (testArray != NULL) {
        CFRelease(testArray);
    }

    return (retval);
}

int
main(void)
{
    int retval = 0;

    retval += _CFTimeZoneExampleDisplayKnownNames();
    retval += _CFTimeZoneExampleDisplayAbbreviations();
    retval += _CFTimeZoneExampleDisplayGMTOffsets();
    retval += _CFTimeZoneExampleCreateTimeZones();
    retval += _CFTimeZoneExampleResetSystem();

    return ((retval == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
}
