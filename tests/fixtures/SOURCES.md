# Test fixture sources

- `test.flac`, `ref-sine.pcm`, and `hires.flac` are generated locally with ffmpeg by `tests/regenerate-fixtures.sh`.
- `downloaded-sample.flac` is a 3-second excerpt of `https://filesamples.com/samples/audio/flac/sample1.flac`, fetched by the same script. It is included only as a decoder/parser test fixture.
