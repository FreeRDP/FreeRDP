# This file is based off of the Platform/Darwin.cmake and Platform/UnixPaths.cmake
# files which are included with CMake 2.8.4
# It has been altered for iOS development

# Options:
#
# IOS_PLATFORM = OS (default) or SIMULATOR
#   This decides if SDKS will be selected from the iPhoneOS.platform or iPhoneSimulator.platform folders
#   If not specified, it can be inferred if you pass CMAKE_OSX_ARCHITECTURES
#	Otherwise the default is OS
#   OS - used to build for iPhone and iPad physical devices, which have an arm arch.
#   SIMULATOR - used to build for the Simulator platforms, which have an x86 arch.
#
# CMAKE_IOS_DEVELOPER_ROOT = automatic(default) or /path/to/platform/Developer folder
#   By default this location is automatically chosen based on the IOS_PLATFORM value above.
#   If set manually, it will override the default location and force the user of a particular Developer Platform
#
# CMAKE_IOS_SDK_ROOT = automatic(default) or /path/to/platform/Developer/SDKs/SDK folder
#   By default this location is automatcially chosen based on the CMAKE_IOS_DEVELOPER_ROOT value.
#   In this case it will always be the most up-to-date SDK found in the CMAKE_IOS_DEVELOPER_ROOT path.
#   If set manually, this will force the use of a specific SDK version
# This toolchain defines the following variables for use externally:
#
# IOS_SDK_VERSION: Version of iOS SDK being used.
# CMAKE_OSX_ARCHITECTURES: Architectures being compiled for (generated from IOS_PLATFORM).
#
# This toolchain defines the following macros for use externally:
#
# set_xcode_property (TARGET XCODE_PROPERTY XCODE_VALUE)
#   A convenience macro for setting xcode specific properties on targets
#   example: set_xcode_property (myioslib IPHONEOS_DEPLOYMENT_TARGET "3.1").
#
# find_host_package (PROGRAM ARGS)
#   A macro used to find executable programs on the host system, not within the
#   iOS environment.  Thanks to the android-cmake project for providing the
#   command.

#########

# Default to building for iPhoneOS if not specified otherwise, and we cannot
# determine the platform from the CMAKE_OSX_ARCHITECTURES variable.  The use
# of CMAKE_OSX_ARCHITECTURES is such that try_compile() projects can correctly
# determine the value of IOS_PLATFORM from the root project, as
# CMAKE_OSX_ARCHITECTURES is propagated to them by CMake.
if (NOT DEFINED IOS_PLATFORM)
  if (CMAKE_OSX_ARCHITECTURES)
    if (CMAKE_OSX_ARCHITECTURES MATCHES ".*arm.*")
      set(IOS_PLATFORM "OS")
    elseif (CMAKE_OSX_ARCHITECTURES MATCHES "i386")
      set(IOS_PLATFORM "SIMULATOR")
    elseif (CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
      set(IOS_PLATFORM "SIMULATOR")
    endif()
  endif()
  if (NOT IOS_PLATFORM)
    set(IOS_PLATFORM "OS")
  endif()
endif()
set(IOS_PLATFORM ${IOS_PLATFORM} CACHE STRING
  "Type of iOS platform for which to build.")

# Check the platform selection and setup for developer root
if ("${IOS_PLATFORM}" STREQUAL "OS")
  set (XCODE_PLATFORM iPhoneOS)
  set (IOS_PLATFORM_LOCATION "iPhoneOS.platform")
  set (XCODE_IOS_PLATFORM iphoneos)
  set (IOS_ARCH armv7 armv7s arm64)

  # This causes the installers to properly locate the output libraries
  set (CMAKE_XCODE_EFFECTIVE_PLATFORMS "-iphoneos")
elseif ("${IOS_PLATFORM}" STREQUAL "SIMULATOR")
  set (SIMULATOR true)
  set (XCODE_PLATFORM iPhoneSimulator)
  set (IOS_PLATFORM_LOCATION "iPhoneSimulator.platform")
  set (XCODE_IOS_PLATFORM iphonesimulator)
  set (IOS_ARCH i386 x86_64)

  # This causes the installers to properly locate the output libraries
  set (CMAKE_XCODE_EFFECTIVE_PLATFORMS "-iphonesimulator")
else ()
  message (FATAL_ERROR "Unsupported IOS_PLATFORM value selected. Please choose OS or SIMULATOR")
endif ()

# Set the architectures for which to build.
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES ${IOS_ARCH})
endif()
message(STATUS "Configuring iOS build for ${CMAKE_OSX_ARCHITECTURES} architectures")
set(CMAKE_OSX_ARCHITECTURES ${CMAKE_OSX_ARCHITECTURES} CACHE STRING "Build architecture for iOS")

# Setup iOS developer location unless specified manually with CMAKE_IOS_DEVELOPER_ROOT
if (NOT DEFINED CMAKE_IOS_DEVELOPER_ROOT)
    set (CMAKE_IOS_DEVELOPER_ROOT "/Applications/Xcode.app/Contents/Developer/Platforms/${IOS_PLATFORM_LOCATION}/Developer")
endif ()
set (CMAKE_IOS_DEVELOPER_ROOT ${CMAKE_IOS_DEVELOPER_ROOT} CACHE PATH "Location of iOS Platform")

# Find and use the most recent iOS sdk unless specified manually with CMAKE_IOS_SDK_ROOT
if (NOT DEFINED CMAKE_IOS_SDK_ROOT)
  file (GLOB _CMAKE_IOS_SDKS "${CMAKE_IOS_DEVELOPER_ROOT}/SDKs/*")
  if (_CMAKE_IOS_SDKS)
    list (SORT _CMAKE_IOS_SDKS)
    list (REVERSE _CMAKE_IOS_SDKS)
    list (GET _CMAKE_IOS_SDKS 0 CMAKE_IOS_SDK_ROOT)
  else ()
    message (FATAL_ERROR "No iOS SDKs found in default search path ${CMAKE_IOS_DEVELOPER_ROOT}. Manually set CMAKE_IOS_SDK_ROOT or install the iOS SDK.")
  endif ()
  message (STATUS "Toolchain using default iOS SDK: ${CMAKE_IOS_SDK_ROOT}")
endif ()
set (CMAKE_IOS_SDK_ROOT ${CMAKE_IOS_SDK_ROOT} CACHE PATH "Location of the selected iOS SDK")

# Get the SDK version information.
execute_process(COMMAND xcodebuild -sdk ${CMAKE_IOS_SDK_ROOT} -version SDKVersion
  OUTPUT_VARIABLE IOS_SDK_VERSION
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND xcode-select -p OUTPUT_VARIABLE XCODE_DEVELOPER_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
set(XCODE_CPP_INCLUDE_DIR "${XCODE_DEVELOPER_DIR}/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1")
set(XCODE_SDK_INCLUDE_DIR "${XCODE_DEVELOPER_DIR}/Platforms/${XCODE_PLATFORM}.platform/Developer/SDKs/${XCODE_PLATFORM}.sdk/usr/include")

set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${XCODE_CPP_INCLUDE_DIR}")
include_directories(BEFORE SYSTEM "${XCODE_SDK_INCLUDE_DIR}")

set (CMAKE_OSX_SYSROOT ${XCODE_IOS_PLATFORM})

# Specify minimum version of deployment target.
# Unless specified, the latest SDK version is used by default.
if(NOT DEFINED IOS_DEPLOYMENT_TARGET)
  if(DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(IOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}" CACHE STRING "minimum iOS deployment target")
  else()
	 set(IOS_DEPLOYMENT_TARGET "${IOS_SDK_VERSION}" CACHE STRING "minimum iOS deployment target")
  endif()
endif()

set(XCODE_IOS_PLATFORM_VERSION_FLAGS "-m${XCODE_IOS_PLATFORM}-version-min=${IOS_DEPLOYMENT_TARGET}")

if(CMAKE_GENERATOR MATCHES "Xcode")
    set(CMAKE_XCODE_ATTRIBUTE_ARCHS "${IOS_ARCH}")
    set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${IOS_DEPLOYMENT_TARGET}")
endif()

message(STATUS "Building for minimum iOS version: ${IOS_DEPLOYMENT_TARGET}"
               " (SDK version: ${IOS_SDK_VERSION})")

# Standard settings
set (CMAKE_SYSTEM_NAME Darwin)
set (CMAKE_SYSTEM_VERSION ${IOS_SDK_VERSION})
set (UNIX TRUE)
set (APPLE TRUE)
set (IOS TRUE)
set (APPLE_IOS TRUE)

# Force unset of OS X-specific deployment target (otherwise autopopulated), required as of cmake 2.8.10.
set(CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "Must be empty for iOS builds." FORCE)

# Find the C & C++ compilers for the specified SDK.
if (NOT CMAKE_C_COMPILER)
  execute_process(COMMAND xcrun -sdk ${CMAKE_IOS_SDK_ROOT} -find clang
    OUTPUT_VARIABLE CMAKE_C_COMPILER
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if (NOT CMAKE_CXX_COMPILER)
  execute_process(COMMAND xcrun -sdk ${CMAKE_IOS_SDK_ROOT} -find clang++
    OUTPUT_VARIABLE CMAKE_CXX_COMPILER
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_WORKS TRUE)

# Hack: if a new cmake (which uses CMAKE_INSTALL_NAME_TOOL) runs on an old
# build tree (where install_name_tool was hardcoded) and where
# CMAKE_INSTALL_NAME_TOOL isn't in the cache and still cmake didn't fail in
# CMakeFindBinUtils.cmake (because it isn't rerun) hardcode
# CMAKE_INSTALL_NAME_TOOL here to install_name_tool, so it behaves as it did
# before
if (NOT DEFINED CMAKE_INSTALL_NAME_TOOL)
  find_program(CMAKE_INSTALL_NAME_TOOL install_name_tool)
endif (NOT DEFINED CMAKE_INSTALL_NAME_TOOL)

# All iOS/Darwin specific settings - some may be redundant.
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dylib")
set(CMAKE_SHARED_MODULE_PREFIX "lib")
set(CMAKE_SHARED_MODULE_SUFFIX ".so")
set(CMAKE_MODULE_EXISTS 1)
set(CMAKE_DL_LIBS "")

set(CMAKE_C_OSX_COMPATIBILITY_VERSION_FLAG "-compatibility_version ")
set(CMAKE_C_OSX_CURRENT_VERSION_FLAG "-current_version ")
set(CMAKE_CXX_OSX_COMPATIBILITY_VERSION_FLAG "${CMAKE_C_OSX_COMPATIBILITY_VERSION_FLAG}")
set(CMAKE_CXX_OSX_CURRENT_VERSION_FLAG "${CMAKE_C_OSX_CURRENT_VERSION_FLAG}")

# Force variables set by FindThreads.cmake
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)

# change default target type for try_compile() calls (iOS doesn't support executable targets)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Hidden visibilty is required for cxx on iOS 
set(CMAKE_C_FLAGS_INIT "")
set(CMAKE_C_FLAGS "${XCODE_IOS_PLATFORM_VERSION_FLAGS} ${CMAKE_C_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "")
set(CMAKE_CXX_FLAGS "${XCODE_IOS_PLATFORM_VERSION_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)

set(CMAKE_C_LINK_FLAGS "-Wl,-search_paths_first ${CMAKE_C_LINK_FLAGS}")
set(CMAKE_CXX_LINK_FLAGS "-Wl,-search_paths_first ${CMAKE_CXX_LINK_FLAGS}")

set(CMAKE_PLATFORM_HAS_INSTALLNAME 1)
set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "-dynamiclib -headerpad_max_install_names")
set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "-bundle -headerpad_max_install_names")
set(CMAKE_SHARED_MODULE_LOADER_C_FLAG "-Wl,-bundle_loader,")
set(CMAKE_SHARED_MODULE_LOADER_CXX_FLAG "-Wl,-bundle_loader,")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib" ".so" ".a")

# Set the find root to the iOS developer roots and to user defined paths.
set(CMAKE_FIND_ROOT_PATH ${CMAKE_IOS_DEVELOPER_ROOT} ${CMAKE_IOS_SDK_ROOT}
  ${CMAKE_PREFIX_PATH} CACHE STRING "iOS find search path root" FORCE)

# Default to searching for frameworks first.
set(CMAKE_FIND_FRAMEWORK FIRST)

# Set up the default search directories for frameworks.
set(CMAKE_SYSTEM_FRAMEWORK_PATH
  ${CMAKE_IOS_SDK_ROOT}/System/Library/Frameworks
  ${CMAKE_IOS_SDK_ROOT}/System/Library/PrivateFrameworks
  ${CMAKE_IOS_SDK_ROOT}/Developer/Library/Frameworks)

# Only search the specified iOS SDK, not the remainder of the host filesystem.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Find (Apple's) libtool.
execute_process(COMMAND xcrun -sdk ${CMAKE_IOS_SDK_ROOT} -find libtool
  OUTPUT_VARIABLE IOS_LIBTOOL
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)
# Configure libtool to be used instead of ar + ranlib to build static libraries.
# This is required on Xcode 7+, but should also work on previous versions of Xcode.
set(CMAKE_C_CREATE_STATIC_LIBRARY
  "${IOS_LIBTOOL} -static -o <TARGET> <LINK_FLAGS> <OBJECTS> ")
set(CMAKE_CXX_CREATE_STATIC_LIBRARY
  "${IOS_LIBTOOL} -static -o <TARGET> <LINK_FLAGS> <OBJECTS> ")

# This little macro lets you set any XCode specific property.
macro(set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
  set_property(TARGET ${TARGET} PROPERTY
    XCODE_ATTRIBUTE_${XCODE_PROPERTY} ${XCODE_VALUE})
endmacro(set_xcode_property)

# This macro lets you find executable programs on the host system.
macro(find_host_package)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
  set(IOS FALSE)

  find_package(${ARGN})

  set(IOS TRUE)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endmacro(find_host_package)
