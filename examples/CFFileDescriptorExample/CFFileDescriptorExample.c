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
 *     This file implements a "toy" demonstration of the
 *     CoreFoundation CFFileDescriptor object by effecting a Fibonacci
 *     sequence computation engine between a writer and reader
 *     callback across a POSIX pipe where the writer sends the current
 *     state over the pipe and the reader computes it with thre writer
 *     and reader each "signalling" one another for next iteration
 *     readiness through callback enablement.
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include <AssertMacros.h>

#include <CoreFoundation/CoreFoundation.h>


/* Preprocessor Definitions */

#define kFibonacciStateInitializer { 0, 1 }

/* Type Definitions */

enum {
    kReaderIndex = 0,
    kWriterIndex = 1
};

typedef struct {
    size_t               mValues[2];
} State;

typedef struct {
    State                mState;
    size_t               mLimit;
    bool                 mStop;
    CFFileDescriptorRef  mDescriptors[2];
} Context;

/* Function Prototypes */

static void DescriptorReadCallBack(CFFileDescriptorRef aDescriptorRef, CFOptionFlags aCallBackTypes, void *aContext);
static void DescriptorWriteCallBack(CFFileDescriptorRef aDescriptorRef, CFOptionFlags aCallBackTypes, void *aContext);

/* Global Variables */

static const int kDescriptorInvalid = -1;
static const int kStatusFailure     = -1;


void
DescriptorReadCallBack(CFFileDescriptorRef aDescriptorRef, CFOptionFlags aCallBackTypes, void *aContext)
{
    Context *    lContext = (Context *)(aContext);
    Boolean      lIsValid;
    int          lDescriptor;
    State        lState;
    const size_t kExpectedSize = sizeof (lState);
    size_t       lNextSum;
    int          lStatus;


    lIsValid = CFFileDescriptorIsValid(aDescriptorRef);
    __Require(lIsValid, done);

    lDescriptor = CFFileDescriptorGetNativeDescriptor(aDescriptorRef);
    __Require(lDescriptor != kDescriptorInvalid, done);

    lStatus = read(lDescriptor, &lState, kExpectedSize);
    __Require(lStatus == kExpectedSize, done);

    lNextSum = lState.mValues[0] + lState.mValues[1];

    if (lNextSum > lContext->mLimit)
    {
        lContext->mStop = true;
    }
    else
    {
        lContext->mState.mValues[0] = lState.mValues[1];
        lContext->mState.mValues[1] = lNextSum;

        // Print the next sequence value.

        printf("%zu\n", lNextSum);
    }

    // Unconditionally enable the writer callback, which acts as the
    // initiator, so it can determine what to do next.

    CFFileDescriptorEnableCallBacks(lContext->mDescriptors[kWriterIndex], kCFFileDescriptorWriteCallBack);

 done:
    return;
}

void
DescriptorWriteCallBack(CFFileDescriptorRef aDescriptorRef, CFOptionFlags aCallBackTypes, void *aContext)
{
    Context *    lContext = (Context *)(aContext);
    Boolean      lIsValid;
    int          lDescriptor;
    const size_t lSize = sizeof(lContext->mState);
    int          lStatus;


    // If we have not yet reached the desired termination limit,
    // signal the writer by sending the state to process.

    if (!lContext->mStop)
    {
        lIsValid = CFFileDescriptorIsValid(aDescriptorRef);
        __Require(lIsValid, done);

        lDescriptor = CFFileDescriptorGetNativeDescriptor(aDescriptorRef);
        __Require(lDescriptor != kDescriptorInvalid, done);

        lStatus = write(lDescriptor, &lContext->mState, lSize);
        __Require(lStatus == lSize, done);

        // Enable the reader callback, which acts as the responder,
        // for the reader descriptor so it can process the state the
        // writer just pushed.

        CFFileDescriptorEnableCallBacks(lContext->mDescriptors[kReaderIndex], kCFFileDescriptorReadCallBack);

    } else {
        CFRunLoopStop(CFRunLoopGetCurrent());

    }

 done:
    return;
}

int
main(int argc, const char * const argv[])
{
    static const Boolean    kCloseOnInvalidate    = true;
    char *                  end                   = NULL;
    Context                 ourContext            = { { kFibonacciStateInitializer }, 0, false, { NULL, NULL } };
    CFFileDescriptorContext fileDescriptorContext = { 0, &ourContext, NULL, NULL, NULL };
    int                     fds[2]                = { kDescriptorInvalid, kDescriptorInvalid };
    CFRunLoopSourceRef      sources[2]            = { NULL, NULL };
    int                     status;


    // Perform a basic sanity check on usage: at least one timer and a limit.

    __Require_Action(argc < 3, done, status = kStatusFailure; fprintf(stderr, "Usage: %s iterations\n", argv[0]));

    // Convert and check the specified iteration limit value.

    ourContext.mLimit = strtoul(argv[argc - 1], &end, 10);
    __Require_Action(errno != ERANGE, exit, status = kStatusFailure);

    if (errno == ERANGE || ourContext.mLimit == 0) {
        status = kStatusFailure;
        fprintf(stderr, "The iterations must be greater than zero.\n");
        goto exit;
    }

    // Create the pipe

    status = pipe(&fds[0]);
    __Require(status == 0, exit);

    // Create CoreFoundation descriptor objects from the pipe

    ourContext.mDescriptors[kReaderIndex] = CFFileDescriptorCreate(kCFAllocatorDefault,
                                                                   fds[kReaderIndex],
                                                                   !kCloseOnInvalidate,
                                                                   DescriptorReadCallBack,
                                                                   &fileDescriptorContext);
    __Require_Action(ourContext.mDescriptors[kReaderIndex] != NULL, done, status = kStatusFailure);

    ourContext.mDescriptors[kWriterIndex] = CFFileDescriptorCreate(kCFAllocatorDefault,
                                                                   fds[kWriterIndex],
                                                                   !kCloseOnInvalidate,
                                                                   DescriptorWriteCallBack,
                                                                   &fileDescriptorContext);
    __Require_Action(ourContext.mDescriptors[kWriterIndex] != NULL, done, status = kStatusFailure);

    // Create CoreFoundation run loop sources from the descriptors.

    sources[kReaderIndex] = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault,
                                                                ourContext.mDescriptors[kReaderIndex],
                                                                0);
    __Require_Action(sources[kReaderIndex] != NULL, done, status = kStatusFailure);

    sources[kWriterIndex] = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault,
                                                                ourContext.mDescriptors[kWriterIndex],
                                                                0);
    __Require_Action(sources[kWriterIndex] != NULL, done, status = kStatusFailure);

    // Start by printing the first two sequence values.

    printf("%zu\n%zu\n", ourContext.mState.mValues[0], ourContext.mState.mValues[1]);

    // Let the writer run first by enabling its callback. Callbacks
    // are one-shot and will be further enabled in the callbacks
    // themselves.

    CFFileDescriptorEnableCallBacks(ourContext.mDescriptors[kWriterIndex], kCFFileDescriptorWriteCallBack);

    // Add the writer and reader run loop sources.

    CFRunLoopAddSource(CFRunLoopGetCurrent(), sources[kReaderIndex], kCFRunLoopDefaultMode);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), sources[kWriterIndex], kCFRunLoopDefaultMode);

    // Run the writer and reader sources.

    CFRunLoopRun();

 done:
    CFFileDescriptorDisableCallBacks(ourContext.mDescriptors[kWriterIndex], kCFFileDescriptorWriteCallBack);
    CFFileDescriptorDisableCallBacks(ourContext.mDescriptors[kReaderIndex], kCFFileDescriptorReadCallBack);

    if (sources[kWriterIndex] != NULL)
    {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), sources[kWriterIndex], kCFRunLoopDefaultMode);
        CFRelease(sources[kWriterIndex]);
    }

    if (sources[kReaderIndex] != NULL)
    {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), sources[kReaderIndex], kCFRunLoopDefaultMode);
        CFRelease(sources[kReaderIndex]);
    }

    if (ourContext.mDescriptors[kWriterIndex] != NULL)
    {
        CFFileDescriptorInvalidate(ourContext.mDescriptors[kWriterIndex]);
        CFRelease(ourContext.mDescriptors[kWriterIndex]);
    }

    if (ourContext.mDescriptors[kReaderIndex] != NULL)
    {
        CFFileDescriptorInvalidate(ourContext.mDescriptors[kReaderIndex]);
        CFRelease(ourContext.mDescriptors[kReaderIndex]);
    }

    if (fds[kWriterIndex] != kDescriptorInvalid)
    {
        close(fds[kWriterIndex]);
    }

    if (fds[kReaderIndex] != kDescriptorInvalid)
    {
        close(fds[kReaderIndex]);
    }

 exit:
    return ((status == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
}



