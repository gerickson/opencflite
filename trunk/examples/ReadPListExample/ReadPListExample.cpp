/*
 * Copyright (c) 2008-2009 Brent Fulgham.  All rights reserved.
 *
 * This source code is a modified version of the CoreFoundation sources released by Apple Inc. under
 * the terms of the APSL version 2.0 (see below).
 *
 * For information about changes from the original Apple source release can be found by reviewing the
 * source control system for the project at https://sourceforge.net/svn/?group_id=246198.
 *
 * The original license information is as follows:
 *
 */

//
// Apple's "Read a PList" example program.
// Taken from http://developer.apple.com/opensource/cflite.html
//

#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>

static void readPropertyListFromFile (const char *path) {
    CFDataRef data = NULL;
		
    FILE* file = fopen (path, "r");

	if (file == NULL) {
		fprintf(stderr, "Cannot open `%s' for reading.\n", path);
	} else {
        int result = fseek (file, 0, SEEK_END);
        result = ftell (file);
        rewind (file);

        char* buffer = (char*)calloc (1, result);

        if (buffer != NULL) {
            int rc = (int)fread (buffer, result, 1, file);
            if (rc > 0 || !ferror (file)) {
                data = CFDataCreate (NULL, (const UInt8*)buffer, result);
            }

            free (buffer);
        } 

        fclose (file);
    }

    if (data != NULL) {
        CFPropertyListRef propertyList = CFPropertyListCreateFromXMLData (NULL, data, kCFPropertyListImmutable, NULL);

        CFShow (CFSTR ("Property list (as read from file):"));
        CFShow (propertyList);

		CFRelease(data);
    }
}

int main (int argc, const char* argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <Property List>\n", argv[0]);
		return 1;
	}

    readPropertyListFromFile (argv[1]);	

    return 0;
}
