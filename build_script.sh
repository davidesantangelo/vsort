#!/bin/bash

set -e  # Exit on error

echo "Building vsort library..."

# Check for Command Line Tools installation
if ! xcode-select -p &>/dev/null; then
    echo "Error: Xcode Command Line Tools not found."
    echo "Please install them using: xcode-select --install"
    exit 1
fi

# Detect available SDK paths
MACOS_SDK_PATH=""
POSSIBLE_SDK_PATHS=(
    "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
    "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    "/Library/Developer/CommandLineTools/SDKs/MacOSX14.sdk"
    "/Library/Developer/CommandLineTools/SDKs/MacOSX13.sdk"
    "/Library/Developer/CommandLineTools/SDKs/MacOSX12.sdk"
    "/Library/Developer/CommandLineTools/SDKs/MacOSX11.sdk"
)

for SDK in "${POSSIBLE_SDK_PATHS[@]}"; do
    if [ -d "$SDK" ]; then
        MACOS_SDK_PATH="$SDK"
        break
    fi
done

if [ -z "$MACOS_SDK_PATH" ]; then
    echo "Warning: Could not find MacOS SDK. Will try to continue without specifying SDK."
    SDK_FLAGS=""
else
    echo "Using MacOS SDK at: $MACOS_SDK_PATH"
    SDK_FLAGS="-isysroot $MACOS_SDK_PATH"
fi

# Standard compiler flags with SDK path
CFLAGS="-O3 -Wall -DVSORT_VERSION=\"0.4.0\" -I. $SDK_FLAGS"

# Detect if running on Apple Silicon
if [ "$(uname -m)" = "arm64" ]; then
    CFLAGS="$CFLAGS -mcpu=apple-a14 -DUSE_APPLE_SILICON_OPTIMIZATIONS"
fi

# Add optimization flags
CFLAGS="$CFLAGS -ffast-math -ftree-vectorize -funroll-loops"

# Compile source files
echo "Compiling with flags: $CFLAGS"
clang $CFLAGS -c -o vsort.o vsort.c
clang $CFLAGS -c -o vsort_logger.o vsort_logger.c

# Create the static library
echo "Creating static library..."
ar rcs libvsort.a vsort.o vsort_logger.o

echo "Building tests..."
# Build test_basic with the same flags
clang $CFLAGS -o tests/test_basic tests/test_basic.c -framework Foundation -framework CoreFoundation -L. -lvsort -lm

# Build other tests as needed
# clang $CFLAGS -o tests/test_performance tests/test_performance.c -framework Foundation -framework CoreFoundation -L. -lvsort -lm
# clang $CFLAGS -o tests/test_apple_silicon tests/test_apple_silicon.c -framework Foundation -framework CoreFoundation -L. -lvsort -lm

echo "Building examples..."
# Build basic example
clang $CFLAGS -o examples/basic_example examples/basic_example.c -framework Foundation -framework CoreFoundation -L. -lvsort -lm

echo "Build completed successfully!"
