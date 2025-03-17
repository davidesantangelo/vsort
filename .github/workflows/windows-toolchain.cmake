# Windows toolchain file to avoid shared library issues
set(CMAKE_SYSTEM_NAME Windows)

# Force static libraries on Windows
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF CACHE BOOL "Export symbols for DLL" FORCE)

# Use static runtime libraries
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 
# Disable automatic DLL exports
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Ot /GL")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 /Ot /GL")

# Ensure we're using static linking
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG")
