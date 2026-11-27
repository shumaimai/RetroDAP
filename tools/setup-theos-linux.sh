#!/bin/sh
set -eu

THEOS_DIR=${THEOS:-$HOME/.local/theos-work/theos}
WORK_DIR=$(dirname "$THEOS_DIR")
DOWNLOADS="$WORK_DIR/downloads"
TOOLCHAIN_URL="https://github.com/L1ghtmann/llvm-project/releases/download/test-210562a/iOSToolchain-x86_64.tar.xz"
SDK_BASE="https://github.com/qianjigui/iOS-sdks/raw/master/iPhoneOS6.1.sdk"

mkdir -p "$WORK_DIR" "$DOWNLOADS"

if [ ! -d "$THEOS_DIR" ]; then
    git clone --recursive --depth=1 https://github.com/theos/theos.git "$THEOS_DIR"
fi
mkdir -p "$THEOS_DIR/toolchain/linux/iphone" "$THEOS_DIR/sdks"

if [ ! -x "$THEOS_DIR/toolchain/linux/iphone/bin/clang" ]; then
    curl -L --retry 3 -o "$DOWNLOADS/iOSToolchain-x86_64.tar.xz" "$TOOLCHAIN_URL"
    TMP_TOOLCHAIN=$(mktemp -d)
    tar -xJf "$DOWNLOADS/iOSToolchain-x86_64.tar.xz" -C "$TMP_TOOLCHAIN"
    CLANG_PATH=$(find "$TMP_TOOLCHAIN" -path '*/bin/clang' | head -1)
    if [ -z "$CLANG_PATH" ]; then
        echo "toolchain archive does not contain bin/clang" >&2
        exit 1
    fi
    TOOLCHAIN_ROOT=$(dirname "$(dirname "$CLANG_PATH")")
    cp -a "$TOOLCHAIN_ROOT/"* "$THEOS_DIR/toolchain/linux/iphone/"
    rm -rf "$TMP_TOOLCHAIN"
fi

if [ ! -d "$THEOS_DIR/sdks/iPhoneOS6.1.sdk" ]; then
    for part in 001 002 003; do
        curl -L --retry 3 -o "$DOWNLOADS/iPhoneOS6.1.sdk.zip.$part" "$SDK_BASE/iPhoneOS6.1.sdk.zip.$part"
    done
    cat "$DOWNLOADS/iPhoneOS6.1.sdk.zip.001" "$DOWNLOADS/iPhoneOS6.1.sdk.zip.002" "$DOWNLOADS/iPhoneOS6.1.sdk.zip.003" > "$DOWNLOADS/iPhoneOS6.1.sdk.zip"
    TMP_DIR=$(mktemp -d)
    unzip -q "$DOWNLOADS/iPhoneOS6.1.sdk.zip" -d "$TMP_DIR"
    mv "$TMP_DIR/iPhoneOS6.1.sdk" "$THEOS_DIR/sdks/iPhoneOS6.1.sdk"
    rm -rf "$TMP_DIR"
fi

printf 'THEOS=%s\n' "$THEOS_DIR"
printf 'clang: %s\n' "$($THEOS_DIR/toolchain/linux/iphone/bin/clang --version | head -1)"
printf 'sdk: %s\n' "$THEOS_DIR/sdks/iPhoneOS6.1.sdk"
