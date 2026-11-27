#import "AudioOutputController.h"

#import <AVFoundation/AVFoundation.h>

static OSStatus RetroDAPRenderCallback(void* userData,
    AudioUnitRenderActionFlags* actionFlags,
    const AudioTimeStamp* timeStamp,
    UInt32 busNumber,
    UInt32 frameCount,
    AudioBufferList* ioData) {
    (void)actionFlags;
    (void)timeStamp;
    (void)busNumber;
    RDEngine* engine = (RDEngine*)userData;
    if (ioData == NULL || ioData->mNumberBuffers == 0) {
        return noErr;
    }
    for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
        UInt32 framesForBuffer = ioData->mBuffers[i].mDataByteSize / 4;
        UInt32 framesToRender = framesForBuffer < frameCount ? framesForBuffer : frameCount;
        rdap_engine_render(engine, (int16_t*)ioData->mBuffers[i].mData, framesToRender);
    }
    return noErr;
}

@implementation AudioOutputController

- (id)initWithEngine:(RDEngine*)engine {
    self = [super init];
    if (self != nil) {
        engine_ = engine;
        audioUnit_ = NULL;
        currentSampleRate_ = 44100;
        lastPreferredSampleRate_ = 0;
        started_ = NO;
    }
    return self;
}

- (void)dealloc {
    [self stop];
    if (audioUnit_ != NULL) {
        AudioComponentInstanceDispose(audioUnit_);
        audioUnit_ = NULL;
    }
    [super dealloc];
}

- (BOOL)configureAudioUnit {
    if (audioUnit_ == NULL) {
        AudioComponentDescription description;
        memset(&description, 0, sizeof(description));
        description.componentType = kAudioUnitType_Output;
        description.componentSubType = kAudioUnitSubType_RemoteIO;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;
        AudioComponent component = AudioComponentFindNext(NULL, &description);
        if (component == NULL) {
            return NO;
        }
        if (AudioComponentInstanceNew(component, &audioUnit_) != noErr) {
            return NO;
        }
    }

    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));
    format.mSampleRate = currentSampleRate_;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = 16;
    format.mChannelsPerFrame = 2;
    format.mBytesPerFrame = 4;
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = 4;

    OSStatus status = AudioUnitSetProperty(audioUnit_,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input,
        0,
        &format,
        sizeof(format));
    if (status != noErr) {
        return NO;
    }

    AURenderCallbackStruct callback;
    memset(&callback, 0, sizeof(callback));
    callback.inputProc = RetroDAPRenderCallback;
    callback.inputProcRefCon = engine_;
    status = AudioUnitSetProperty(audioUnit_,
        kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Input,
        0,
        &callback,
        sizeof(callback));
    return status == noErr;
}

- (BOOL)start {
    if (started_) {
        return YES;
    }
    if (![self configureAudioUnit]) {
        return NO;
    }
    if (AudioUnitInitialize(audioUnit_) != noErr) {
        return NO;
    }
    if (AudioOutputUnitStart(audioUnit_) != noErr) {
        AudioUnitUninitialize(audioUnit_);
        return NO;
    }
    started_ = YES;
    return YES;
}

- (void)stop {
    if (!started_) {
        return;
    }
    AudioOutputUnitStop(audioUnit_);
    AudioUnitUninitialize(audioUnit_);
    started_ = NO;
}

- (void)syncSampleRate {
    unsigned int rate = rdap_engine_output_sample_rate(engine_);
    if (rate == 0) {
        return;
    }

    if (rate != lastPreferredSampleRate_) {
        NSError* error = nil;
        if ([[AVAudioSession sharedInstance] setPreferredSampleRate:(double)rate error:&error]) {
            lastPreferredSampleRate_ = rate;
        }
    }

    if (rate == currentSampleRate_) {
        return;
    }
    BOOL wasStarted = started_;
    if (wasStarted) {
        [self stop];
    }
    currentSampleRate_ = rate;
    if (wasStarted) {
        [self start];
    }
}

@end
