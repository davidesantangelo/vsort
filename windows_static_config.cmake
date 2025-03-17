# Force static build for Windows
# This overrides any project settings

# Globally force static libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# Force static runtime linkage for MSVC
foreach(flag_var 
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
   if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
   endif()
endforeach()

# Disable automatic DLL exports completely
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF CACHE BOOL "Don't automatically export all symbols" FORCE)

# Add optimization flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 /Ot" CACHE STRING "C flags" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Ot" CACHE STRING "C++ flags" FORCE)
