#import <UIKit/UIKit.h>

#include "../core/player_core.h"

@class AudioOutputController;

extern NSString* const RetroDAPRouteDidChangeNotification;

@interface RetroDAPAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow* window_;
    UITabBarController* tabController_;
    RDEngine* engine_;
    AudioOutputController* audioOutput_;
}

@property(nonatomic, retain) UIWindow* window;
@property(nonatomic, readonly) RDEngine* engine;
@property(nonatomic, readonly) AudioOutputController* audioOutput;

+ (NSString*)currentRouteDescription;

@end
