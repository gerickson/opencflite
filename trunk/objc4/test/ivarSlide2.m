#include "test.h"
#include <string.h>
#include <stdint.h>
#include <objc/objc-runtime.h>
#include <objc/objc-auto.h>
#include <auto_zone.h>

#define OLD 1
#include "ivarSlide.h"

#ifdef __cplusplus
class CXX {
 public:
    static uintptr_t count;
    uintptr_t magic;
    CXX() : magic(1) { } 
    ~CXX() { count += magic; }
};
uintptr_t CXX::count;
#endif

@interface Bitfields : Super {
    uint8_t uint8_ivar;
    uint8_t uint8_bitfield1 :7;
    uint8_t uint8_bitfield2 :1;

    id id_ivar;
 
    uintptr_t uintptr_ivar;
    uintptr_t /*uintptr_bitfield1*/ :31;  // anonymous (rdar://5723893)
    uintptr_t uintptr_bitfield2 :1;

    id id_ivar2;
}
@end

@implementation Bitfields @end


@interface Sub : Super {
  @public 
    uintptr_t subIvar;
    __strong uintptr_t subIvar2;
    __weak uintptr_t subIvar3;
#ifdef __cplusplus
    CXX cxx;
#else
    // same layout as cxx
    uintptr_t cxx_magic;
#endif
}
@end

@implementation Sub @end


@interface Sub2 : ShrinkingSuper {
  @public 
    __weak uintptr_t subIvar;
    __strong uintptr_t subIvar2;
}
@end

@implementation Sub2 @end

@interface MoreStrongSub : MoreStrongSuper { id subIvar; } @end
@interface LessStrongSub : LessStrongSuper { id subIvar; } @end
@interface MoreWeakSub : MoreWeakSuper { id subIvar; }  @end
@interface MoreWeak2Sub : MoreWeak2Super { id subIvar; }  @end
@interface LessWeakSub : LessWeakSuper { id subIvar; }  @end
@interface LessWeak2Sub : LessWeak2Super { id subIvar; }  @end

@implementation MoreStrongSub @end
@implementation LessStrongSub @end
@implementation MoreWeakSub @end
@implementation MoreWeak2Sub @end
@implementation LessWeakSub @end
@implementation LessWeak2Sub @end

@interface NoGCChangeSub : NoGCChangeSuper { 
  @public
    char subc3; 
} 
@end
@implementation NoGCChangeSub @end

@interface RunsOf15Sub : RunsOf15 { 
  @public
    char sub; 
} 
@end
@implementation RunsOf15Sub @end


int main(int argc __attribute__((unused)), char **argv)
{
    if (objc_collecting_enabled()) {
        objc_startCollectorThread();
    }

#if __OBJC2__

    /* 
       Bitfield ivars.
       rdar://5723893 anonymous bitfield ivars crash when slid
       rdar://5724385 bitfield ivar alignment incorrect

       Compile-time layout of Bitfields: 
         [0 scan] isa
         [1 skip] uint8_ivar, uint8_bitfield
         [2 scan] id_ivar
         [3 skip] uintptr_ivar
         [4 skip] uintptr_bitfield
         [5 scan] id_ivar2

       Runtime layout of Bitfields:
         [0 scan] isa
         [1 skip] superIvar
         [2 skip] uint8_ivar, uint8_bitfield
         [3 scan] id_ivar
         [4 skip] uintptr_ivar
         [5 skip] uintptr_bitfield
         [6 scan] id_ivar2
    */

    [Bitfields class];

    testassert(class_getInstanceSize([Bitfields class]) == 7*sizeof(void*));

    if (objc_collecting_enabled()) {
        const char *bitfieldlayout;
        bitfieldlayout = class_getIvarLayout([Bitfields class]);
        testassert(0 == strcmp(bitfieldlayout, "\x01\x21\x21"));

        bitfieldlayout = class_getWeakIvarLayout([Bitfields class]);
        testassert(bitfieldlayout == NULL);
    }

    /* 
       Compile-time layout of Sub: 
         [0 scan] isa
         [1 skip] subIvar
         [2 scan] subIvar2
         [3 weak] subIvar3
         [6 skip] cxx

       Runtime layout of Sub:
         [0 scan] isa
         [1 skip] superIvar
         [2 skip] subIvar
         [3 scan] subIvar2
         [4 weak] subIvar3
         [6 skip] cxx

       Also, superIvar is only one byte, so subIvar's alignment must 
       be handled correctly.

       fixme test more layouts
    */

    Ivar ivar;
    static Sub * volatile sub;
    sub = [Sub new];
    sub->subIvar = 10;
    testassert(((uintptr_t *)sub)[2] == 10);

#ifdef __cplusplus
    uintptr_t i;
    for (i = 0; i < 10; i++) {
        if (i != 0) sub = [Sub new];
        testassert(((uintptr_t *)sub)[5] == 1);
        testassert(sub->cxx.magic == 1);
        sub->cxx.magic++;
        testassert(((uintptr_t *)sub)[5] == 2);
        testassert(sub->cxx.magic == 2);
        if (! objc_collecting_enabled()) {
            [sub dealloc];
        }
    }

    if (objc_collecting_enabled()) {
        // only require one of the objects to be reclaimed
        objc_collect(OBJC_EXHAUSTIVE_COLLECTION | OBJC_WAIT_UNTIL_DONE);
        objc_collect(OBJC_EXHAUSTIVE_COLLECTION | OBJC_WAIT_UNTIL_DONE);
        testassert(CXX::count > 0  &&  CXX::count <= i*2  &&  CXX::count % 2 == 0);
    }
    else {
        testassert(CXX::count == i*2);
    }
#endif


    testassert(class_getInstanceSize([Sub class]) == 6*sizeof(void*));

    ivar = class_getInstanceVariable([Sub class], "subIvar");
    testassert(ivar);
    testassert(2*sizeof(void*) == ivar_getOffset(ivar));
    testassert(0 == strcmp(ivar_getName(ivar), "subIvar"));
    testassert(0 == strcmp(ivar_getTypeEncoding(ivar), 
#if __LP64__
                           "Q"
#else
                           "I"
#endif
                           ));

#ifdef __cplusplus
    ivar = class_getInstanceVariable([Sub class], "cxx");
    testassert(ivar);
#endif

    ivar = class_getInstanceVariable([Super class], "superIvar");
    testassert(ivar);
    testassert(sizeof(void*) == ivar_getOffset(ivar));
    testassert(0 == strcmp(ivar_getName(ivar), "superIvar"));
    testassert(0 == strcmp(ivar_getTypeEncoding(ivar), "c"));

    ivar = class_getInstanceVariable([Super class], "subIvar");
    testassert(!ivar);

    if (objc_collecting_enabled()) {
        const char *superlayout;
        const char *sublayout;
        superlayout = class_getIvarLayout([Super class]);
        sublayout = class_getIvarLayout([Sub class]);
        testassert(0 == strcmp(superlayout, "\x01\x10"));
        testassert(0 == strcmp(sublayout, "\x01\x21\x20"));

        superlayout = class_getWeakIvarLayout([Super class]);
        sublayout = class_getWeakIvarLayout([Sub class]);
        testassert(superlayout == NULL);
        testassert(0 == strcmp(sublayout, "\x41\x10"));
    }

    /* 
       Shrinking superclass.
       Subclass ivars do not compact, but the GC layout needs to 
       update, including the gap that the superclass no longer spans.

       Compile-time layout of Sub2: 
         [0 scan] isa
         [1-5 scan] superIvar
         [6-10 weak] superIvar2
         [11 weak] subIvar
         [12 scan] subIvar2

       Runtime layout of Sub2:
         [0 scan] isa
         [1-10 skip] was superIvar
         [11 weak] subIvar
         [12 scan] subIvar2
    */

    Sub2 *sub2 = [Sub2 new];
    sub2->isa = [Sub2 class];
    sub2->subIvar = 10;
    testassert(((uintptr_t *)sub2)[11] == 10);

    testassert(class_getInstanceSize([Sub2 class]) == 13*sizeof(void*));

    ivar = class_getInstanceVariable([Sub2 class], "subIvar");
    testassert(ivar);
    testassert(11*sizeof(void*) == ivar_getOffset(ivar));
    testassert(0 == strcmp(ivar_getName(ivar), "subIvar"));

    ivar = class_getInstanceVariable([ShrinkingSuper class], "superIvar");
    testassert(!ivar);

    if (objc_collecting_enabled()) {
        const char *superlayout;
        const char *sublayout;
        superlayout = class_getIvarLayout([ShrinkingSuper class]);
        sublayout = class_getIvarLayout([Sub2 class]);
        testassert(superlayout == NULL);
        testassert(0 == strcmp(sublayout, "\x01\xb1"));

        superlayout = class_getWeakIvarLayout([ShrinkingSuper class]);
        sublayout = class_getWeakIvarLayout([Sub2 class]);
        testassert(superlayout == NULL);
        testassert(0 == strcmp(sublayout, "\xb1\x10"));
    }

    /*
       Ivars slide but GC layouts stay the same
       Here, the last word of the superclass is misaligned, but 
       its GC layout includes a bit for that whole word. 
       Additionally, all of the subclass ivars fit into that word too, 
       both before and after sliding. 
       The runtime will try to slide the GC layout and must not be 
       confused (rdar://6851700). Note that the second skip-word may or may 
       not actually be included, because it crosses the end of the object.
       

       Compile-time layout of NoGCChangeSub: 
         [0 scan] isa
         [1 skip] d
         [2 skip] superc1, subc3

       Runtime layout of NoGCChangeSub:
         [0 scan] isa
         [1 skip] d
         [2 skip] superc1, superc2, subc3
    */
    if (objc_collecting_enabled()) {
        Ivar ivar1 = class_getInstanceVariable([NoGCChangeSub class], "superc1");
        testassert(ivar1);
        Ivar ivar2 = class_getInstanceVariable([NoGCChangeSub class], "superc2");
        testassert(ivar2);
        Ivar ivar3 = class_getInstanceVariable([NoGCChangeSub class], "subc3");
        testassert(ivar3);
        testassert(ivar_getOffset(ivar1) != ivar_getOffset(ivar2)  &&  
                   ivar_getOffset(ivar1) != ivar_getOffset(ivar3)  &&  
                   ivar_getOffset(ivar2) != ivar_getOffset(ivar3));
    }

    /* Ivar layout includes runs of 15 words.
       rdar://6859875 this would generate a truncated GC layout.
    */
    if (objc_collecting_enabled()) {
        const uint8_t *layout = (const uint8_t *)
            class_getIvarLayout(objc_getClass("RunsOf15Sub"));
        testassert(layout);
        int totalSkip = 0;
        int totalScan = 0;
        // should find 30+ each of skip and scan
        uint8_t c;
        while ((c = *layout++)) {
            totalSkip += c>>4;
            totalScan += c&0xf;
        }
        testassert(totalSkip >= 30);
        testassert(totalScan >= 30);
    }

// __OBJC2__
#endif


    /* 
       Non-strong -> strong
       Classes do not change size, but GC layouts must be updated.
       Both new and old ABI detect this case (rdar://5774578)

       Compile-time layout of MoreStrongSub: 
         [0 scan] isa
         [1 skip] superIvar
         [2 scan] subIvar

       Runtime layout of MoreStrongSub:
         [0 scan] isa
         [1 scan] superIvar
         [2 scan] subIvar
    */
    testassert(class_getInstanceSize([MoreStrongSub class]) == 3*sizeof(void*));
    if (objc_collecting_enabled()) {
        const char *layout;
        layout = class_getIvarLayout([MoreStrongSub class]);
        testassert(layout == NULL);

        layout = class_getWeakIvarLayout([MoreStrongSub class]);
        testassert(layout == NULL);
    }


    /*
       Strong -> weak
       Classes do not change size, but GC layouts must be updated.
       Old ABI intentionally does not detect this case (rdar://5774578)
       
       Compile-time layout of MoreWeakSub: 
         [0 scan] isa
         [1 scan] superIvar
         [2 scan] subIvar

       Runtime layout of MoreWeakSub:
         [0 scan] isa
         [1 weak] superIvar
         [2 scan] subIvar
    */
    testassert(class_getInstanceSize([MoreWeakSub class]) == 3*sizeof(void*));
    if (objc_collecting_enabled()) {
        const char *layout;
        layout = class_getIvarLayout([MoreWeakSub class]);
#if __OBJC2__
        testassert(0 == strcmp(layout, "\x01\x11"));
#else
        testassert(layout == NULL);
#endif

        layout = class_getWeakIvarLayout([MoreWeakSub class]);
#if __OBJC2__
        testassert(0 == strcmp(layout, "\x11\x10"));
#else
        testassert(layout == NULL);
#endif
    }


    /*
       Non-strong -> weak
       Classes do not change size, but GC layouts must be updated.
       Old ABI intentionally does not detect this case (rdar://5774578)
       
       Compile-time layout of MoreWeak2Sub: 
         [0 scan] isa
         [1 skip] superIvar
         [2 scan] subIvar

       Runtime layout of MoreWeak2Sub:
         [0 scan] isa
         [1 weak] superIvar
         [2 scan] subIvar
    */
    testassert(class_getInstanceSize([MoreWeak2Sub class]) == 3*sizeof(void*));
    if (objc_collecting_enabled()) {
        const char *layout;
        layout = class_getIvarLayout([MoreWeak2Sub class]);
        testassert(0 == strcmp(layout, "\x01\x11")  ||
                   0 == strcmp(layout, "\x01\x10\x01"));

        layout = class_getWeakIvarLayout([MoreWeak2Sub class]);
#if __OBJC2__
        testassert(0 == strcmp(layout, "\x11\x10"));
#else
        testassert(layout == NULL);
#endif
    }


    /*
       Strong -> non-strong
       Classes do not change size, but GC layouts must be updated.
       Old ABI intentionally does not detect this case (rdar://5774578)

       Compile-time layout of LessStrongSub: 
         [0 scan] isa
         [1 scan] superIvar
         [2 scan] subIvar

       Runtime layout of LessStrongSub:
         [0 scan] isa
         [1 skip] superIvar
         [2 scan] subIvar
    */
    testassert(class_getInstanceSize([LessStrongSub class]) == 3*sizeof(void*));
    if (objc_collecting_enabled()) {
        const char *layout;
        layout = class_getIvarLayout([LessStrongSub class]);
#if __OBJC2__
        testassert(0 == strcmp(layout, "\x01\x11"));
#else
        testassert(layout == NULL);
#endif

        layout = class_getWeakIvarLayout([LessStrongSub class]);
        testassert(layout == NULL);
    }


    /*
       Weak -> strong
       Classes do not change size, but GC layouts must be updated.
       Both new and old ABI detect this case (rdar://5774578 rdar://6924114)
       
       Compile-time layout of LessWeakSub: 
         [0 scan] isa
         [1 weak] superIvar
         [2 scan] subIvar

       Runtime layout of LessWeakSub:
         [0 scan] isa
         [1 scan] superIvar
         [2 scan] subIvar
    */
    testassert(class_getInstanceSize([LessWeakSub class]) == 3*sizeof(void*));
    if (objc_collecting_enabled()) {
        const char *layout;
        layout = class_getIvarLayout([LessWeakSub class]);
        testassert(layout == NULL);

        layout = class_getWeakIvarLayout([LessWeakSub class]);
        testassert(layout == NULL);
    }


    /*
       Weak -> non-strong
       Classes do not change size, but GC layouts must be updated.
       Old ABI intentionally does not detect this case (rdar://5774578)
       
       Compile-time layout of LessWeak2Sub: 
         [0 scan] isa
         [1 weak] superIvar
         [2 scan] subIvar

       Runtime layout of LessWeak2Sub:
         [0 scan] isa
         [1 skip] superIvar
         [2 scan] subIvar
    */
    testassert(class_getInstanceSize([LessWeak2Sub class]) == 3*sizeof(void*));
    if (objc_collecting_enabled()) {
        const char *layout;
        layout = class_getIvarLayout([LessWeak2Sub class]);
        testassert(0 == strcmp(layout, "\x01\x11")  ||
                   0 == strcmp(layout, "\x01\x10\x01"));

        layout = class_getWeakIvarLayout([LessWeak2Sub class]);
#if __OBJC2__
        testassert(layout == NULL);
#else
        testassert(0 == strcmp(layout, "\x11\x10"));
#endif
    }


    succeed(basename(argv[0]));
    return 0;
}
