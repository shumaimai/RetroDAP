#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host
c++ -std=c++11 -Wall -Wextra -Werror -Ivendor -Isrc/core \
    src/core/dr_flac_impl.cpp src/core/ring_buffer.cpp src/core/flac_decoder.cpp \
    src/core/library_scanner.cpp src/core/player_core.cpp tests/host_tests.cpp \
    -pthread -o build/host/host_tests
./build/host/host_tests tests/fixtures
