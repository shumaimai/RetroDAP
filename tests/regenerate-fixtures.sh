#!/bin/sh
set -eu
cd "$(dirname "$0")/fixtures"

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

TMP_SOURCE=$(mktemp "${TMPDIR:-/tmp}/retrodap-sample.XXXXXX")
trap 'rm -f "$TMP_SOURCE"' EXIT HUP INT TERM
curl -L --retry 3 -o "$TMP_SOURCE" \
    https://filesamples.com/samples/audio/flac/sample1.flac
ffmpeg -v error -y -i "$TMP_SOURCE" -t 3 -c:a flac downloaded-sample.flac
