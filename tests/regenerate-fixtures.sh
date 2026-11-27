#!/bin/sh
set -eu
cd "$(dirname "$0")"

ffmpeg -v error -y \
    -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=2" \
    -f lavfi -i "color=c=0x3355aa:s=64x64:d=0.1" \
    -map 0:a -map 1:v -c:a flac -c:v png -disposition:v attached_pic \
    -metadata title="Sine Test" -metadata artist="RetroDAP" \
    -metadata album="Host Tests" -metadata tracknumber="3" \
    test.flac
ffmpeg -v error -y -i test.flac -map 0:a -af 'pan=stereo|c0=c0|c1=c0' \
    -f s16le -acodec pcm_s16le ref-sine.pcm
ffmpeg -v error -y -f lavfi -i "sine=frequency=880:sample_rate=96000:duration=1" \
    -af aformat=sample_fmts=s32 -c:a flac hires.flac

if [ ! -f /tmp/retrodap-sample-source.flac ]; then
    curl -L --retry 3 -o /tmp/retrodap-sample-source.flac \
        https://filesamples.com/samples/audio/flac/sample1.flac
fi
ffmpeg -v error -y -i /tmp/retrodap-sample-source.flac -t 3 -c:a flac downloaded-sample.flac
