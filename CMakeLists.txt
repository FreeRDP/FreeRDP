# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2012 HP Development Company, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 2.8)

project(FreeRDP C CXX)

if(NOT DEFINED VENDOR)
	set(VENDOR "FreeRDP" CACHE STRING "FreeRDP package vendor")
endif()

if(NOT DEFINED PRODUCT)
	set(PRODUCT "FreeRDP" CACHE STRING "FreeRDP package name")
endif()

if(NOT DEFINED FREERDP_VENDOR)
	set(FREERDP_VENDOR 1)
endif()

set(CMAKE_COLOR_MAKEFILE ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include our extra modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

if((CMAKE_SYSTEM_NAME MATCHES "WindowsStore") AND (CMAKE_SYSTEM_VERSION MATCHES "10.0"))
	set(UWP 1)
	add_definitions("-D_UWP")
	set(CMAKE_WINDOWS_VERSION "WIN10")
endif()

# Check for cmake compatibility (enable/disable features)
include(CheckCmakeCompat)

# Include cmake modules
if(WITH_CLANG_FORMAT)
	include(ClangFormat)
endif()

include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(FindPkgConfig)
include(TestBigEndian)

include(FindFeature)
include(ConfigOptions)
include(ComplexLibrary)
include(FeatureSummary)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(GNUInstallDirsWrapper)
include(CMakePackageConfigHelpers)
include(InstallFreeRDPMan)
include(GetGitRevisionDescription)
include(SetFreeRDPCMakeInstallDir)

# Soname versioning
set(BUILD_NUMBER 0)
if ($ENV{BUILD_NUMBER})
	set(BUILD_NUMBER $ENV{BUILD_NUMBER})
endif()
set(WITH_LIBRARY_VERSIONING "ON")

set(RAW_VERSION_STRING "2.1.0")
if(EXISTS "${CMAKE_SOURCE_DIR}/.source_tag")
	file(READ ${CMAKE_SOURCE_DIR}/.source_tag RAW_VERSION_STRING)
elseif(USE_VERSION_FROM_GIT_TAG)
	git_get_exact_tag(_GIT_TAG --tags --always)
	if (NOT ${_GIT_TAG} STREQUAL "n/a")
		set(RAW_VERSION_STRING ${_GIT_TAG})
	endif()
endif()
string(STRIP ${RAW_VERSION_STRING} RAW_VERSION_STRING)

set(VERSION_REGEX "^.?([0-9]+)\\.([0-9]+)\\.([0-9]+)-?(.*)")
string(REGEX REPLACE "${VERSION_REGEX}" "\\1" FREERDP_VERSION_MAJOR "${RAW_VERSION_STRING}")
string(REGEX REPLACE "${VERSION_REGEX}" "\\2" FREERDP_VERSION_MINOR "${RAW_VERSION_STRING}")
string(REGEX REPLACE "${VERSION_REGEX}" "\\3" FREERDP_VERSION_REVISION "${RAW_VERSION_STRING}")
string(REGEX REPLACE "${VERSION_REGEX}" "\\4" FREERDP_VERSION_SUFFIX "${RAW_VERSION_STRING}")

set(FREERDP_API_VERSION "${FREERDP_VERSION_MAJOR}")
set(FREERDP_VERSION "${FREERDP_VERSION_MAJOR}.${FREERDP_VERSION_MINOR}.${FREERDP_VERSION_REVISION}")
if (FREERDP_VERSION_SUFFIX)
	set(FREERDP_VERSION_FULL "${FREERDP_VERSION}-${FREERDP_VERSION_SUFFIX}")
else()
	set(FREERDP_VERSION_FULL "${FREERDP_VERSION}")
endif()
message("FREERDP_VERSION=${FREERDP_VERSION_FULL}")

set(FREERDP_INCLUDE_DIR "include/freerdp${FREERDP_VERSION_MAJOR}/")

# Compatibility options
if(DEFINED STATIC_CHANNELS)
	message(WARNING "STATIC_CHANNELS is obsolete, please use BUILTIN_CHANNELS instead")
	set(BUILTIN_CHANNELS ${STATIC_CHANNELS} CACHE BOOL "" FORCE)
endif()

# Make paths absolute
if (CMAKE_INSTALL_PREFIX)
	get_filename_component(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)
endif()
if (FREERDP_EXTERNAL_PATH)
	get_filename_component (FREERDP_EXTERNAL_PATH "${FREERDP_EXTERNAL_PATH}" ABSOLUTE)
endif()

# Allow to search the host machine for git/ccache
if(CMAKE_CROSSCOMPILING)
	SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
endif(CMAKE_CROSSCOMPILING)

find_program(CCACHE ccache)
if(CCACHE AND WITH_CCACHE)
	if(CMAKE_VERSION VERSION_GREATER 3.3.2)
		if(NOT DEFINED CMAKE_C_COMPILER_LAUNCHER)
			SET(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
		endif(NOT DEFINED CMAKE_C_COMPILER_LAUNCHER)
		if(NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
			SET(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
		endif(NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
	else()
		set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE})
		set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE})
	endif()
endif(CCACHE AND WITH_CCACHE)

if(EXISTS "${CMAKE_SOURCE_DIR}/.source_version" )
	file(READ ${CMAKE_SOURCE_DIR}/.source_version GIT_REVISION)

  string(STRIP ${GIT_REVISION} GIT_REVISION)
else()
	git_get_exact_tag(GIT_REVISION --tags --always)

	if (${GIT_REVISION} STREQUAL "n/a")
		git_rev_parse (GIT_REVISION --short)
	endif()
endif()

if(CMAKE_CROSSCOMPILING)
	SET (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
endif(CMAKE_CROSSCOMPILING)
# /Allow to search the host machine for git/ccache

message(STATUS "Git Revision ${GIT_REVISION}")

# Turn on solution folders (2.8.4+)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Default to release build type
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
	if(IOS)
		set(BUILD_SHARED_LIBS OFF)
	else()
		set(BUILD_SHARED_LIBS ON)
	endif()
endif()

if(BUILD_TESTING)
	set(EXPORT_ALL_SYMBOLS TRUE)
elseif(NOT DEFINED EXPORT_ALL_SYMBOLS)
	set(EXPORT_ALL_SYMBOLS FALSE)
endif()

if (EXPORT_ALL_SYMBOLS)
#	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
	add_definitions(-DFREERDP_TEST_EXPORTS -DBUILD_TESTING)
endif(EXPORT_ALL_SYMBOLS)

# BSD
if(${CMAKE_SYSTEM_NAME} MATCHES "BSD")
	set(BSD TRUE)
	if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
		set(FREEBSD TRUE)
	endif()
	if(${CMAKE_SYSTEM_NAME} MATCHES "kFreeBSD")
		set(KFREEBSD TRUE)
	endif()
	if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
		set(OPENBSD TRUE)
	endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
	set(BSD TRUE)
	set(FREEBSD TRUE)
endif()

if(FREEBSD)
	find_path(EPOLLSHIM_INCLUDE_DIR NAMES sys/epoll.h sys/timerfd.h HINTS /usr/local/include/libepoll-shim)
	find_library(EPOLLSHIM_LIBS NAMES epoll-shim libepoll-shim HINTS /usr/local/lib)
endif()

# Configure MSVC Runtime
if(MSVC)
	include(MSVCRuntime)
	if(NOT DEFINED MSVC_RUNTIME)
		set(MSVC_RUNTIME "dynamic")
	endif()
	if(MSVC_RUNTIME STREQUAL "static")
		if(BUILD_SHARED_LIBS)
			message(FATAL_ERROR "Static CRT is only supported in a fully static build")
		endif()
		message(STATUS "Use the MSVC static runtime option carefully!")
		message(STATUS "OpenSSL uses /MD by default, and is very picky")
		message(STATUS "Random freeing errors are a common sign of runtime issues")
	endif()
	configure_msvc_runtime()

	if(NOT DEFINED CMAKE_SUPPRESS_REGENERATION)
		set(CMAKE_SUPPRESS_REGENERATION ON)
	endif()
endif()

# Enable 64bit file support on linux and FreeBSD.
if("${CMAKE_SYSTEM_NAME}" MATCHES "Linux" OR FREEBSD)
	add_definitions("-D_FILE_OFFSET_BITS=64")
endif()

# Use Standard conforming getpwnam_r() on Solaris.
if("${CMAKE_SYSTEM_NAME}" MATCHES "SunOS")
	add_definitions("-D_POSIX_PTHREAD_SEMANTICS")
endif()

# Compiler-specific flags
if(CMAKE_COMPILER_IS_GNUCC)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "i686")
		CHECK_SYMBOL_EXISTS(__x86_64__ "" IS_X86_64)
		if(IS_X86_64)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
		else()
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=i686")
		endif()
	else()
		if(CMAKE_POSITION_INDEPENDENT_CODE)
			if(${CMAKE_VERSION} VERSION_LESS 2.8.9)
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
			endif()
		endif()
	endif()

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

	CHECK_C_COMPILER_FLAG (-Wno-unused-result Wno-unused-result)
	if(Wno-unused-result)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-result")
	endif()
	CHECK_C_COMPILER_FLAG (-Wno-unused-but-set-variable Wno-unused-but-set-variable)
	if(Wno-unused-but-set-variable)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-but-set-variable")
	endif()
	CHECK_C_COMPILER_FLAG(-Wno-deprecated-declarations Wno-deprecated-declarations)
	if(Wno-deprecated-declarations)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
	endif()
	CHECK_CXX_COMPILER_FLAG(-Wno-deprecated-declarations Wno-deprecated-declarationsCXX)
	if(Wno-deprecated-declarationsCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
	endif()

	if(NOT EXPORT_ALL_SYMBOLS)
		message(STATUS "GCC default symbol visibility: hidden")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
	endif()
	if(BUILD_TESTING)
		CHECK_C_COMPILER_FLAG(-Wno-format Wno-format)
		if(Wno-format)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format")
		endif()
	endif()
	CHECK_C_COMPILER_FLAG (-Wimplicit-function-declaration Wimplicit-function-declaration)
	if(Wimplicit-function-declaration)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimplicit-function-declaration")
	endif()

	if (NOT OPENBSD)
		CHECK_C_COMPILER_FLAG (-Wredundant-decls Wredundant-decls)
		if(Wredundant-decls)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wredundant-decls")
		endif()
	endif()
	if(CMAKE_BUILD_TYPE STREQUAL "Release")
		add_definitions(-DNDEBUG)
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
	endif()
endif()

# When building with Unix Makefiles and doing any release builds
# try to set __FILE__ to relative paths via a make specific macro
if (CMAKE_GENERATOR MATCHES "Unix Makefile*")
	if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		string(TOUPPER ${CMAKE_BUILD_TYPE} UPPER_BUILD_TYPE)
		CHECK_C_COMPILER_FLAG (-Wno-builtin-macro-redefined Wno-builtin-macro-redefined)
		if(Wno-builtin-macro-redefined)
			set(CMAKE_C_FLAGS_${UPPER_BUILD_TYPE} "${CMAKE_C_FLAGS_${UPPER_BUILD_TYPE}} -Wno-builtin-macro-redefined -D__FILE__='\"$(subst ${CMAKE_BINARY_DIR}/,,$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<)))\"'")
		endif()

		CHECK_CXX_COMPILER_FLAG (-Wno-builtin-macro-redefined Wno-builtin-macro-redefinedCXX)
		if(Wno-builtin-macro-redefinedCXX)
			set(CMAKE_CXX_FLAGS_${UPPER_BUILD_TYPE} "${CMAKE_CXX_FLAGS_${UPPER_BUILD_TYPE}} -Wno-builtin-macro-redefined -D__FILE__='\"$(subst ${CMAKE_BINARY_DIR}/,,$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<)))\"'")
		endif()
	endif()
endif()

if(${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-macros -Wno-padded")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-c11-extensions -Wno-gnu")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-command-line-argument")
	CHECK_C_COMPILER_FLAG(-Wno-deprecated-declarations Wno-deprecated-declarations)
	if(Wno-deprecated-declarations)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
	endif()
	CHECK_CXX_COMPILER_FLAG(-Wno-deprecated-declarations Wno-deprecated-declarationsCXX)
	if(Wno-deprecated-declarationsCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
	endif()
endif()

# Enable address sanitizer, where supported and when required
if(${CMAKE_C_COMPILER_ID} STREQUAL "Clang" OR CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_REQUIRED_FLAGS_SAVED ${CMAKE_REQUIRED_FLAGS})

	CHECK_C_COMPILER_FLAG ("-fno-omit-frame-pointer" fno-omit-frame-pointer)

	file(WRITE ${CMAKE_BINARY_DIR}/foo.txt "")
	if(WITH_SANITIZE_ADDRESS)
		set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
		CHECK_C_COMPILER_FLAG ("-fsanitize=address" fsanitize-address)
		CHECK_C_COMPILER_FLAG ("-fsanitize-blacklist=${CMAKE_BINARY_DIR}/foo.txt" fsanitize-blacklist)
		CHECK_C_COMPILER_FLAG ("-fsanitize-address-use-after-scope" fsanitize-address-use-after-scope)
		unset(CMAKE_REQUIRED_FLAGS)

		if(fsanitize-address)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

			if(fsanitize-blacklist)
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/scripts/blacklist-address-sanitizer.txt")
			endif(fsanitize-blacklist)

			if(fsanitize-address-use-after-scope)
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-address-use-after-scope")
			endif(fsanitize-address-use-after-scope)

		else(fsanitize-address)
			message(WARNING "Missing support for address sanitizer!")
		endif(fsanitize-address)

		if(fno-omit-frame-pointer)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
		endif()
	elseif(WITH_SANITIZE_MEMORY)
		set(CMAKE_REQUIRED_FLAGS "-fsanitize=memory")
		CHECK_C_COMPILER_FLAG ("-fsanitize=memory" fsanitize-memory)
		CHECK_C_COMPILER_FLAG ("-fsanitize-blacklist=${CMAKE_BINARY_DIR}/foo.txt" fsanitize-blacklist)
		CHECK_C_COMPILER_FLAG ("-fsanitize-memory-use-after-dtor" fsanitize-memory-use-after-dtor)
		CHECK_C_COMPILER_FLAG ("-fsanitize-memory-track-origins" fsanitize-memory-track-origins)
		unset(CMAKE_REQUIRED_FLAGS)

		if(fsanitize-memory)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=memory")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory")

			if(fsanitize-blacklist)
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/scripts/blacklist-memory-sanitizer.txt")
			endif(fsanitize-blacklist)

			if (fsanitize-memory-use-after-dtor)
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-memory-use-after-dtor")
			endif(fsanitize-memory-use-after-dtor)

			if (fsanitize-memory-track-origins)
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-memory-track-origins")
			endif(fsanitize-memory-track-origins)

		else(fsanitize-memory)
			message(WARNING "Missing support for memory sanitizer!")
		endif(fsanitize-memory)

		if(fno-omit-frame-pointer)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
		endif()
	elseif(WITH_SANITIZE_THREAD)
		CHECK_C_COMPILER_FLAG ("-fsanitize=thread" fsanitize-thread)
		CHECK_C_COMPILER_FLAG ("-fsanitize-blacklist=${CMAKE_BINARY_DIR}/foo.txt" fsanitize-blacklist)
		unset(CMAKE_REQUIRED_FLAGS)
		if(fsanitize-thread)
			set(CMAKE_REQUIRED_FLAGS "-Werror -fsanitize=thread")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
			if(fsanitize-blacklist)
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/scripts/blacklist-thread-sanitizer.txt")
			endif(fsanitize-blacklist)
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
		else(fsanitize-thread)
			message(WARNING "Missing support for thread sanitizer!")
		endif(fsanitize-thread)

		if(fno-omit-frame-pointer)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
		endif()
	endif()

	file(REMOVE ${CMAKE_BINARY_DIR}/foo.txt)

	if (WITH_NO_UNDEFINED)
		set(CMAKE_REQUIRED_FLAGS "-Wl,--no-undefined")
		CHECK_C_COMPILER_FLAG (-Wl,--no-undefined no-undefined)
		unset(CMAKE_REQUIRED_FLAGS)

		if(no-undefined)
			SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined" )
		endif()
	endif()

	set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVED})
endif()

if(MSVC)
	# Remove previous warning definitions,
	# NMake is otherwise complaining.
	foreach (flags_var_to_scrub
		CMAKE_C_FLAGS
		CMAKE_CXX_FLAGS
		CMAKE_CXX_FLAGS_RELEASE
		CMAKE_CXX_FLAGS_RELWITHDEBINFO
		CMAKE_CXX_FLAGS_MINSIZEREL
		CMAKE_C_FLAGS_RELEASE
		CMAKE_C_FLAGS_RELWITHDEBINFO
		CMAKE_C_FLAGS_MINSIZEREL)
		string (REGEX REPLACE "(^| )[/-]W[ ]*[1-9]" " "
		"${flags_var_to_scrub}" "${${flags_var_to_scrub}}")
	endforeach()

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gd")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		add_definitions(-D_AMD64_)
	else()
		add_definitions(-D_X86_)
	endif()

	set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
	set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})

	if(CMAKE_BUILD_TYPE STREQUAL "Release")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zi")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
	endif()

endif()

if(WIN32)
	add_definitions(-DUNICODE -D_UNICODE)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)

	set(CMAKE_USE_RELATIVE_PATH ON)
	if (${CMAKE_GENERATOR} MATCHES "NMake Makefile*" OR ${CMAKE_GENERATOR} MATCHES "Ninja*" OR ${CMAKE_GENERATOR} MATCHES "Unix Makefiles")
		set(CMAKE_PDB_BINARY_DIR ${CMAKE_BINARY_DIR})
	elseif (${CMAKE_GENERATOR} MATCHES "Visual Studio*")
		set(CMAKE_PDB_BINARY_DIR "${CMAKE_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}")
	else()
		message(FATAL_ERROR "Unknown generator ${CMAKE_GENERATOR}")
	endif()

	string(TIMESTAMP RC_VERSION_YEAR "%Y")

	if(NOT DEFINED CMAKE_WINDOWS_VERSION)
		set(CMAKE_WINDOWS_VERSION "WIN7")
	endif()

	if(CMAKE_WINDOWS_VERSION STREQUAL "WINXP")
		add_definitions(-DWINVER=0x0501 -D_WIN32_WINNT=0x0501)
	elseif(CMAKE_WINDOWS_VERSION STREQUAL "WIN7")
		add_definitions(-DWINVER=0x0601 -D_WIN32_WINNT=0x0601)
	elseif(CMAKE_WINDOWS_VERSION STREQUAL "WIN8")
		add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
	elseif(CMAKE_WINDOWS_VERSION STREQUAL "WIN10")
		add_definitions(-DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)
	endif()

	# Set product and vendor for dll and exe version information.
	set(RC_VERSION_VENDOR ${VENDOR})
	set(RC_VERSION_PRODUCT ${PRODUCT})
	set(RC_VERSION_PATCH ${BUILD_NUMBER})
		set(RC_VERSION_DESCRIPTION "${FREERDP_VERSION_FULL} ${GIT_REVISION} ${CMAKE_WINDOWS_VERSION} ${CMAKE_SYSTEM_PROCESSOR}")

	if (FREERDP_EXTERNAL_SSL_PATH)
		set(OPENSSL_ROOT_DIR ${FREERDP_EXTERNAL_SSL_PATH})
	endif()
endif()

if(IOS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isysroot ${CMAKE_IOS_SDK_ROOT} -g")
endif()

add_definitions(-DWINPR_EXPORTS -DFREERDP_EXPORTS)

# Include files
if(NOT IOS)
	check_include_files(fcntl.h HAVE_FCNTL_H)
	check_include_files(unistd.h HAVE_UNISTD_H)
	check_include_files(execinfo.h HAVE_EXECINFO_H)
	check_include_files(inttypes.h HAVE_INTTYPES_H)
	check_include_files(sys/modem.h HAVE_SYS_MODEM_H)
	check_include_files(sys/filio.h HAVE_SYS_FILIO_H)
	check_include_files(sys/sockio.h HAVE_SYS_SOCKIO_H)
	check_include_files(sys/strtio.h HAVE_SYS_STRTIO_H)
	check_include_files(sys/select.h HAVE_SYS_SELECT_H)
	check_include_files(syslog.h HAVE_SYSLOG_H)
else()
	set(HAVE_FCNTL_H 1)
	set(HAVE_UNISTD_H 1)
	set(HAVE_INTTYPES_H 1)
	set(HAVE_SYS_FILIO_H 1)
endif()

if(NOT IOS)
	check_struct_has_member("struct tm" tm_gmtoff time.h HAVE_TM_GMTOFF)
else()
	set(HAVE_TM_GMTOFF 1)
endif()

# Mac OS X
if(APPLE)
	if(IOS)
		if (NOT FREERDP_IOS_EXTERNAL_SSL_PATH)
			message(STATUS "FREERDP_IOS_EXTERNAL_SSL_PATH not set! Required if openssl is not found in the iOS SDK (which usually isn't")
		endif()
		set(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${FREERDP_IOS_EXTERNAL_SSL_PATH})
		set_property(GLOBAL PROPERTY XCODE_ATTRIBUTE_SKIP_INSTALL YES)
	else(IOS)
		if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
			set(CMAKE_OSX_ARCHITECTURES i386 x86_64)
		endif()
	endif(IOS)

# Temporarily disabled, causes the cmake script to be reexecuted, causing the compilation to fail.
# Workaround: specify the parameter in the command-line
#	if(WITH_CLANG)
#		set(CMAKE_C_COMPILER "clang")
#	endif()

	if (WITH_VERBOSE)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -v")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -v")
	endif()
endif(APPLE)

# OpenBSD
if(OPENBSD)
	set(WITH_MANPAGES "ON")
	set(WITH_ALSA "OFF")
	set(WITH_PULSE "OFF")
	set(WITH_OSS "ON")
	set(WITH_WAYLAND "OFF")
endif()

# Android
if(ANDROID)
	set(WITH_LIBRARY_VERSIONING "OFF")

	set_property( GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ${ANDROID_LIBRARY_USE_LIB64_PATHS} )

	if (${ANDROID_ABI} STREQUAL "armeabi")
		set (WITH_NEON OFF)
	endif()

	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		add_definitions(-DNDK_DEBUG=1)

		# NOTE: Manually add -gdwarf-3, as newer toolchains default to -gdwarf-4,
		#       which is not supported by the gdbserver binary shipped with
		#       the android NDK (tested with r9b)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -gdwarf-3")
	endif()
	set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -llog")

	if (NOT FREERDP_EXTERNAL_PATH)
		if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/external/")
			set (FREERDP_EXTERNAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/")
		else()
			message(STATUS "FREERDP_EXTERNAL_PATH not set!")
		endif()
	endif()

	list (APPEND CMAKE_INCLUDE_PATH ${FREERDP_EXTERNAL_PATH}/${ANDROID_ABI}/include)
	list (APPEND CMAKE_LIBRARY_PATH ${FREERDP_EXTERNAL_PATH}/${ANDROID_ABI}/ )
	set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH )
	set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH )

	if (WITH_GPROF)
		CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/scripts/gprof_generate.sh.cmake
			${CMAKE_BINARY_DIR}/scripts/gprof_generate.sh @ONLY)
	endif(WITH_GPROF)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

if(NOT IOS)
	find_package(Threads REQUIRED)
endif()

if(NOT WIN32)
	CHECK_SYMBOL_EXISTS(pthread_mutex_timedlock pthread.h HAVE_PTHREAD_MUTEX_TIMEDLOCK_SYMBOL)
	if (NOT HAVE_PTHREAD_MUTEX_TIMEDLOCK_SYMBOL)
		CHECK_LIBRARY_EXISTS(pthread pthread_mutex_timedlock "" HAVE_PTHREAD_MUTEX_TIMEDLOCK_LIB)
	endif (NOT HAVE_PTHREAD_MUTEX_TIMEDLOCK_SYMBOL)
	if (NOT HAVE_PTHREAD_MUTEX_TIMEDLOCK_LIB)
		CHECK_LIBRARY_EXISTS(pthreads pthread_mutex_timedlock "" HAVE_PTHREAD_MUTEX_TIMEDLOCK_LIBS)
	endif (NOT HAVE_PTHREAD_MUTEX_TIMEDLOCK_LIB)

	if (HAVE_PTHREAD_MUTEX_TIMEDLOCK_SYMBOL OR HAVE_PTHREAD_MUTEX_TIMEDLOCK_LIB OR HAVE_PTHREAD_MUTEX_TIMEDLOCK_LIBS)
		set(HAVE_PTHREAD_MUTEX_TIMEDLOCK ON)
	endif (HAVE_PTHREAD_MUTEX_TIMEDLOCK_SYMBOL OR HAVE_PTHREAD_MUTEX_TIMEDLOCK_LIB OR HAVE_PTHREAD_MUTEX_TIMEDLOCK_LIBS)
endif()

if(WITH_VALGRIND_MEMCHECK)
	check_include_files(valgrind/memcheck.h HAVE_VALGRIND_MEMCHECK_H)
else()
	unset(HAVE_VALGRIND_MEMCHECK_H CACHE)
endif()

if(UNIX OR CYGWIN)
	check_include_files(aio.h HAVE_AIO_H)
	check_include_files(sys/eventfd.h HAVE_SYS_EVENTFD_H)
	if (HAVE_SYS_EVENTFD_H)
		check_symbol_exists(eventfd_read sys/eventfd.h WITH_EVENTFD_READ_WRITE)
	endif()
	if (FREEBSD)
		list(APPEND CMAKE_REQUIRED_INCLUDES ${EPOLLSHIM_INCLUDE_DIR})
	endif()
	check_include_files(sys/timerfd.h HAVE_SYS_TIMERFD_H)
	if (FREEBSD)
		list(REMOVE_ITEM CMAKE_REQUIRED_INCLUDES ${EPOLLSHIM_INCLUDE_DIR})
	endif()
	check_include_files(poll.h HAVE_POLL_H)
	list(APPEND CMAKE_REQUIRED_LIBRARIES m)
	check_symbol_exists(ceill math.h HAVE_MATH_C99_LONG_DOUBLE)
	list(REMOVE_ITEM CMAKE_REQUIRED_LIBRARIES m)
	set(X11_FEATURE_TYPE "RECOMMENDED")
	set(WAYLAND_FEATURE_TYPE "RECOMMENDED")
else()
	set(X11_FEATURE_TYPE "DISABLED")
	set(WAYLAND_FEATURE_TYPE "DISABLED")
endif()

if(WITH_PCSC_WINPR)
	find_package(PCSCWinPR)
endif()

set(X11_FEATURE_PURPOSE "X11")
set(X11_FEATURE_DESCRIPTION "X11 client and server")

set(WAYLAND_FEATURE_PURPOSE "Wayland")
set(WAYLAND_FEATURE_DESCRIPTION "Wayland client")

set(ZLIB_FEATURE_TYPE "REQUIRED")
set(ZLIB_FEATURE_PURPOSE "compression")
set(ZLIB_FEATURE_DESCRIPTION "data compression")

set(OPENSSL_FEATURE_TYPE "REQUIRED")
set(OPENSSL_FEATURE_PURPOSE "cryptography")
set(OPENSSL_FEATURE_DESCRIPTION "encryption, certificate validation, hashing functions")

set(MBEDTLS_FEATURE_TYPE "OPTIONAL")
set(MBEDTLS_FEATURE_PURPOSE "cryptography")
set(MBEDTLS_FEATURE_DESCRIPTION "encryption, certificate validation, hashing functions")

set(OPENSLES_FEATURE_TYPE "OPTIONAL")
set(OPENSLES_FEATURE_PURPOSE "multimedia")
set(OPENSLES_FEATURE_DESCRIPTION "OpenSLES audio / video")

set(OSS_FEATURE_TYPE "RECOMMENDED")
set(OSS_FEATURE_PURPOSE "sound")
set(OSS_FEATURE_DESCRIPTION "audio input, audio output and multimedia redirection")

set(ALSA_FEATURE_TYPE "RECOMMENDED")
set(ALSA_FEATURE_PURPOSE "sound")
set(ALSA_FEATURE_DESCRIPTION "audio input, audio output and multimedia redirection")

set(PULSE_FEATURE_TYPE "RECOMMENDED")
set(PULSE_FEATURE_PURPOSE "sound")
set(PULSE_FEATURE_DESCRIPTION "audio input, audio output and multimedia redirection")

set(CUPS_FEATURE_TYPE "RECOMMENDED")
set(CUPS_FEATURE_PURPOSE "printing")
set(CUPS_FEATURE_DESCRIPTION "printer device redirection")

set(PCSC_FEATURE_TYPE "RECOMMENDED")
set(PCSC_FEATURE_PURPOSE "smart card")
set(PCSC_FEATURE_DESCRIPTION "smart card device redirection")

set(FFMPEG_FEATURE_TYPE "RECOMMENDED")
set(FFMPEG_FEATURE_PURPOSE "multimedia")
set(FFMPEG_FEATURE_DESCRIPTION "multimedia redirection, audio and video playback")

set(VAAPI_FEATURE_TYPE "OPTIONAL")
set(VAAPI_FEATURE_PURPOSE "multimedia")
set(VAAPI_FEATURE_DESCRIPTION "VA-API hardware acceleration for video playback")

set(IPP_FEATURE_TYPE "OPTIONAL")
set(IPP_FEATURE_PURPOSE "performance")
set(IPP_FEATURE_DESCRIPTION "Intel Integrated Performance Primitives library")

set(JPEG_FEATURE_TYPE "OPTIONAL")
set(JPEG_FEATURE_PURPOSE "codec")
set(JPEG_FEATURE_DESCRIPTION "use JPEG library")

set(X264_FEATURE_TYPE "OPTIONAL")
set(X264_FEATURE_PURPOSE "codec")
set(X264_FEATURE_DESCRIPTION "use x264 library")

set(OPENH264_FEATURE_TYPE "OPTIONAL")
set(OPENH264_FEATURE_PURPOSE "codec")
set(OPENH264_FEATURE_DESCRIPTION "use OpenH264 library")

set(OPENCL_FEATURE_TYPE "OPTIONAL")
set(OPENCL_FEATURE_PURPOSE "codec")
set(OPENCL_FEATURE_DESCRIPTION "use OpenCL library")

set(GSM_FEATURE_TYPE "OPTIONAL")
set(GSM_FEATURE_PURPOSE "codec")
set(GSM_FEATURE_DESCRIPTION "GSM audio codec library")

set(LAME_FEATURE_TYPE "OPTIONAL")
set(LAME_FEATURE_PURPOSE "codec")
set(LAME_FEATURE_DESCRIPTION "lame MP3 audio codec library")

set(FAAD2_FEATURE_TYPE "OPTIONAL")
set(FAAD2_FEATURE_PURPOSE "codec")
set(FAAD2_FEATURE_DESCRIPTION "FAAD2 AAC audio codec library")

set(FAAC_FEATURE_TYPE "OPTIONAL")
set(FAAC_FEATURE_PURPOSE "codec")
set(FAAC_FEATURE_DESCRIPTION "FAAC AAC audio codec library")

set(SOXR_FEATURE_TYPE "OPTIONAL")
set(SOXR_FEATURE_PURPOSE "codec")
set(SOXR_FEATURE_DESCRIPTION "SOX audio resample library")

set(GSSAPI_FEATURE_TYPE "OPTIONAL")
set(GSSAPI_FEATURE_PURPOSE "auth")
set(GSSAPI_FEATURE_DESCRIPTION "add kerberos support")

if(WIN32)
	set(X11_FEATURE_TYPE "DISABLED")
	set(WAYLAND_FEATURE_TYPE "DISABLED")
	set(ZLIB_FEATURE_TYPE "DISABLED")
	set(OSS_FEATURE_TYPE "DISABLED")
	set(ALSA_FEATURE_TYPE "DISABLED")
	set(PULSE_FEATURE_TYPE "DISABLED")
	set(CUPS_FEATURE_TYPE "DISABLED")
	set(PCSC_FEATURE_TYPE "DISABLED")
	set(FFMPEG_FEATURE_TYPE "DISABLED")
	set(VAAPI_FEATURE_TYPE "DISABLED")
	set(OPENSLES_FEATURE_TYPE "DISABLED")
endif()

if(APPLE)
	set(FFMPEG_FEATURE_TYPE "OPTIONAL")
	set(VAAPI_FEATURE_TYPE "DISABLED")
	set(X11_FEATURE_TYPE "OPTIONAL")
	set(WAYLAND_FEATURE_TYPE "DISABLED")
	set(OSS_FEATURE_TYPE "DISABLED")
	set(ALSA_FEATURE_TYPE "DISABLED")
	if(IOS)
		set(X11_FEATURE_TYPE "DISABLED")
		set(PULSE_FEATURE_TYPE "DISABLED")
		set(CUPS_FEATURE_TYPE "DISABLED")
		set(PCSC_FEATURE_TYPE "DISABLED")
	endif()
	set(OPENSLES_FEATURE_TYPE "DISABLED")
endif()

if(UNIX AND NOT ANDROID)
	set(WLOG_SYSTEMD_JOURNAL_FEATURE_TYPE "RECOMMENDED")
	set(WLOG_SYSTEMD_JOURNAL_FEATURE_PURPOSE "systemd journal appender")
	set(WLOG_SYSTEMD_JOURNAL_FEATURE_DESCRIPTION "allows to export wLog to systemd journal")

	#include(Findlibsystemd)
	find_feature(libsystemd ${WLOG_SYSTEMD_JOURNAL_FEATURE_TYPE} ${WLOG_SYSTEMD_JOURNAL_FEATURE_PURPOSE} ${WLOG_SYSTEMD_JOURNAL_FEATURE_DESCRIPTION})

	if(LIBSYSTEMD_FOUND)
		set(HAVE_JOURNALD_H TRUE)
	else()
		unset(HAVE_JOURNALD_H)
	endif()
endif(UNIX AND NOT ANDROID)

if(ANDROID)
	set(X11_FEATURE_TYPE "DISABLED")
	set(WAYLAND_FEATURE_TYPE "DISABLED")
	set(OSS_FEATURE_TYPE "DISABLED")
	set(ALSA_FEATURE_TYPE "DISABLED")
	set(PULSE_FEATURE_TYPE "DISABLED")
	set(CUPS_FEATURE_TYPE "DISABLED")
	set(PCSC_FEATURE_TYPE "DISABLED")
	set(FFMPEG_FEATURE_TYPE "DISABLED")
	set(VAAPI_FEATURE_TYPE "DISABLED")
	set(OPENSLES_FEATURE_TYPE "REQUIRED")
endif()

find_feature(X11 ${X11_FEATURE_TYPE} ${X11_FEATURE_PURPOSE} ${X11_FEATURE_DESCRIPTION})
find_feature(Wayland ${WAYLAND_FEATURE_TYPE} ${WAYLAND_FEATURE_PURPOSE} ${WAYLAND_FEATURE_DESCRIPTION})

find_feature(ZLIB ${ZLIB_FEATURE_TYPE} ${ZLIB_FEATURE_PURPOSE} ${ZLIB_FEATURE_DESCRIPTION})
find_feature(OpenSSL ${OPENSSL_FEATURE_TYPE} ${OPENSSL_FEATURE_PURPOSE} ${OPENSSL_FEATURE_DESCRIPTION})
find_feature(MbedTLS ${MBEDTLS_FEATURE_TYPE} ${MBEDTLS_FEATURE_PURPOSE} ${MBEDTLS_FEATURE_DESCRIPTION})
find_feature(OpenSLES ${OPENSLES_FEATURE_TYPE} ${OPENSLES_FEATURE_PURPOSE} ${OPENSLES_FEATURE_DESCRIPTION})

find_feature(OSS ${OSS_FEATURE_TYPE} ${OSS_FEATURE_PURPOSE} ${OSS_FEATURE_DESCRIPTION})
find_feature(ALSA ${ALSA_FEATURE_TYPE} ${ALSA_FEATURE_PURPOSE} ${ALSA_FEATURE_DESCRIPTION})
find_feature(Pulse ${PULSE_FEATURE_TYPE} ${PULSE_FEATURE_PURPOSE} ${PULSE_FEATURE_DESCRIPTION})

find_feature(Cups ${CUPS_FEATURE_TYPE} ${CUPS_FEATURE_PURPOSE} ${CUPS_FEATURE_DESCRIPTION})
find_feature(PCSC ${PCSC_FEATURE_TYPE} ${PCSC_FEATURE_PURPOSE} ${PCSC_FEATURE_DESCRIPTION})

find_feature(FFmpeg ${FFMPEG_FEATURE_TYPE} ${FFMPEG_FEATURE_PURPOSE} ${FFMPEG_FEATURE_DESCRIPTION})

find_feature(JPEG ${JPEG_FEATURE_TYPE} ${JPEG_FEATURE_PURPOSE} ${JPEG_FEATURE_DESCRIPTION})
find_feature(x264 ${X264_FEATURE_TYPE} ${X264_FEATURE_PURPOSE} ${X264_FEATURE_DESCRIPTION})
find_feature(OpenH264 ${OPENH264_FEATURE_TYPE} ${OPENH264_FEATURE_PURPOSE} ${OPENH264_FEATURE_DESCRIPTION})
find_feature(OpenCL ${OPENCL_FEATURE_TYPE} ${OPENCL_FEATURE_PURPOSE} ${OPENCL_FEATURE_DESCRIPTION})
find_feature(GSM ${GSM_FEATURE_TYPE} ${GSM_FEATURE_PURPOSE} ${GSM_FEATURE_DESCRIPTION})
find_feature(LAME ${LAME_FEATURE_TYPE} ${LAME_FEATURE_PURPOSE} ${LAME_FEATURE_DESCRIPTION})
find_feature(FAAD2 ${FAAD2_FEATURE_TYPE} ${FAAD2_FEATURE_PURPOSE} ${FAAD2_FEATURE_DESCRIPTION})
find_feature(FAAC ${FAAC_FEATURE_TYPE} ${FAAC_FEATURE_PURPOSE} ${FAAC_FEATURE_DESCRIPTION})
find_feature(soxr ${SOXR_FEATURE_TYPE} ${SOXR_FEATURE_PURPOSE} ${SOXR_FEATURE_DESCRIPTION})

find_feature(GSSAPI ${GSSAPI_FEATURE_TYPE} ${GSSAPI_FEATURE_PURPOSE} ${GSSAPI_FEATURE_DESCRIPTION})

if (WITH_OPENH264 AND NOT WITH_OPENH264_LOADING)
	option(WITH_OPENH264_LOADING "Use LoadLibrary to load openh264 at runtime" OFF)
endif (WITH_OPENH264 AND NOT WITH_OPENH264_LOADING)

if ((WITH_FFMPEG OR WITH_DSP_FFMPEG) AND NOT FFMPEG_FOUND)
	message(FATAL_ERROR "FFMPEG support requested but not detected")
endif()
set(WITH_FFMPEG ${FFMPEG_FOUND})

# Version check, if we have detected FFMPEG but the version is too old
# deactivate it as sound backend.
if (WITH_DSP_FFMPEG)
	# Deactivate FFmpeg backend for sound, if the version is too old.
	# See libfreerdp/codec/dsp_ffmpeg.h
	file(STRINGS "${AVCODEC_INCLUDE_DIR}/libavcodec/version.h" AV_VERSION_FILE REGEX "LIBAVCODEC_VERSION_M[A-Z]+[\t ]*[0-9]+")
	FOREACH(item ${AV_VERSION_FILE})
	  STRING(REGEX MATCH "LIBAVCODEC_VERSION_M[A-Z]+[\t ]*[0-9]+" litem ${item})
	IF(litem)
			string(REGEX REPLACE "[ \t]+" ";" VSPLIT_LINE ${litem})
			list(LENGTH VSPLIT_LINE VSPLIT_LINE_LEN)
			if (NOT "${VSPLIT_LINE_LEN}" EQUAL "2")
				message(ERROR "invalid entry in libavcodec version header ${item}")
			endif(NOT "${VSPLIT_LINE_LEN}" EQUAL "2")
			list(GET VSPLIT_LINE 0 VNAME)
			list(GET VSPLIT_LINE 1 VVALUE)
			set(${VNAME} ${VVALUE})
	ENDIF(litem)
	ENDFOREACH(item ${AV_VERSION_FILE})

	set(AVCODEC_VERSION "${LIBAVCODEC_VERSION_MAJOR}.${LIBAVCODEC_VERSION_MINOR}.${LIBAVCODEC_VERSION_MICRO}")
	if (AVCODEC_VERSION VERSION_LESS "57.48.101")
		message(WARNING "FFmpeg version detected (${AVCODEC_VERSION}) is too old. (Require at least 57.48.101 for sound). Deactivating")
		set(WITH_DSP_FFMPEG OFF)
	endif()
endif (WITH_DSP_FFMPEG)

if (WITH_OPENH264 AND NOT OPENH264_FOUND)
	message(FATAL_ERROR "OpenH264 support requested but not detected")
endif()
set(WITH_OPENH264 ${OPENH264_FOUND})

if ( (WITH_GSSAPI) AND (NOT GSS_FOUND))
	message(WARNING "-DWITH_GSSAPI=ON is set, but not GSSAPI implementation was found, disabling")
elseif(WITH_GSSAPI)
	if(GSS_FLAVOUR STREQUAL "MIT")
		add_definitions("-DWITH_GSSAPI -DWITH_GSSAPI_MIT")
		if(GSS_VERSION_1_13)
			add_definitions("-DHAVE_AT_LEAST_KRB_V1_13")
		endif()
		include_directories(${_GSS_INCLUDE_DIR})
	elseif(GSS_FLAVOUR STREQUAL "Heimdal")
		add_definitions("-DWITH_GSSAPI -DWITH_GSSAPI_HEIMDAL")
		include_directories(${_GSS_INCLUDE_DIR})
	else()
		message(WARNING "Kerberos version not detected")
	endif()
endif()

if(TARGET_ARCH MATCHES "x86|x64")
	if (NOT APPLE)
		# Intel Performance Primitives
		find_feature(IPP ${IPP_FEATURE_TYPE} ${IPP_FEATURE_PURPOSE} ${IPP_FEATURE_DESCRIPTION})
	endif()
endif()

if(OPENSSL_FOUND)
	add_definitions("-DWITH_OPENSSL")
	message(STATUS "Using OpenSSL Version: ${OPENSSL_VERSION}")
	include_directories(${OPENSSL_INCLUDE_DIR})
endif()

if(MBEDTLS_FOUND)
	add_definitions("-DWITH_MBEDTLS")
endif()

if (TARGET_ARCH MATCHES "sparc")
	set(HAVE_ALIGNED_REQUIRED 1)
endif()

if (WITH_X264 OR WITH_OPENH264 OR WITH_MEDIA_FOUNDATION OR WITH_FFMPEG)
	set(WITH_GFX_H264 ON)
else()
	set(WITH_GFX_H264 OFF)
endif()

# Android expects all libraries to be loadable
# without paths.
if (ANDROID OR WIN32 OR MAC_BUNDLE)
  set(FREERDP_DATA_PATH "share")
  if (NOT FREERDP_INSTALL_PREFIX)
	  set(FREERDP_INSTALL_PREFIX ".")
  endif()
  set(FREERDP_LIBRARY_PATH ".")
  set(FREERDP_PLUGIN_PATH ".")
else()
	set(FREERDP_DATA_PATH "${CMAKE_INSTALL_PREFIX}/share/freerdp${FREERDP_VERSION_MAJOR}")
	if (NOT FREERDP_INSTALL_PREFIX)
		set(FREERDP_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
	endif()
	set(FREERDP_LIBRARY_PATH "${CMAKE_INSTALL_LIBDIR}")
	set(FREERDP_PLUGIN_PATH "${CMAKE_INSTALL_LIBDIR}/freerdp${FREERDP_VERSION_MAJOR}")
endif()
set(FREERDP_ADDIN_PATH "${FREERDP_PLUGIN_PATH}")

# Path to put extensions
set(FREERDP_EXTENSION_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/freerdp${FREERDP_VERSION_MAJOR}/extensions")

# Proxy plugins path
if(NOT DEFINED PROXY_PLUGINDIR)
	message("using default plugins location")
  	set(FREERDP_PROXY_PLUGINDIR "${CMAKE_BINARY_DIR}/server/proxy/plugins")
else()
	set(FREERDP_PROXY_PLUGINDIR "${PROXY_PLUGINDIR}")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Configure files
add_definitions("-DHAVE_CONFIG_H")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# RPATH configuration
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
if (APPLE)
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
	set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
else (APPLE)
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	if (NOT FREEBSD)
		set(CMAKE_INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:\$ORIGIN/..")
	endif()
endif(APPLE)

if (BUILD_SHARED_LIBS)
	set(CMAKE_MACOSX_RPATH ON)
endif()

# Android profiling
if(ANDROID)
	if(WITH_GPROF)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
		set(PROFILER_LIBRARIES
			"${FREERDP_EXTERNAL_PROFILER_PATH}/obj/local/${ANDROID_ABI}/libandroid-ndk-profiler.a")
		include_directories("${FREERDP_EXTERNAL_PROFILER_PATH}")
	endif()
endif()

# Unit Tests

include(CTest)

if(BUILD_TESTING)
	enable_testing()

	if(MSVC)
		set(TESTING_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
	else()
		set(TESTING_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Testing")
	endif()
endif()

# WinPR
include_directories("${CMAKE_SOURCE_DIR}/winpr/include")
include_directories("${CMAKE_BINARY_DIR}/winpr/include")

if (${CMAKE_VERSION} VERSION_LESS 2.8.12)
	set(PUBLIC_KEYWORD "")
	set(PRIVATE_KEYWORD "")
else()
	set(PUBLIC_KEYWORD "PUBLIC")
	set(PRIVATE_KEYWORD "PRIVATE")
endif()

if(BUILD_SHARED_LIBS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINPR_DLL")
endif()

add_subdirectory(winpr)

# Sub-directories

if(WITH_THIRD_PARTY)
	add_subdirectory(third-party)
	if (NOT "${THIRD_PARTY_INCLUDES}" STREQUAL "")
		include_directories(${THIRD_PARTY_INCLUDES})
	endif()
endif()

add_subdirectory(include)

add_subdirectory(libfreerdp)

if (IOS)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.0")
	if (IOS_PLATFORM MATCHES "SIMULATOR")
		set(CMAKE_OSX_SYSROOT "iphonesimulator")
	else()
		set(CMAKE_OSX_SYSROOT "iphoneos")
	endif()
endif()

# RdTk
include_directories("${CMAKE_SOURCE_DIR}/rdtk/include")
include_directories("${CMAKE_BINARY_DIR}/rdtk/include")

add_subdirectory(rdtk)

if(WAYLAND_FOUND)
	add_subdirectory(uwac)
endif()

if(BSD)
	if(IS_DIRECTORY /usr/local/include)
		include_directories(/usr/local/include)
		link_directories(/usr/local/lib)
	endif()
	if(OPENBSD)
		if(IS_DIRECTORY /usr/X11R6/include)
			include_directories(/usr/X11R6/include)
		endif()
	endif()
endif()

if(WITH_CHANNELS)
	add_subdirectory(channels)
endif()

if(WITH_CLIENT_COMMON OR WITH_CLIENT)
add_subdirectory(client)
endif()

if(WITH_SERVER)
	add_subdirectory(server)
endif()

# Packaging

set(CMAKE_CPACK_INCLUDE_FILE "CMakeCPack.cmake")

if(NOT (VENDOR MATCHES "FreeRDP"))
	if(DEFINED CLIENT_VENDOR_PATH)
		if(EXISTS "${CMAKE_SOURCE_DIR}/${CLIENT_VENDOR_PATH}/CMakeCPack.cmake")
			set(CMAKE_CPACK_INCLUDE_FILE "${CLIENT_VENDOR_PATH}/CMakeCPack.cmake")
		endif()
	endif()
endif()

#message("VENDOR: ${VENDOR} CLIENT_VENDOR_PATH: ${CLIENT_VENDOR_PATH} CMAKE_CPACK_INCLUDE_FILE: ${CMAKE_CPACK_INCLUDE_FILE}")

include(${CMAKE_CPACK_INCLUDE_FILE})

set(FREERDP_BUILD_CONFIG_LIST "")
GET_CMAKE_PROPERTY(res VARIABLES)
FOREACH(var ${res})
	IF (var MATCHES "^WITH_*|^BUILD_TESTING|^BUILTIN_CHANNELS|^HAVE_*")
		LIST(APPEND FREERDP_BUILD_CONFIG_LIST "${var}=${${var}}")
	ENDIF()
ENDFOREACH()
string(REPLACE ";" " " FREERDP_BUILD_CONFIG "${FREERDP_BUILD_CONFIG_LIST}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/buildflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/buildflags.h)
