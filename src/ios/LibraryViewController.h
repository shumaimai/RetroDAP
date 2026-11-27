#import <UIKit/UIKit.h>

#include "../core/player_core.h"

@interface LibraryViewController : UITableViewController {
    RDEngine* engine_;
    RDLibrary* library_;
    BOOL scanning_;
}

- (id)initWithEngine:(RDEngine*)engine;
- (void)rescanLibrary;

@end
