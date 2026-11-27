# RetroDAP

iOS 6.1.3 iPhone 4S dedicated FLAC audio player built with Theos. RetroDAP is a phase-1 standalone application installed as `/Applications/RetroDAP.app`; it does not include a SpringBoard launcher tweak.

## Target

- iPhone 4S, ARMv7 32-bit, iOS 6.1 SDK, minimum iOS 6.x
- 30-pin MFi digital/audio docks, headphones, or built-in speaker through RemoteIO
- FLAC files under `/var/mobile/Media/FLAC/`, scanned recursively
- Objective-C manual reference counting UI; no Xcode project, Storyboard, XIB, ARC, or Swift
- Portable C/C++11 FLAC decoder, metadata parser, SPSC ring buffer, scanner, and playback core

## Linux build setup

Install basic dependencies (`git`, `make`, `perl`, `curl`, `unzip`, `xz-utils`, `fakeroot`, `dpkg-dev`, `rsync`, and a host C++ compiler), then run:

```sh
./tools/setup-theos-linux.sh
```

The script downloads Theos, the Linux iOS clang/ld toolchain, and `iPhoneOS6.1.sdk` from `qianjigui/iOS-sdks` into `~/.local/theos-work/theos`.

## Verify and package

```sh
make host-test
make package
```

The package is written to `packages/*.deb`. It is pseudo-signed with `ldid` using `entitlements.plist`; no Apple certificate or provisioning profile is used.

Install on the jailbroken phone with OpenSSH/dpkg or a package manager, then copy FLAC files below `/var/mobile/Media/FLAC/`.

## Test fixtures

`tests/fixtures` contains generated sine-wave fixtures plus a short excerpt downloaded from filesamples.com. See `tests/fixtures/SOURCES.md`; regenerate all fixtures with `tests/regenerate-fixtures.sh`.
