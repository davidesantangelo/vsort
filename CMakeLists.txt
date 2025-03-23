cmake_minimum_required(VERSION 3.12)

project(vsort 
    VERSION 0.3.4
    DESCRIPTION "High-Performance Sorting Algorithm for Apple Silicon"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)

# Add logger source files
set(VSORT_SOURCES vsort.c vsort_logger.c)

# Option for Apple Silicon optimizations
option(USE_APPLE_SILICON_OPTIMIZATIONS "Enable optimizations for Apple Silicon" OFF)
option(ENABLE_NEON "Enable NEON SIMD instructions" OFF)

# Check for Apple Silicon
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(USE_APPLE_SILICON_OPTIMIZATIONS ON)
    add_compile_definitions(USE_APPLE_SILICON_OPTIMIZATIONS)
    add_compile_options(-mcpu=apple-a14)
    
    # Enable NEON instructions - always available on Apple Silicon
    set(ENABLE_NEON ON)
    add_compile_definitions(USE_ARM_NEON)
else()
    # Enable NEON if requested on other ARM platforms
    if(ENABLE_NEON AND NOT (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
        add_compile_definitions(USE_ARM_NEON)
        # Only use -mfpu=neon on traditional ARM, not on Apple Silicon
        add_compile_options(-mfpu=neon)
    endif()
endif()

# Define the library
if(WIN32)
    # Create a shared library (DLL) on Windows
    add_library(vsort SHARED ${VSORT_SOURCES})
    # Export symbols properly for DLL
    target_compile_definitions(vsort PRIVATE VSORT_EXPORTS)
    target_compile_definitions(vsort PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # Create a static library on other platforms
    add_library(vsort STATIC ${VSORT_SOURCES})
endif()

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
if(MSVC)
    # MSVC (Visual Studio) compiler flags
    target_compile_options(vsort PRIVATE 
        /O2             # Optimize for speed
        /Ob2            # Inline function expansion
        /Oi             # Enable intrinsic functions
        /Ot             # Favor fast code
        /Gy             # Function-level linking
        /fp:fast        # Fast floating point model
    )
elseif(APPLE)
    # macOS specific flags for Apple Silicon
    target_compile_options(vsort PRIVATE 
        -O3
        -ffast-math
        -ftree-vectorize
        -funroll-loops
        -march=native
    )
    
    # Check for ARM architecture (Apple Silicon)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(ARCH MATCHES "arm64")
        target_compile_options(vsort PRIVATE -mcpu=apple-a14)
    endif()
else()
    # Generic GCC/Clang flags for other platforms
    target_compile_options(vsort PRIVATE 
        -O3
        -ffast-math
        -ftree-vectorize
        -funroll-loops
    )
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific settings - no need to link with math library
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
    # Add Windows-specific configurations for examples
    if(MSVC)
        target_compile_options(${example} PRIVATE 
            /O2             # Optimize for speed
            /fp:fast        # Fast floating point model
        )
        target_compile_definitions(${example} PRIVATE _CRT_SECURE_NO_WARNINGS)
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
    # Add Windows-specific configurations for tests
    if(MSVC)
        target_compile_options(${test} PRIVATE 
            /O2             # Optimize for speed
            /fp:fast        # Fast floating point model
        )
        target_compile_definitions(${test} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endforeach()

# Install rules
install(TARGETS vsort
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES vsort.h DESTINATION include)

# Enable testing for the existing test targets
enable_testing()

# Register existing tests with CTest
foreach(test ${TESTS})
    add_test(NAME ${test} COMMAND ${test})
endforeach()