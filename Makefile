export THEOS ?= $(HOME)/.local/theos-work/theos
export TARGET = iphone:clang:6.1:6.1
export ARCHS = armv7
export PACKAGE_FORMAT = deb
export FINALPACKAGE ?= 1

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = RetroDAP
RetroDAP_BUNDLE_NAME = RetroDAP
RetroDAP_INSTALL_PATH = /Applications

RetroDAP_FILES = \
	src/ios/main.m \
	src/ios/RetroDAPAppDelegate.m \
	src/ios/AudioOutputController.m \
	src/ios/NowPlayingViewController.m \
	src/ios/LibraryViewController.m \
	src/core/dr_flac_impl.cpp \
	src/core/ring_buffer.cpp \
	src/core/flac_decoder.cpp \
	src/core/library_scanner.cpp \
	src/core/player_core.cpp

RetroDAP_FRAMEWORKS = UIKit Foundation AVFoundation AudioToolbox MediaPlayer CoreGraphics
RetroDAP_CFLAGS = -Wall -Wextra -Wno-unused-parameter -fno-objc-arc -Ivendor -Isrc -Isrc/core
RetroDAP_OBJCFLAGS = -fno-objc-arc
RetroDAP_CCFLAGS = -std=c++11
RetroDAP_CODESIGN_FLAGS = -Sentitlements.plist

include $(THEOS_MAKE_PATH)/application.mk

.PHONY: host-test
host-test:
	@mkdir -p build/host
	c++ -std=c++11 -Wall -Wextra -Werror -Ivendor -Isrc/core \
		src/core/dr_flac_impl.cpp src/core/ring_buffer.cpp src/core/flac_decoder.cpp \
		src/core/library_scanner.cpp src/core/player_core.cpp tests/host_tests.cpp \
		-pthread -o build/host/host_tests
	./build/host/host_tests tests/fixtures
