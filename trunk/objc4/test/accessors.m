#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include "test.h"

extern id objc_getProperty(id self, SEL _cmd, ptrdiff_t offset, BOOL atomic);
extern void objc_setProperty(id self, SEL _cmd, ptrdiff_t offset, id newValue, BOOL atomic, BOOL shouldCopy);

@interface Test : NSObject {
    NSString *_value;
    // _object is at the last optimized property offset
    id _object __attribute__((aligned(64)));
}
@property(readonly) Class class;
@property(copy) NSString *value;
@property(assign) id object;
@end

typedef struct {  
    id isa;
    NSString *_value;
    // _object is at the last optimized property offset
    id _object __attribute__((aligned(64)));
} TestDefs;

@implementation Test

// Question:  why can't this code be automatically generated?

- (void)dealloc {
    self.value = nil;
    self.object = nil;
    [super dealloc];
}

- (Class)class { return objc_getProperty(self, _cmd, 0, YES); }

- (NSString*)value { return (NSString*) objc_getProperty(self, _cmd, offsetof(TestDefs, _value), YES); }
- (void)setValue:(NSString*)inValue { objc_setProperty(self, _cmd, offsetof(TestDefs, _value), inValue, YES, YES); }

- (id)object { return objc_getProperty(self, _cmd, offsetof(TestDefs, _object), YES); }
- (void)setObject:(id)inObject { objc_setProperty(self, _cmd, offsetof(TestDefs, _object), inObject, YES, NO); }

- (NSString *)description {
    return [NSString stringWithFormat:@"value = %@, object = %@", self.value, self.object];
}

@end

int main() {
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    
    NSMutableString *value = [NSMutableString stringWithUTF8String:"test"];
    id object = [NSNumber numberWithInt:11];
    Test *t = [[Test new] autorelease];
    t.value = value;
    [value setString:@"yuck"];      // mutate the string.
    testassert(t.value != value);   // must copy, since it was mutable.
    testassert([t.value isEqualToString:@"test"]);

    Class testClass = [Test class];
    Class cls = t.class;
    testassert(testClass == cls);
    cls = t.class;
    testassert(testClass == cls);

    t.object = object;
    t.object = object;

    // NSLog(@"t.object = %@, t.value = %@", t.object, t.value);
    // NSLog(@"t.object = %@, t.value = %@", t.object, t.value); // second call will optimized getters.
    
    [pool drain];

    succeed(__FILE__);

    return 0;
}
