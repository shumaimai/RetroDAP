#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>

#include "../core/player_core.h"

@interface AudioOutputController : NSObject {
    RDEngine* engine_;
    AudioUnit audioUnit_;
    unsigned int currentSampleRate_;
    unsigned int lastPreferredSampleRate_;
    BOOL started_;
}

- (id)initWithEngine:(RDEngine*)engine;
- (BOOL)start;
- (void)stop;
- (void)syncSampleRate;

@end
