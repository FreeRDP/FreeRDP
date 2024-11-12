option(CMAKE_COLOR_MAKEFILE "colorful CMake makefile" ON)
option(CMAKE_VERBOSE_MAKEFILE "verbose CMake makefile" ON)
option(CMAKE_POSITION_INDEPENDENT_CODE "build with position independent code (-fPIC or -fPIE)" ON)
option(WITH_LIBRARY_VERSIONING "Use library version triplet" ON)
option(WITH_BINARY_VERSIONING "Use binary versioning" OFF)
option(WITH_RESOURCE_VERSIONING "Use resource versioning" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# We want to control the winpr assert for the whole project
option(WITH_VERBOSE_WINPR_ASSERT "Compile with verbose WINPR_ASSERT." ON)
if(WITH_VERBOSE_WINPR_ASSERT)
  add_compile_definitions(WITH_VERBOSE_WINPR_ASSERT)
endif()

# known issue on android, thus disabled until we support newer CMake
# https://github.com/android/ndk/issues/1444
if(NOT ANDROID)
  if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
  endif()
  if(POLICY CMP0138)
    cmake_policy(SET CMP0138 NEW)
  endif()
  include(CheckIPOSupported)
  check_ipo_supported(RESULT supported OUTPUT error)
  if(NOT supported)
    message(WARNING "LTO not supported, got ${error}")
  endif()

  option(CMAKE_INTERPROCEDURAL_OPTIMIZATION "build with link time optimization" ${supported})
endif()

set(SUPPORTED_BUILD_TYPES "Debug" "Release" "MinSizeRel" "RelWithDebInfo")

# Default to release build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  # Set a default build type if none was specified
  set(default_build_type "Release")

  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${SUPPORTED_BUILD_TYPES})
endif()

if(CMAKE_BUILD_TYPE)
  if(NOT "${CMAKE_BUILD_TYPE}" IN_LIST SUPPORTED_BUILD_TYPES)
    message(FATAL_ERROR "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} not supported. Set to any of ${SUPPORTED_BUILD_TYPES}")
  endif()
endif()

if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "Release;Debug;MinSizeRel;RelWithDebInfo" CACHE INTERNAL "freerdp default")
endif()

include(PlatformDefaults)
include(PreventInSourceBuilds)
include(GNUInstallDirsWrapper)
include(MSVCRuntime)
include(ConfigureRPATH)
include(ClangTidy)
include(AddTargetWithResourceFile)
include(DisableCompilerWarnings)
include(CleaningConfigureFile)
