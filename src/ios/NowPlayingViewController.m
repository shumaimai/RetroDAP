#import "NowPlayingViewController.h"

#import <MediaPlayer/MediaPlayer.h>
#import <stdlib.h>

#import "AudioOutputController.h"
#import "RetroDAPAppDelegate.h"

static NSString* RetroDAPUTF8String(const char* text) {
    if (text == NULL) {
        return nil;
    }
    NSString* value = [NSString stringWithUTF8String:text];
    if (value != nil) {
        return value;
    }
    value = [NSString stringWithCString:text encoding:NSISOLatin1StringEncoding];
    return value != nil ? value : @"";
}

@implementation NowPlayingViewController

- (id)initWithEngine:(RDEngine*)engine audioOutput:(AudioOutputController*)audioOutput {
    self = [super initWithNibName:nil bundle:nil];
    if (self != nil) {
        engine_ = engine;
        audioOutput_ = [audioOutput retain];
        artworkCache_ = [[NSCache alloc] init];
        artworkCache_.totalCostLimit = 4 * 1024 * 1024;
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [updateTimer_ invalidate];
    [currentPath_ release];
    [artworkCache_ release];
    [audioOutput_ release];
    [super dealloc];
}

- (UILabel*)makeLabelWithFrame:(CGRect)frame fontSize:(CGFloat)size color:(UIColor*)color alignment:(UITextAlignment)alignment {
    UILabel* label = [[[UILabel alloc] initWithFrame:frame] autorelease];
    label.backgroundColor = [UIColor clearColor];
    label.textColor = color;
    label.font = [UIFont systemFontOfSize:size];
    label.textAlignment = alignment;
    return label;
}

- (UIButton*)makeButtonWithFrame:(CGRect)frame title:(NSString*)title action:(SEL)action {
    UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
    button.frame = frame;
    [button setTitle:title forState:UIControlStateNormal];
    [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [button setTitleColor:[UIColor colorWithWhite:0.55 alpha:1.0] forState:UIControlStateHighlighted];
    button.titleLabel.font = [UIFont boldSystemFontOfSize:28.0];
    button.backgroundColor = [UIColor colorWithWhite:0.07 alpha:1.0];
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor blackColor];

    UIColor* dimColor = [UIColor colorWithWhite:0.68 alpha:1.0];
    artworkView_ = [[[UIImageView alloc] initWithFrame:CGRectMake(60, 8, 200, 200)] autorelease];
    artworkView_.backgroundColor = [UIColor colorWithRed:0.07 green:0.07 blue:0.07 alpha:1.0];
    artworkView_.contentMode = UIViewContentModeScaleAspectFit;
    [self.view addSubview:artworkView_];

    titleLabel_ = [self makeLabelWithFrame:CGRectMake(10, 214, 300, 22) fontSize:18.0 color:[UIColor whiteColor] alignment:NSTextAlignmentCenter];
    artistLabel_ = [self makeLabelWithFrame:CGRectMake(10, 236, 300, 18) fontSize:14.0 color:dimColor alignment:NSTextAlignmentCenter];
    albumLabel_ = [self makeLabelWithFrame:CGRectMake(10, 256, 300, 18) fontSize:13.0 color:dimColor alignment:NSTextAlignmentCenter];
    formatLabel_ = [self makeLabelWithFrame:CGRectMake(10, 276, 300, 17) fontSize:12.0 color:[UIColor colorWithRed:0.35 green:0.85 blue:0.55 alpha:1.0] alignment:NSTextAlignmentCenter];
    [self.view addSubview:titleLabel_];
    [self.view addSubview:artistLabel_];
    [self.view addSubview:albumLabel_];
    [self.view addSubview:formatLabel_];

    seekSlider_ = [[[UISlider alloc] initWithFrame:CGRectMake(14, 300, 292, 24)] autorelease];
    seekSlider_.minimumValue = 0.0f;
    seekSlider_.maximumValue = 1.0f;
    [seekSlider_ addTarget:self action:@selector(seekTouchDown) forControlEvents:UIControlEventTouchDown];
    [seekSlider_ addTarget:self action:@selector(seekChanged) forControlEvents:UIControlEventValueChanged];
    [seekSlider_ addTarget:self action:@selector(seekFinished) forControlEvents:UIControlEventTouchUpInside];
    [seekSlider_ addTarget:self action:@selector(seekFinished) forControlEvents:UIControlEventTouchUpOutside];
    [seekSlider_ addTarget:self action:@selector(seekFinished) forControlEvents:UIControlEventTouchCancel];
    [self.view addSubview:seekSlider_];

    elapsedLabel_ = [self makeLabelWithFrame:CGRectMake(12, 324, 70, 16) fontSize:11.0 color:dimColor alignment:NSTextAlignmentLeft];
    remainingLabel_ = [self makeLabelWithFrame:CGRectMake(238, 324, 70, 16) fontSize:11.0 color:dimColor alignment:NSTextAlignmentRight];
    [self.view addSubview:elapsedLabel_];
    [self.view addSubview:remainingLabel_];

    UIButton* previousButton = [self makeButtonWithFrame:CGRectMake(18, 348, 82, 42) title:@"|<" action:@selector(previousPressed)];
    playButton_ = [self makeButtonWithFrame:CGRectMake(119, 348, 82, 42) title:@">" action:@selector(playPausePressed)];
    UIButton* nextButton = [self makeButtonWithFrame:CGRectMake(220, 348, 82, 42) title:@">|" action:@selector(nextPressed)];
    [self.view addSubview:previousButton];
    [self.view addSubview:playButton_];
    [self.view addSubview:nextButton];

    routeLabel_ = [self makeLabelWithFrame:CGRectMake(10, 394, 300, 15) fontSize:11.0 color:dimColor alignment:NSTextAlignmentCenter];
    [self.view addSubview:routeLabel_];

    titleLabel_.text = @"RetroDAP";
    artistLabel_.text = @"Select a track in Library";
    formatLabel_.text = @"FLAC";
    elapsedLabel_.text = @"0:00";
    remainingLabel_.text = @"-0:00";
    [self updateRouteLabel];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(updateRouteLabel) name:RetroDAPRouteDidChangeNotification object:nil];
}

- (void)viewDidUnload {
    [updateTimer_ invalidate];
    updateTimer_ = nil;
    artworkView_ = nil;
    titleLabel_ = nil;
    artistLabel_ = nil;
    albumLabel_ = nil;
    formatLabel_ = nil;
    elapsedLabel_ = nil;
    remainingLabel_ = nil;
    routeLabel_ = nil;
    seekSlider_ = nil;
    playButton_ = nil;
    [super viewDidUnload];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    [self becomeFirstResponder];
    if (updateTimer_ == nil) {
        updateTimer_ = [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(updatePlaybackUI) userInfo:nil repeats:YES];
    }
    [self updatePlaybackUI];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [self resignFirstResponder];
}

- (BOOL)canBecomeFirstResponder {
    return YES;
}

- (NSString*)timeString:(double)seconds {
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    int total = (int)(seconds + 0.5);
    return [NSString stringWithFormat:@"%d:%02d", total / 60, total % 60];
}

- (UIImage*)resizedArtworkFromData:(NSData*)data {
    if (data == nil || [data length] == 0) {
        return nil;
    }
    UIImage* source = [UIImage imageWithData:data];
    if (source == nil) {
        return nil;
    }
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(200, 200), YES, 0.0);
    [source drawInRect:CGRectMake(0, 0, 200, 200)];
    UIImage* resized = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return resized;
}

- (UIImage*)artworkForInfo:(RDTrackInfo*)info {
    if (info->artworkSize == 0 || info->artworkSize > 8 * 1024 * 1024) {
        return nil;
    }
    NSString* key = RetroDAPUTF8String(info->path);
    UIImage* cached = [artworkCache_ objectForKey:key];
    if (cached != nil) {
        return cached;
    }
    void* bytes = malloc(info->artworkSize);
    if (bytes == NULL) {
        return nil;
    }
    rdap_engine_current_artwork(engine_, bytes, info->artworkSize);
    NSData* data = [NSData dataWithBytes:bytes length:info->artworkSize];
    free(bytes);
    UIImage* resized = [self resizedArtworkFromData:data];
    if (resized != nil) {
        [artworkCache_ setObject:resized forKey:key cost:(NSInteger)[data length]];
    }
    return resized;
}

- (void)updateNowPlayingInfo:(RDTrackInfo*)info artwork:(UIImage*)artwork position:(double)position {
    NSMutableDictionary* nowPlaying = [NSMutableDictionary dictionary];
    if (info->title[0] != '\0') {
        [nowPlaying setObject:RetroDAPUTF8String(info->title) forKey:MPMediaItemPropertyTitle];
    }
    if (info->artist[0] != '\0') {
        [nowPlaying setObject:RetroDAPUTF8String(info->artist) forKey:MPMediaItemPropertyArtist];
    }
    if (info->album[0] != '\0') {
        [nowPlaying setObject:RetroDAPUTF8String(info->album) forKey:MPMediaItemPropertyAlbumTitle];
    }
    [nowPlaying setObject:[NSNumber numberWithDouble:info->durationSeconds] forKey:MPMediaItemPropertyPlaybackDuration];
    [nowPlaying setObject:[NSNumber numberWithDouble:position] forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    [nowPlaying setObject:[NSNumber numberWithDouble:rdap_engine_is_playing(engine_) ? 1.0 : 0.0] forKey:MPNowPlayingInfoPropertyPlaybackRate];
    if (artwork != nil) {
        MPMediaItemArtwork* itemArtwork = [[[MPMediaItemArtwork alloc] initWithImage:artwork] autorelease];
        [nowPlaying setObject:itemArtwork forKey:MPMediaItemPropertyArtwork];
    }
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nowPlaying;
}

- (void)updatePlaybackUI {
    [audioOutput_ syncSampleRate];

    RDTrackInfo info;
    if (!rdap_engine_current_info(engine_, &info)) {
        return;
    }

    NSString* path = RetroDAPUTF8String(info.path);
    BOOL trackChanged = currentPath_ == nil || ![currentPath_ isEqualToString:path];
    UIImage* artwork = nil;
    if (trackChanged) {
        [currentPath_ autorelease];
        currentPath_ = [path retain];
        artwork = [self artworkForInfo:&info];
        artworkView_.image = artwork;
        titleLabel_.text = info.title[0] != '\0' ? RetroDAPUTF8String(info.title) : [path lastPathComponent];
        artistLabel_.text = info.artist[0] != '\0' ? RetroDAPUTF8String(info.artist) : @"Unknown Artist";
        albumLabel_.text = info.album[0] != '\0' ? RetroDAPUTF8String(info.album) : @"Unknown Album";
        formatLabel_.text = [NSString stringWithFormat:@"FLAC %.1fkHz / %ubit / %uch  ->  %.1fkHz out", info.sampleRate / 1000.0, info.bitsPerSample, info.channels, info.outputSampleRate / 1000.0];
    } else {
        artwork = artworkView_.image;
    }

    double position = rdap_engine_position_seconds(engine_);
    if (!scrubbing_) {
        seekSlider_.maximumValue = (float)(info.durationSeconds > 0.0 ? info.durationSeconds : 1.0);
        seekSlider_.value = (float)position;
        elapsedLabel_.text = [self timeString:position];
        remainingLabel_.text = [NSString stringWithFormat:@"-%@", [self timeString:info.durationSeconds - position]];
    }
    [playButton_ setTitle:rdap_engine_is_playing(engine_) ? @"||" : @">" forState:UIControlStateNormal];

    if (trackChanged) {
        [self updateNowPlayingInfo:&info artwork:artwork position:position];
    }
}

- (void)updateRouteLabel {
    routeLabel_.text = [NSString stringWithFormat:@"Output: %@", [RetroDAPAppDelegate currentRouteDescription]];
}

- (void)seekTouchDown {
    scrubbing_ = YES;
}

- (void)seekChanged {
    elapsedLabel_.text = [self timeString:seekSlider_.value];
    RDTrackInfo info;
    if (rdap_engine_current_info(engine_, &info)) {
        remainingLabel_.text = [NSString stringWithFormat:@"-%@", [self timeString:info.durationSeconds - seekSlider_.value]];
    }
}

- (void)seekFinished {
    if (scrubbing_) {
        rdap_engine_seek_seconds(engine_, seekSlider_.value);
    }
    scrubbing_ = NO;
}

- (void)playPausePressed {
    if (rdap_engine_is_playing(engine_)) {
        rdap_engine_pause(engine_);
    } else {
        rdap_engine_resume(engine_);
    }
    [self updatePlaybackUI];
}

- (void)previousPressed {
    rdap_engine_previous(engine_);
    [self performSelector:@selector(updatePlaybackUI) withObject:nil afterDelay:0.1];
}

- (void)nextPressed {
    rdap_engine_next(engine_);
    [self performSelector:@selector(updatePlaybackUI) withObject:nil afterDelay:0.1];
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
            [self playPausePressed];
            break;
        case UIEventSubtypeRemoteControlNextTrack:
            rdap_engine_next(engine_);
            break;
        case UIEventSubtypeRemoteControlPreviousTrack:
            rdap_engine_previous(engine_);
            break;
        default:
            return;
    }
    [self updatePlaybackUI];
}

@end
