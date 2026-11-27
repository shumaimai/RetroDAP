#import "RetroDAPAppDelegate.h"

#import <AVFoundation/AVFoundation.h>

#import "AudioOutputController.h"
#import "LibraryViewController.h"
#import "NowPlayingViewController.h"

NSString* const RetroDAPRouteDidChangeNotification = @"RetroDAPRouteDidChangeNotification";

@implementation RetroDAPAppDelegate

@synthesize window = window_;
@synthesize engine = engine_;
@synthesize audioOutput = audioOutput_;

+ (NSString*)currentRouteDescription {
    AVAudioSessionRouteDescription* route = [[AVAudioSession sharedInstance] currentRoute];
    if (route == nil || [route.outputs count] == 0) {
        return @"Speaker";
    }
    AVAudioSessionPortDescription* output = [route.outputs objectAtIndex:0];
    if ([output.portType isEqualToString:AVAudioSessionPortLineOut] ||
        [output.portType isEqualToString:AVAudioSessionPortUSBAudio]) {
        return @"Dock Audio";
    }
    if ([output.portType isEqualToString:AVAudioSessionPortHeadphones]) {
        return @"Headphones";
    }
    if ([output.portType isEqualToString:AVAudioSessionPortBuiltInSpeaker]) {
        return @"Speaker";
    }
    return output.portName != nil ? output.portName : @"Audio Output";
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [audioOutput_ release];
    if (engine_ != NULL) {
        rdap_engine_destroy(engine_);
    }
    [tabController_ release];
    [window_ release];
    [super dealloc];
}

- (void)configureAudioSession {
    NSError* error = nil;
    AVAudioSession* session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback error:&error];
    [session setPreferredIOBufferDuration:0.02 error:&error];
    [session setActive:YES error:&error];
}

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    (void)launchOptions;
    [self configureAudioSession];

    engine_ = rdap_engine_create(1024 * 1024);
    audioOutput_ = [[AudioOutputController alloc] initWithEngine:engine_];
    [audioOutput_ syncSampleRate];
    [audioOutput_ start];

    NowPlayingViewController* nowPlaying = [[[NowPlayingViewController alloc] initWithEngine:engine_ audioOutput:audioOutput_] autorelease];
    nowPlaying.tabBarItem = [[[UITabBarItem alloc] initWithTitle:@"Playing" image:nil tag:0] autorelease];

    LibraryViewController* library = [[[LibraryViewController alloc] initWithEngine:engine_] autorelease];
    UINavigationController* libraryNavigation = [[[UINavigationController alloc] initWithRootViewController:library] autorelease];
    libraryNavigation.tabBarItem = [[[UITabBarItem alloc] initWithTitle:@"Library" image:nil tag:1] autorelease];
    libraryNavigation.navigationBar.barStyle = UIBarStyleBlack;

    tabController_ = [[UITabBarController alloc] init];
    tabController_.viewControllers = [NSArray arrayWithObjects:nowPlaying, libraryNavigation, nil];

    window_ = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    window_.backgroundColor = [UIColor blackColor];
    window_.rootViewController = tabController_;
    [window_ makeKeyAndVisible];

    [application beginReceivingRemoteControlEvents];

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(routeChanged:) name:AVAudioSessionRouteChangeNotification object:nil];
    [center addObserver:self selector:@selector(interruptionChanged:) name:AVAudioSessionInterruptionNotification object:nil];
    return YES;
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
    (void)application;
    [self configureAudioSession];
    [audioOutput_ syncSampleRate];
    [audioOutput_ start];
}

- (void)applicationWillResignActive:(UIApplication*)application {
    (void)application;
}

- (void)routeChanged:(NSNotification*)notification {
    NSNumber* reason = [[notification userInfo] objectForKey:AVAudioSessionRouteChangeReasonKey];
    if (reason != nil && [reason unsignedIntegerValue] == AVAudioSessionRouteChangeReasonOldDeviceUnavailable) {
        rdap_engine_pause(engine_);
    }
    [audioOutput_ syncSampleRate];
    [[NSNotificationCenter defaultCenter] postNotificationName:RetroDAPRouteDidChangeNotification object:nil];
}

- (void)interruptionChanged:(NSNotification*)notification {
    NSNumber* type = [[notification userInfo] objectForKey:AVAudioSessionInterruptionTypeKey];
    if (type != nil && [type unsignedIntegerValue] == AVAudioSessionInterruptionTypeBegan) {
        rdap_engine_pause(engine_);
    } else if (type != nil && [type unsignedIntegerValue] == AVAudioSessionInterruptionTypeEnded) {
        [self configureAudioSession];
        [audioOutput_ start];
    }
}

@end
