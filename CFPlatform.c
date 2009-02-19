/*
 * Copyright (c) 2008-2009 Brent Fulgham <bfulgham@gmail.org>.  All rights reserved.
 * Copyright (c) 2009 Grant Erickson <gerickson@nuovations.com>. All rights reserved.
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
/*	CFPlatform.c
	Copyright 1999-2002, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include "CFInternal.h"
#include "CFPriv.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#if DEPLOYMENT_TARGET_WINDOWS
/* In typical fragile fashion, order of include on Windows is important */
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <tchar.h>
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif
#define strdup _strdup
#include <shlobj.h>
extern size_t strlcpy(char *dst, const char *src, size_t siz);
extern size_t strlcat(char *dst, const char *src, size_t siz);
#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0
#endif
#else
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#endif
#if DEPLOYMENT_TARGET_MACOSX
#include <mach-o/dyld.h>
#include <crt_externs.h>
#endif

#if DEPLOYMENT_TARGET_MACOSX
#define kCFPlatformInterfaceStringEncoding	kCFStringEncodingUTF8
#else
#define kCFPlatformInterfaceStringEncoding	CFStringGetSystemEncoding()
#endif

static CFStringRef _CFUserName(void);

#if DEPLOYMENT_TARGET_MACOSX
// CoreGraphics and LaunchServices are only projects (1 Dec 2006) that use these
char **_CFArgv(void) { return *_NSGetArgv(); }
int _CFArgc(void) { return *_NSGetArgc(); }
#endif


__private_extern__ Boolean _CFGetCurrentDirectory(char *path, int maxlen) {
#if DEPLOYMENT_TARGET_WINDOWS || 0
    DWORD len = GetCurrentDirectoryA(maxlen, path);
    return ((0 != len) && (maxlen > 0) && (len + 1 <= (DWORD)maxlen));
#else
    return getcwd(path, maxlen) != NULL;
#endif
}

static Boolean __CFIsCFM = false;

// If called super early, we just return false
__private_extern__ Boolean _CFIsCFM(void) {
    return __CFIsCFM;
}

#if DEPLOYMENT_TARGET_WINDOWS || 0
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#if DEPLOYMENT_TARGET_WINDOWS
#define PATH_LIST_SEP ';'
#else
#define PATH_LIST_SEP ':'
#endif

static char *_CFSearchForNameInPath(const char *name, char *path) {
    struct stat statbuf;
#if DEPLOYMENT_TARGET_WINDOWS && !defined(__GNUC__)
    char nname[MAX_PATH + 1];
#else
    char nname[strlen(name) + strlen(path) + 2];
#endif
#if !DEPLOYMENT_TARGET_WINDOWS
    int no_hang_fd = open("/dev/autofs_nowait", 0);
#endif
    for (;;) {
        char *p = (char *)strchr(path, PATH_LIST_SEP);
        if (NULL != p) {
            *p = '\0';
        }
        nname[0] = '\0';
        strlcat(nname, path, sizeof(nname));
        strlcat(nname, "/", sizeof(nname));
        strlcat(nname, name, sizeof(nname));
        // Could also do access(us, X_OK) == 0 in next condition,
        // for executable-only searching
        if (0 == stat(nname, &statbuf) && (statbuf.st_mode & S_IFMT) == S_IFREG) {
            if (p != NULL) {
                *p = PATH_LIST_SEP;
            }
#if !DEPLOYMENT_TARGET_WINDOWS
           close(no_hang_fd);
#endif
           return strdup(nname);
        }
        if (NULL == p) {
            break;
        }
        *p = PATH_LIST_SEP;
        path = p + 1;
    }
#if !DEPLOYMENT_TARGET_WINDOWS
    close(no_hang_fd);
#endif
    return NULL;
}

#if DEPLOYMENT_TARGET_WINDOWS
// Returns the path to the CF DLL, which we can then use to find resources like char sets

__private_extern__ const char* _CFDLLPath(void) {
    static char cachedPath[MAX_PATH+1] = "";

    if ('\0' == cachedPath[0]) {
#if defined(DEBUG)
        char* DLLFileName = "CoreFoundation_debug";
#elif defined(PROFILE)
        char* DLLFileName = "CoreFoundation_profile";
#else
        char* DLLFileName = "CoreFoundation";
#endif
        HMODULE ourModule = GetModuleHandleA(DLLFileName);
        CFAssert(ourModule, __kCFLogAssertion, "GetModuleHandle failed");

        DWORD wResult = GetModuleFileNameA(ourModule, cachedPath, MAX_PATH+1);
        CFAssert1(wResult > 0, __kCFLogAssertion, "GetModuleFileName failed: %d", GetLastError());
        CFAssert1(wResult < MAX_PATH+1, __kCFLogAssertion, "GetModuleFileName result truncated: %s", cachedPath);

        // strip off last component, the DLL name
        CFIndex idx;
        for (idx = wResult - 1; idx; idx--) {
            if ('\\' == cachedPath[idx]) {
                cachedPath[idx] = '\0';
                break;
            }
        }
    }
    return cachedPath;
}
#endif

static const char *__CFProcessPath = NULL;
static const char *__CFprogname = NULL;

const char **_CFGetProgname(void) {
    if (!__CFprogname)
        _CFProcessPath();		// sets up __CFprogname as a side-effect
    return &__CFprogname;
}

const char **_CFGetProcessPath(void) {
    if (!__CFProcessPath)
        _CFProcessPath();		// sets up __CFProcessPath as a side-effect
    return &__CFProcessPath;
}

const char *_CFProcessPath(void) {
    if (__CFProcessPath) return __CFProcessPath;

    char *thePath = NULL;
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_LINUX
	if (!_CFIsSetUgid()) {
#else
    if (!__CFProcessPath) {
#endif
        thePath = getenv("CFProcessPath");
        if (thePath) {
	         int len = (int)strlen(thePath);
            __CFProcessPath = (const char *)CFAllocatorAllocate(kCFAllocatorSystemDefault, len+1, 0);
            if (__CFOASafe) __CFSetLastAllocationEventName((void *)__CFProcessPath, "CFUtilities (process-path)");
            memmove((char *)__CFProcessPath, thePath, len + 1);
        }
    }
#if DEPLOYMENT_TARGET_WINDOWS
    if (!__CFProcessPath) {
        char buf[CFMaxPathSize] = {0};
        HINSTANCE hinst = GetModuleHandle(NULL);
        DWORD rlen = hinst ? GetModuleFileNameA(hinst, buf, CFMaxPathSize) : 0;
	     thePath = rlen ? buf : NULL;
#elif DEPLOYMENT_TARGET_MACOSX
    int execIndex = 0;
    
    if (!__CFProcessPath && NULL != (*_NSGetArgv())[execIndex]) {
        int no_hang_fd = open("/dev/autofs_nowait", 0);
        char buf[CFMaxPathSize] = {0};
        struct stat statbuf;
        const char *arg0 = (*_NSGetArgv())[execIndex];
        if (arg0[0] == '/') {
            // We've got an absolute path; look no further;
            thePath = (char *)arg0;
        } else {
            char *theList = getenv("PATH");
            if (NULL != theList && NULL == strrchr(arg0, '/')) {
                thePath = _CFSearchForNameInPath(arg0, theList);
                if (thePath) {
                    // User could have "." or "../bin" or other relative path in $PATH
                    if (('/' != thePath[0]) && _CFGetCurrentDirectory(buf, CFMaxPathSize)) {
                        strlcat(buf, "/", sizeof(buf));
                        strlcat(buf, thePath, sizeof(buf));
                        if (0 == stat(buf, &statbuf)) {
                            free(thePath);
                            thePath = buf;
                        }
                    }
                    if (thePath != buf) {
                        strlcpy(buf, thePath, sizeof(buf));
                        free((void *)thePath);
                        thePath = buf;
                    }
                }
            }
        }
        
        // After attempting a search through $PATH, if existant,
        // try prepending the current directory to argv[0].
        if (!thePath && _CFGetCurrentDirectory(buf, CFMaxPathSize)) {
            if (buf[strlen(buf)-1] != '/') {
                strlcat(buf, "/", sizeof(buf));
            }
            strlcat(buf, arg0, CFMaxPathSize);
            if (0 == stat(buf, &statbuf)) {
                thePath = buf;
            }
        }

        if (thePath) {
            // We are going to process the buffer replacing all "/./" and "//" with "/"
            CFIndex srcIndex = 0, dstIndex = 0;
            CFIndex len = strlen(thePath);
            for (srcIndex=0; srcIndex<len; srcIndex++) {
                thePath[dstIndex] = thePath[srcIndex];
                dstIndex++;
                while (srcIndex < len-1 && thePath[srcIndex] == '/' && (thePath[srcIndex+1] == '/' || (thePath[srcIndex+1] == '.' && srcIndex < len-2 && thePath[srcIndex+2] == '/'))) srcIndex += (thePath[srcIndex+1] == '/' ? 1 : 2);
            }
            thePath[dstIndex] = 0;
        }
        if (!thePath) {
            thePath = (*_NSGetArgv())[execIndex];
        }
#elif DEPLOYMENT_TARGET_LINUX
	if (!__CFProcessPath) {
		pid_t pid = getpid();
		char buf[2][CFMaxPathSize] = {{0}, {0}};
		int n;

		n = snprintf(buf[0], CFMaxPathSize, "/proc/%u/exe", pid);

		if (n > 0) {
			n = readlink(buf[0], buf[1], CFMaxPathLength);
			if (n > 0 && n <= CFMaxPathLength) {
				buf[1][n] = '\0';
				thePath = buf[1];
			}
		}
#else
#error "Don't know how to compute the process path for this platform."
#endif

        if (thePath) {
            int len = (int)strlen(thePath);
            __CFProcessPath = (const char *)CFAllocatorAllocate(kCFAllocatorSystemDefault, len + 1, 0);
            if (__CFOASafe) __CFSetLastAllocationEventName((void *)__CFProcessPath, "CFUtilities (process-path)");
            memmove((char *)__CFProcessPath, thePath, len + 1);
        }
        if (__CFProcessPath) {            
            const char *p = 0;
            int i;
            for (i = 0; __CFProcessPath[i] != 0; i++){
               if (__CFProcessPath[i] == PATH_SEP)
                    p = __CFProcessPath + i; 
            }
            if (p != 0)
                __CFprogname = p + 1;
            else
                __CFprogname = __CFProcessPath;
        }

#if DEPLOYMENT_TARGET_MACOSX
        close(no_hang_fd);
#endif

    }

    if (!__CFProcessPath) {
        __CFProcessPath = "";
        __CFprogname = __CFProcessPath;
    } else {
        const char *p = 0;
        int i;
        for (i = 0; __CFProcessPath[i] != 0; i++){
            if (__CFProcessPath[i] == PATH_SEP)
                p = __CFProcessPath + i; 
        }
        if (p != 0)
            __CFprogname = p + 1;
        else
            __CFprogname = __CFProcessPath;
    }
    return __CFProcessPath;
}

__private_extern__ CFStringRef _CFProcessNameString(void) {
    static CFStringRef __CFProcessNameString = NULL;
    if (!__CFProcessNameString) {
        const char *processName = *_CFGetProgname();
        if (!processName) processName = "";
        __CFProcessNameString = CFStringCreateWithCString(__CFGetDefaultAllocator(), processName, kCFPlatformInterfaceStringEncoding);
    }
    return __CFProcessNameString;
}

static CFStringRef __CFUserName = NULL;

#if (DEPLOYMENT_TARGET_MACOSX) || defined(__svr4__) || defined(__hpux__) || DEPLOYMENT_TARGET_LINUX || DEPLOYMENT_TARGET_FREEBSD
static CFURLRef __CFHomeDirectory = NULL;
static uint32_t __CFEUID = -1;
static uint32_t __CFUID = -1;

static CFURLRef _CFCopyHomeDirURLForUser(struct passwd *upwd) {
    CFURLRef home = NULL;
    if (!_CFIsSetUgid()) {
	const char *path = getenv("CFFIXED_USER_HOME");
	if (path) {
	    home = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)path, strlen(path), true);
	}
    }
    if (!home) {
        if (upwd && upwd->pw_dir) {
            home = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)upwd->pw_dir, strlen(upwd->pw_dir), true);
	}
    }
    return home;
}

static void _CFUpdateUserInfo(void) {
    struct passwd *upwd;

    __CFEUID = geteuid();
    __CFUID = getuid();
    if (__CFHomeDirectory)  CFRelease(__CFHomeDirectory);
    __CFHomeDirectory = NULL;
    if (__CFUserName) CFRelease(__CFUserName);
    __CFUserName = NULL;

    upwd = getpwuid(__CFEUID ? __CFEUID : __CFUID);
    __CFHomeDirectory = _CFCopyHomeDirURLForUser(upwd);
    if (!__CFHomeDirectory) {
        const char *cpath = getenv("HOME");
        if (cpath) {
            __CFHomeDirectory = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)cpath, strlen(cpath), true);
        }
    }

    // This implies that UserManager stores directory info in CString
    // rather than FileSystemRep.  Perhaps this is wrong & we should
    // expect NeXTSTEP encodings.  A great test of our localized system would
    // be to have a user "O-umlat z e r".  XXX
    if (upwd && upwd->pw_name) {
        __CFUserName = CFStringCreateWithCString(kCFAllocatorSystemDefault, upwd->pw_name, kCFPlatformInterfaceStringEncoding);
    } else {
        const char *cuser = getenv("USER");
        if (cuser)
            __CFUserName = CFStringCreateWithCString(kCFAllocatorSystemDefault, cuser, kCFPlatformInterfaceStringEncoding);
    }
}
#endif

static CFURLRef _CFCreateHomeDirectoryURLForUser(CFStringRef uName) {
#if (DEPLOYMENT_TARGET_MACOSX) || defined(__svr4__) || defined(__hpux__) || DEPLOYMENT_TARGET_LINUX || DEPLOYMENT_TARGET_FREEBSD
    if (!uName) {
        if (geteuid() != __CFEUID || getuid() != __CFUID || !__CFHomeDirectory)
            _CFUpdateUserInfo();
        if (__CFHomeDirectory) CFRetain(__CFHomeDirectory);
        return __CFHomeDirectory;
    } else {
        struct passwd *upwd = NULL;
        char buf[128], *user;
        SInt32 len = CFStringGetLength(uName), size = CFStringGetMaximumSizeForEncoding(len, kCFPlatformInterfaceStringEncoding);
        CFIndex usedSize;
        if (size < 127) {
            user = buf;
        } else {
            user = (char*)CFAllocatorAllocate(kCFAllocatorSystemDefault, size+1, 0);
            if (__CFOASafe) __CFSetLastAllocationEventName(user, "CFUtilities (temp)");
        }
        if (CFStringGetBytes(uName, CFRangeMake(0, len), kCFPlatformInterfaceStringEncoding, 0, true, (uint8_t *)user, size, &usedSize) == len) {
            user[usedSize] = '\0';
            upwd = getpwnam(user);
        }
        if (buf != user) {
            CFAllocatorDeallocate(kCFAllocatorSystemDefault, user);
        }
        return _CFCopyHomeDirURLForUser(upwd);
    }
#elif DEPLOYMENT_TARGET_WINDOWS
    CFStringRef user = !uName ? _CFUserName() : uName;
    CFURLRef home = NULL;

    if (!uName || CFEqual(user, _CFUserName())) {
        const char *cpath = getenv("HOMEPATH");
        const char *cdrive = getenv("HOMEDRIVE");
        if (cdrive && cpath) {
            char fullPath[CFMaxPathSize];
            CFStringRef str;
            strlcpy(fullPath, cdrive, sizeof(fullPath));
            strlcat(fullPath, cpath, sizeof(fullPath));
            str = CFStringCreateWithCString(kCFAllocatorSystemDefault, fullPath, kCFPlatformInterfaceStringEncoding);
            home = CFURLCreateWithFileSystemPath(kCFAllocatorSystemDefault, str, kCFURLWindowsPathStyle, true);
            CFRelease(str);
        }
    }
    if (home == NULL) {
		UniChar pathChars[MAX_PATH];
		if (S_OK == SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, (LPWSTR) pathChars)) {
			UniChar* p = pathChars;
			CFIndex len = 0;
			CFStringRef str;
			while (*p++ != 0)
				++len;
			str = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, pathChars, len);
            home = CFURLCreateWithFileSystemPath(kCFAllocatorSystemDefault, str, kCFURLWindowsPathStyle, true);
            CFRelease(str);
                } else {
                    // We have to get "some" directory location, so fall-back to the 
                    // processes current directory.
                    UniChar currDir[MAX_PATH];
                    DWORD dwChars = GetCurrentDirectory(MAX_PATH + 1, (LPWSTR)currDir);
                    if (dwChars > 0) {
                        UniChar* p = currDir;
			            CFIndex len = 0;
			            CFStringRef str;
			            while (*p++ != 0)
				            ++len;
			            str = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, currDir, len);
                        home = CFURLCreateWithFileSystemPath(kCFAllocatorSystemDefault, str, kCFURLWindowsPathStyle, true);
                    }
                }
    }
    // We could do more here (as in KB Article Q101507). If that article is to
    // be believed, we should only run into this case on Win95, or through
    // user error.
    if (home) {
        CFStringRef str = CFURLCopyFileSystemPath(home, kCFURLWindowsPathStyle);
        if (str && CFStringGetLength(str) == 0) {
            CFRelease(home);
            home=NULL;
        }
        if (str) CFRelease(str);
    }
    return home;
#else
#error Dont know how to compute users home directories on this platform
#endif
}

static CFStringRef _CFUserName(void) {
#if (DEPLOYMENT_TARGET_MACOSX) || defined(__svr4__) || defined(__hpux__) || DEPLOYMENT_TARGET_LINUX || DEPLOYMENT_TARGET_FREEBSD
    if (geteuid() != __CFEUID || getuid() != __CFUID)
	_CFUpdateUserInfo();
#elif DEPLOYMENT_TARGET_WINDOWS
    if (!__CFUserName) {
	char username[1040];
	DWORD size = 1040;
	username[0] = 0;
	if (GetUserNameA(username, &size)) {
            __CFUserName = CFStringCreateWithCString(kCFAllocatorSystemDefault, username, kCFPlatformInterfaceStringEncoding);
	} else {
	    const char *cname = getenv("USERNAME");
	    if (cname)
                __CFUserName = CFStringCreateWithCString(kCFAllocatorSystemDefault, cname, kCFPlatformInterfaceStringEncoding);
	}
    }
#else
#error Dont know how to compute user name on this platform
#endif
    if (!__CFUserName)
        __CFUserName = (CFStringRef)CFRetain(CFSTR(""));
    return __CFUserName;
}

__private_extern__ CFStringRef _CFGetUserName(void) {
    return CFStringCreateCopy(kCFAllocatorSystemDefault, _CFUserName());
}

#define CFMaxHostNameLength	256
#define CFMaxHostNameSize	(CFMaxHostNameLength+1)

__private_extern__ CFStringRef _CFStringCreateHostName(void) {
    char myName[CFMaxHostNameSize];

    // return @"" instead of nil a la CFUserName() and Ali Ozer
    if (0 != gethostname(myName, CFMaxHostNameSize)) myName[0] = '\0';
    return CFStringCreateWithCString(kCFAllocatorSystemDefault, myName, kCFPlatformInterfaceStringEncoding);
}

/* These are sanitized versions of the above functions. We might want to eliminate the above ones someday.
   These can return NULL.
*/
CF_EXPORT CFStringRef CFGetUserName(void) {
    return _CFUserName();
}

CF_EXPORT CFURLRef CFCopyHomeDirectoryURLForUser(CFStringRef uName) {
    return _CFCreateHomeDirectoryURLForUser(uName);
}

#undef CFMaxHostNameLength
#undef CFMaxHostNameSize

