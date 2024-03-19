option(CMAKE_COLOR_MAKEFILE "colorful CMake makefile" ON)
option(CMAKE_VERBOSE_MAKEFILE "verbose CMake makefile" ON)
option(CMAKE_POSITION_INDEPENDENT_CODE "build with position independent code (-fPIC or -fPIE)" ON)
option(WITH_LIBRARY_VERSIONING "Use library version triplet" ON)
option(WITH_BINARY_VERSIONING "Use binary versioning" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# known issue on android, thus disabled until we support newer CMake
# https://github.com/android/ndk/issues/1444
if (NOT ANDROID)
	if(POLICY CMP0069)
		cmake_policy(SET CMP0069 NEW)
	endif()
	if(POLICY CMP0138)
		cmake_policy(SET CMP0138 NEW)
	endif()
	include(CheckIPOSupported)
	check_ipo_supported(RESULT supported OUTPUT error)
	if (NOT supported)
		message(WARNING "LTO not supported, got ${error}")
	endif()

	option(CMAKE_INTERPROCEDURAL_OPTIMIZATION "build with link time optimization" ${supported})
endif()

# Default to release build type
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "project default")
endif()

include(PlatformDefaults)
include(PreventInSourceBuilds)
include(GNUInstallDirsWrapper)
include(MSVCRuntime)
include(ConfigureRPATH)
include(ClangTidy)
include(AddTargetWithResourceFile)
