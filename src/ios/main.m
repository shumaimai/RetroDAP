#import <UIKit/UIKit.h>

#import "RetroDAPAppDelegate.h"

int main(int argc, char* argv[]) {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    int result = UIApplicationMain(argc, argv, nil, @"RetroDAPAppDelegate");
    [pool drain];
    return result;
}
