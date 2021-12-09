/*
 * Copyright (c) 2021 Grant Erickson. All rights reserved.
 * Copyright (c) 2011 Brent Fulgham <bfulgham@gmail.org>.  All rights reserved.
 *
 */

/**
 *   @file
 *     This file implements a test application to confirm that
 *     CoreFoundation bundle functions work properly.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <AssertMacros.h>

#include <CoreFoundation/CoreFoundation.h>

#if !defined(LOG_CFBUNDLEEXAMPLE)
#define LOG_CFBUNDLEEXAMPLE             0
#endif

#define __CFBundleExampleLog(format, ...)       do { fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr); } while (0)

#if LOG_CFBUNDLEEXAMPLE
#define __CFBundleExampleMaybeLog(format, ...)                           \
    __CFBundleExampleLog(format, ##__VA_ARGS__)
#else
#define __CFBundleExampleMaybeLog(format, ...)
#endif

#define __CFBundleExampleMaybeTraceWithFormat(dir, name, format, ...)    \
    __CFBundleExampleMaybeLog(dir " %s" format, name, ##__VA_ARGS__)
#define __CFBundleExampleTraceEnterWithFormat(format, ...)               \
    __CFBundleExampleMaybeTraceWithFormat("-->", __func__, " " format, ##__VA_ARGS__)
#define __CFBundleExampleTraceExitWithFormat(format, ...)                \
    __CFBundleExampleMaybeTraceWithFormat("<--", __func__, " " format, ##__VA_ARGS__)
#define __CFBundleExampleTraceEnter()                                    \
    __CFBundleExampleTraceEnterWithFormat("\n")
#define __CFBundleExampleTraceExit()                                     \
    __CFBundleExampleTraceExitWithFormat("\n")

// This function will print the provided arguments (printf style
// varargs) out to the console.
//
// Note that the CFString formatting function accepts "%@" as a way to
// display CF types.
//
// For types other than CFString and CFNumber, the result of %@ is
// mostly for debugging and can differ between releases and different
// platforms.
static void show(CFStringRef formatString, ...) {
    CFStringRef resultString;
    CFDataRef data;
    va_list argList;

    va_start(argList, formatString);
    resultString = CFStringCreateWithFormatAndArguments(NULL, NULL, formatString, argList);
    va_end(argList);

    data = CFStringCreateExternalRepresentation(NULL, resultString, CFStringGetSystemEncoding(), '?');

    if (data != NULL) {
        __CFBundleExampleLog("%.*s\n", (int)CFDataGetLength(data), CFDataGetBytePtr(data));
        CFRelease(data);
    }

    CFRelease(resultString);
}

static void dumpBundleDirectoryAndRelease(CFURLRef urlRef, CFStringRef dirDescription) {
    __Require(urlRef != 0, done);
    __Require(dirDescription != 0, done);

    show(CFSTR("%@ directory URL: %@"), dirDescription, urlRef);

    CFRelease(urlRef);

 done:
    return;
}

static void dumpBundleContents(CFBundleRef bundleRef) {
    CFURLRef        bundleUrlRef = 0;
    CFDictionaryRef dictRef      = 0;
    CFStringRef     stringRef    = 0;
    CFArrayRef      arrayRef     = 0;
    CFArrayRef      arrayRef2    = 0;

    long longResult = 0;

    __Require(bundleRef != 0, done);

    stringRef = CFBundleGetIdentifier(bundleRef);
    show(CFSTR("Bundle identifier: %@"), stringRef);

    longResult = CFBundleGetVersionNumber(bundleRef);
    show(CFSTR("Version: %d"), longResult);

    bundleUrlRef = CFBundleCopyBundleURL(bundleRef);
    __Require(bundleUrlRef != 0, done);

    show(CFSTR("Bundle location (as URL): %@"), bundleUrlRef);
    CFRelease(bundleUrlRef);
    bundleUrlRef = 0;

    dictRef = CFBundleGetInfoDictionary(bundleRef);
    __Require(dictRef != 0, done);

    show(CFSTR("Main CFBundle Dictionary: %@:"), dictRef);

    stringRef = CFBundleGetDevelopmentRegion(bundleRef);
    show(CFSTR("Development region: %@"), stringRef);

    bundleUrlRef = CFBundleCopySupportFilesDirectoryURL(bundleRef);
    __Require(bundleUrlRef != 0, done);

    dumpBundleDirectoryAndRelease(bundleUrlRef, CFSTR("Support files"));

    bundleUrlRef = CFBundleCopyResourcesDirectoryURL(bundleRef);
    __Require(bundleUrlRef != 0, done);

    dumpBundleDirectoryAndRelease(bundleUrlRef, CFSTR("Resources"));

    bundleUrlRef = CFBundleCopyPrivateFrameworksURL(bundleRef);
    __Require(bundleUrlRef != 0, done);

    dumpBundleDirectoryAndRelease(bundleUrlRef, CFSTR("Private frameworks"));

    bundleUrlRef = CFBundleCopySharedFrameworksURL(bundleRef);
    __Require(bundleUrlRef != 0, done);

    dumpBundleDirectoryAndRelease(bundleUrlRef, CFSTR("Shared frameworks"));

    bundleUrlRef = CFBundleCopySharedSupportURL(bundleRef);
    __Require(bundleUrlRef != 0, done);

    dumpBundleDirectoryAndRelease(bundleUrlRef, CFSTR("Shared support"));

    bundleUrlRef = CFBundleCopyBuiltInPlugInsURL(bundleRef);
    __Require(bundleUrlRef != 0, done);

    dumpBundleDirectoryAndRelease(bundleUrlRef, CFSTR("Built-in Plugins"));

    arrayRef = CFBundleCopyBundleLocalizations(bundleRef);
    if (!arrayRef)
    {
        show(CFSTR("CFBundleCopyBundleLocalizations(): No localizations present."));
    }
    else
    {
       show(CFSTR("Bundle Localizations: %@"), arrayRef);

       arrayRef2 = CFBundleCopyPreferredLocalizationsFromArray(arrayRef);

       if (!arrayRef2)
           show(CFSTR("CFBundleCopyPreferredLocalizationsFromArray(): No PREFERRED localizations."));
       else {
           show(CFSTR("PREFERRED Bundle Localizations: %@"), arrayRef2);
           CFRelease(arrayRef2);
           arrayRef2 = 0;
       }

       CFRelease(arrayRef);
       arrayRef = 0;
    }

    arrayRef = CFBundleCopyExecutableArchitectures(bundleRef);
    if (!arrayRef)
    {
        show(CFSTR("CFBundleCopyExecutableArchitectures(): No architectures defined."));
    }
    else
    {
        show(CFSTR("Executable architectures: %@"), arrayRef);
        CFRelease(arrayRef);
        arrayRef = 0;
    }

 done:
    return;
}

#if defined(WIN32)
static void safariBundleExample(void) {
    static const char * const safariBundlePath64 = "C:\\Program Files (x86)\\Safari\\Safari.resources";
    static const char* const  safariBundlePath   = "C:\\Program Files\\Safari\\Safari.resources";
    CFBundleRef               safariBundleRef    = 0;
    CFURLRef                  safariPathRef      = 0;

    __CFBundleExampleLog("3. Safari Bundle:\n");

    // Try inspecting Safari.

    safariPathRef = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, safariBundlePath, (CFIndex)strlen (safariBundlePath), false);
    __Require(safariPathRef != 0, done);

    show(CFSTR("Try Safari Path (32-bit OS): %@"), safariPathRef);

    safariBundleRef = CFBundleCreate (kCFAllocatorDefault, safariPathRef);
    CFRelease(safariPathRef);
    if (!safariBundleRef)
    {
        safariPathRef = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, safariBundlePath64, (CFIndex)strlen (safariBundlePath64), false);
        show(CFSTR("Try Safari Path (64-bit OS): %@"), safariPathRef);
        __Require(safariPathRef != 0, done);
        safariBundleRef = CFBundleCreate (kCFAllocatorDefault, safariPathRef);
        CFRelease(safariPathRef);
    }

    __Require(safariBundleRef != 0, done);

    dumpBundleContents(safariBundleRef);
    CFRelease(safariBundleRef);

 done:
    return;
}
#else
#define safariBundleExample() do { ; } while (0)
#endif

int main (int argc, const char *argv[]) {
    const char *cfLiteBundlePathString = 0;
    CFStringRef cfLiteBundlePath       = 0;
    CFBundleRef bundleRef              = 0;
    CFBundleRef cfLiteBundleRef        = 0;
    CFURLRef    bundleUrlRef           = 0;
    CFURLRef    cfLiteURLRef           = 0;
    int         retval                 = EXIT_FAILURE;

    __Require_Action(argc <= 2,
                     done,
                     __CFBundleExampleLog("Usage: %s [ <CoreFoundation bundle path> ]\n",
                                          argv[0]));

    if (argc == 2) {
        cfLiteBundlePathString = argv[1];

    } else {
        cfLiteBundlePathString = "./CFLite.resources";

    }

    cfLiteBundlePath = CFStringCreateWithCString(kCFAllocatorSystemDefault,
                                                 cfLiteBundlePathString,
                                                 kCFStringEncodingUTF8);
    __Require(cfLiteBundlePath != 0, done);

    // 1. Use bundle routines on this application.

    __CFBundleExampleLog("1. Test Application Bundle:\n");

    bundleRef = CFBundleGetMainBundle();
    __Require(bundleRef != 0, done);

    dumpBundleContents(bundleRef);

    __CFBundleExampleLog("\n\n");

    // 2. Inspect our CFLite bundle.

    __CFBundleExampleLog("2. CFLite Bundle:\n");

    bundleUrlRef = CFBundleCopyBundleURL(bundleRef);
    __Require(bundleUrlRef != 0, done);

    cfLiteURLRef = CFURLCreateWithFileSystemPathRelativeToBase(kCFAllocatorDefault, cfLiteBundlePath, kCFURLPOSIXPathStyle, false, bundleUrlRef);
    CFRelease(bundleUrlRef);

    show(CFSTR("CFLite path: %@"), cfLiteURLRef);

    cfLiteBundleRef = CFBundleCreate (kCFAllocatorDefault, cfLiteURLRef);
    __Require(cfLiteBundleRef != 0, done);

    dumpBundleContents(cfLiteBundleRef);
    CFRelease(cfLiteBundleRef);

    safariBundleExample();

    retval = EXIT_SUCCESS;

 done:
    if (cfLiteBundlePath != 0) {
        CFRelease(cfLiteBundlePath);
    }

    if (cfLiteURLRef != 0) {
        CFRelease(cfLiteURLRef);
    }

    return retval;
}


