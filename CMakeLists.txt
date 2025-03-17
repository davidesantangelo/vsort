cmake_minimum_required(VERSION 3.12)

project(vsort 
    VERSION 0.1.1
    DESCRIPTION "High-Performance Sorting Algorithm for Apple Silicon"
    LANGUAGES C
)

# Check for Apple Silicon
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(USE_APPLE_SILICON_OPTIMIZATIONS ON)
    add_compile_definitions(USE_APPLE_SILICON_OPTIMIZATIONS)
    add_compile_options(-mcpu=apple-a14)
endif()

# Add compile options
add_compile_options(
    -O3
    -Wall
    -ffast-math
    -ftree-vectorize
    -funroll-loops
)

# Define the library
add_library(vsort 
    vsort.c
)

target_include_directories(vsort 
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set version definitions
target_compile_definitions(vsort PRIVATE 
    VSORT_VERSION="${PROJECT_VERSION}"
)

# Check for OpenMP
find_package(OpenMP)
if(OpenMP_C_FOUND)
    target_link_libraries(vsort PUBLIC OpenMP::OpenMP_C)
else()
    message(STATUS "OpenMP not available, parallel features disabled")
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    target_compile_definitions(vsort PRIVATE _CRT_SECURE_NO_WARNINGS)
    # No need to link with math library on Windows - it's part of the standard library
else()
    # Unix/Linux/macOS - link with math library
    target_link_libraries(vsort PRIVATE m)
endif()

# Examples
set(EXAMPLES
    basic_example
    benchmark
    custom_comparator_example
    float_sorting_example
    struct_sorting_example
)

foreach(example ${EXAMPLES})
    add_executable(${example} examples/${example}.c)
    target_link_libraries(${example} vsort)
    if(APPLE)
        target_link_libraries(${example} "-framework Foundation" "-framework CoreFoundation")
    endif()
    if(NOT WIN32)
        target_link_libraries(${example} m)  # Link math library
    endif()
endforeach()

# Tests
set(TESTS
    test_basic
    test_performance
    test_apple_silicon
)

foreach(test ${TESTS})
    add_executable(${test} tests/${test}.c)
    target_link_libraries(${test} vsort)
    if(APPLE)
        target_link_libraries(${test} "-framework Foundation" "-framework CoreFoundation")
    endif()
    if(NOT WIN32)
        target_link_libraries(${test} m)  # Link math library
    endif()
endforeach()

# Install rules
install(TARGETS vsort
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES vsort.h DESTINATION include)
