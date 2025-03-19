#!/bin/bash

set -e  # Exit on error

echo "Building vsort library..."

# Compile both source files
clang -O3 -Wall -DVSORT_VERSION=\"0.3.2\" -mcpu=apple-a14 -DUSE_APPLE_SILICON_OPTIMIZATIONS -ffast-math -ftree-vectorize -funroll-loops -c -o vsort.o vsort.c
clang -O3 -Wall -DVSORT_VERSION=\"0.3.2\" -mcpu=apple-a14 -DUSE_APPLE_SILICON_OPTIMIZATIONS -ffast-math -ftree-vectorize -funroll-loops -c -o vsort_logger.o vsort_logger.c

# Create the static library
ar rcs libvsort.a vsort.o vsort_logger.o

echo "Building tests..."
# Build test_basic
clang -O3 -Wall -DVSORT_VERSION=\"0.3.2\" -mcpu=apple-a14 -DUSE_APPLE_SILICON_OPTIMIZATIONS -ffast-math -ftree-vectorize -funroll-loops -I. -o tests/test_basic tests/test_basic.c -framework Foundation -framework CoreFoundation -L. -lvsort -lm

# Build other tests as needed
# clang -O3 -Wall -DVSORT_VERSION=\"0.3.2\" -mcpu=apple-a14 -DUSE_APPLE_SILICON_OPTIMIZATIONS -ffast-math -ftree-vectorize -funroll-loops -I. -o tests/test_performance tests/test_performance.c -framework Foundation -framework CoreFoundation -L. -lvsort -lm
# clang -O3 -Wall -DVSORT_VERSION=\"0.3.2\" -mcpu=apple-a14 -DUSE_APPLE_SILICON_OPTIMIZATIONS -ffast-math -ftree-vectorize -funroll-loops -I. -o tests/test_apple_silicon tests/test_apple_silicon.c -framework Foundation -framework CoreFoundation -L. -lvsort -lm

echo "Building examples..."
# Build basic example
clang -O3 -Wall -DVSORT_VERSION=\"0.3.2\" -mcpu=apple-a14 -DUSE_APPLE_SILICON_OPTIMIZATIONS -ffast-math -ftree-vectorize -funroll-loops -I. -o examples/basic_example examples/basic_example.c -framework Foundation -framework CoreFoundation -L. -lvsort -lm

echo "Build completed successfully!"
