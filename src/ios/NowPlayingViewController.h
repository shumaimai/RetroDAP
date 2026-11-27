#import <UIKit/UIKit.h>

#include "../core/player_core.h"

@class AudioOutputController;

@interface NowPlayingViewController : UIViewController {
    RDEngine* engine_;
    AudioOutputController* audioOutput_;

    UIImageView* artworkView_;
    UILabel* titleLabel_;
    UILabel* artistLabel_;
    UILabel* albumLabel_;
    UILabel* formatLabel_;
    UILabel* elapsedLabel_;
    UILabel* remainingLabel_;
    UILabel* routeLabel_;
    UISlider* seekSlider_;
    UIButton* playButton_;

    NSTimer* updateTimer_;
    NSCache* artworkCache_;
    NSString* currentPath_;
    BOOL scrubbing_;
}

- (id)initWithEngine:(RDEngine*)engine audioOutput:(AudioOutputController*)audioOutput;

@end
