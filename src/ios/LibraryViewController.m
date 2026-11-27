#import "LibraryViewController.h"

#import <stdlib.h>

static NSString* const RetroDAPLibraryRoot = @"/var/mobile/Media/FLAC";

@implementation LibraryViewController

- (id)initWithEngine:(RDEngine*)engine {
    self = [super initWithStyle:UITableViewStylePlain];
    if (self != nil) {
        engine_ = engine;
        library_ = NULL;
        scanning_ = NO;
        self.title = @"Library";
    }
    return self;
}

- (void)dealloc {
    if (library_ != NULL) {
        rdap_library_destroy(library_);
    }
    [super dealloc];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor blackColor];
    self.tableView.backgroundColor = [UIColor blackColor];
    self.tableView.separatorColor = [UIColor colorWithWhite:0.18 alpha:1.0];
    self.navigationItem.rightBarButtonItem = [[[UIBarButtonItem alloc] initWithTitle:@"Rescan" style:UIBarButtonItemStyleBordered target:self action:@selector(rescanLibrary)] autorelease];
    [self rescanLibrary];
}

- (void)rescanLibrary {
    if (scanning_) {
        return;
    }
    scanning_ = YES;
    self.navigationItem.rightBarButtonItem.enabled = NO;
    [self performSelectorInBackground:@selector(scanThread) withObject:nil];
}

- (void)scanThread {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    RDLibrary* scanned = rdap_library_scan([RetroDAPLibraryRoot UTF8String]);
    [self performSelectorOnMainThread:@selector(scanFinished:) withObject:[NSValue valueWithPointer:scanned] waitUntilDone:NO];
    [pool drain];
}

- (void)scanFinished:(NSValue*)value {
    RDLibrary* scanned = (RDLibrary*)[value pointerValue];
    if (library_ != NULL) {
        rdap_library_destroy(library_);
    }
    library_ = scanned;
    scanning_ = NO;
    self.navigationItem.rightBarButtonItem.enabled = YES;
    [self.tableView reloadData];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    [self becomeFirstResponder];
}

- (BOOL)canBecomeFirstResponder {
    return YES;
}

- (void)remoteControlReceivedWithEvent:(UIEvent*)event {
    if (event == nil || event.type != UIEventTypeRemoteControl) {
        return;
    }
    switch (event.subtype) {
        case UIEventSubtypeRemoteControlPlay:
            rdap_engine_resume(engine_);
            break;
        case UIEventSubtypeRemoteControlPause:
            rdap_engine_pause(engine_);
            break;
        case UIEventSubtypeRemoteControlTogglePlayPause:
            if (rdap_engine_is_playing(engine_)) {
                rdap_engine_pause(engine_);
            } else {
                rdap_engine_resume(engine_);
            }
            break;
        case UIEventSubtypeRemoteControlNextTrack:
            rdap_engine_next(engine_);
            break;
        case UIEventSubtypeRemoteControlPreviousTrack:
            rdap_engine_previous(engine_);
            break;
        default:
            break;
    }
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
    (void)tableView;
    return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section {
    (void)tableView;
    (void)section;
    if (scanning_) {
        return 1;
    }
    size_t count = rdap_library_count(library_);
    return count == 0 ? 1 : (NSInteger)count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath {
    static NSString* identifier = @"TrackCell";
    UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:identifier];
    if (cell == nil) {
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:identifier] autorelease];
        cell.backgroundColor = [UIColor blackColor];
        cell.textLabel.textColor = [UIColor whiteColor];
        cell.detailTextLabel.textColor = [UIColor colorWithWhite:0.65 alpha:1.0];
    }

    if (scanning_) {
        cell.textLabel.text = @"Scanning FLAC library...";
        cell.detailTextLabel.text = RetroDAPLibraryRoot;
        cell.selectionStyle = UITableViewCellSelectionStyleNone;
        return cell;
    }

    size_t count = rdap_library_count(library_);
    if (count == 0) {
        cell.textLabel.text = @"No FLAC files";
        cell.detailTextLabel.text = @"Copy files under /var/mobile/Media/FLAC/";
        cell.selectionStyle = UITableViewCellSelectionStyleNone;
        return cell;
    }

    RDTrackInfo info;
    if (rdap_library_info(library_, (size_t)indexPath.row, &info)) {
        NSString* title = info.title[0] != '\0' ? [NSString stringWithUTF8String:info.title] : [[NSString stringWithUTF8String:info.path] lastPathComponent];
        NSString* artist = info.artist[0] != '\0' ? [NSString stringWithUTF8String:info.artist] : @"Unknown Artist";
        NSString* album = info.album[0] != '\0' ? [NSString stringWithUTF8String:info.album] : @"Unknown Album";
        cell.textLabel.text = title;
        cell.detailTextLabel.text = [NSString stringWithFormat:@"%u  %@ - %@  FLAC %.1fkHz/%ubit", info.trackNumber, artist, album, info.sampleRate / 1000.0, info.bitsPerSample];
        cell.selectionStyle = UITableViewCellSelectionStyleGray;
    }
    return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    size_t count = rdap_library_count(library_);
    if (scanning_ || count == 0 || indexPath.row >= (NSInteger)count) {
        return;
    }

    const char** paths = (const char**)calloc(count, sizeof(char*));
    if (paths == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        RDTrackInfo info;
        if (rdap_library_info(library_, i, &info)) {
            paths[i] = strdup(info.path);
        }
    }
    rdap_engine_set_queue(engine_, paths, count);
    rdap_engine_play_index(engine_, (size_t)indexPath.row);
    for (size_t i = 0; i < count; ++i) {
        free((void*)paths[i]);
    }
    free(paths);
}

@end
