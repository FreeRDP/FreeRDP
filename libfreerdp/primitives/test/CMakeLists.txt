# FreeRDP: A Remote Desktop Protocol Client
# primitives test makefile builder
# vi:ts=4 sw=4:
#
# (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing permissions
# and limitations under the License.
#

# TODO: Integrate this into the testing framework, in some form.
# Right now this produces a standalone test that covers both functionality
# and performance of the primitives library entrypoints.

cmake_minimum_required(VERSION 2.8)
set(MODULE_NAME "prim_test")
set(MODULE_PREFIX "PRIMITIVES_LIBRARY_TEST")

set(PRIMITIVE_TEST_CFILES
	prim_test.c
	test_add.c
	test_alphaComp.c
	test_andor.c
	test_colors.c
	test_copy.c
	test_set.c
	test_shift.c
	test_sign.c
	../prim_add.c
	../prim_andor.c
	../prim_alphaComp.c
	../prim_colors.c
	../prim_copy.c
	../prim_set.c
	../prim_shift.c
	../prim_sign.c
	../prim_add_opt.c
	../prim_alphaComp_opt.c
	../prim_andor_opt.c
	../prim_colors_opt.c
	../prim_set_opt.c
	../prim_shift_opt.c
	../prim_sign_opt.c
	../primitives.c
    )

set(PRIMITIVE_TEST_HEADERS
	measure.h
	prim_test.h
	../prim_internal.h
)

set(PRIMITIVE_TEST_SRCS
    ${PRIMITIVE_TEST_CFILES}
    ${PRIMITIVE_TEST_HEADERS}
    )

include_directories(. ../../.. ../../../include ../../../winpr/include)
add_definitions(-DPRIM_STATIC=auto -DALL_PRIMITIVES_VERSIONS -DHAVE_CONFIG_H)

# If these haven't been set by the caller, set them now to defaults.
if(NOT DEFINED WITH_IPP)
	set(WITH_IPP FALSE) 
endif()
if(NOT DEFINED WITH_SSE2)
	if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
		set(WITH_SSE2 FALSE)
	else()
		set(WITH_SSE2 TRUE)
	endif()
endif()
if(NOT DEFINED WITH_NEON)
	if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
		set(WITH_NEON TRUE)
	else()
		set(WITH_NEON FALSE)
	endif()
endif()

if(WITH_SSE2)
	if(CMAKE_COMPILER_IS_GNUCC)
		set(OPTFLAGS "${OPTFLAGS} -msse2 -mssse3 -O2 -Wdeclaration-after-statement")
	endif()

	if(MSVC)
		set(OPTFLAGS "${OPTFLAGS} /arch:SSE2")
	endif()
elseif(WITH_NEON)
	if(CMAKE_COMPILER_IS_GNUCC)
	    set(OPTFLAGS "${OPTFLAGS} -mfpu=neon -mfloat-abi=${ARM_FP_ABI} -O2")
	endif()
	# TODO: Add MSVC equivalent
endif()

add_executable(prim_test ${PRIMITIVE_TEST_SRCS})

if(WITH_IPP)
	if(NOT DEFINED IPP_FOUND)
		include(../../../cmake/FindIPP.cmake)
	endif()

	# IPP PATH debugging messages
    message(IPP_FOUND=${IPP_FOUND})
    message(IPP_VERSION_STR=${IPP_VERSION_STR})
    message(IPP_VERSION_MAJOR=${IPP_VERSION_MAJOR})
    message(IPP_VERSION_MINOR=${IPP_VERSION_MINOR})
    message(IPP_VERSION_BUILD=${IPP_VERSION_BUILD})
    message(IPP_ROOT_DIR=${IPP_ROOT_DIR})
    message(IPP_INCLUDE_DIRS=${IPP_INCLUDE_DIRS})
    message(IPP_LIBRARY_DIRS=${IPP_LIBRARY_DIRS})
    message(IPP_LIBRARIES=${IPP_LIBRARIES})
    message(IPP_COMPILER_LIBRARY_DIRS=${IPP_COMPILER_LIBRARY_DIRS})
    message(IPP_COMPILER_LIBRARIES=${IPP_COMPILER_LIBRARIES})
    message(IPP_LIBRARY_LIST=${IPP_LIBRARY_LIST})
    message(IPP_LIB_PREFIX=${IPP_LIB_PREFIX})
    message(IPP_LIB_SUFFIX=${IPP_LIB_SUFFIX})
    message(IPP_PREFIX=${IPP_PREFIX})
    message(IPP_SUFFIX=${IPP_SUFFIX})
    message(IPPCORE=${IPPCORE})
    message(IPPS=${IPPS})
    message(IPPI=${IPPI})
    message(IPPCC=${IPPCC})
    message(IPPCV=${IPPCV})
    message(IPPVM=${IPPVM})

	if(CMAKE_COMPILER_IS_GNUCC)
		foreach(INCLDIR ${IPP_INCLUDE_DIRS})
			set(OPTFLAGS "${OPTFLAGS} -I${INCLDIR}")
		endforeach(INCLDIR)
	endif()
	target_link_libraries(prim_test ${IPP_LIBRARY_LIST})
endif()

set_property(SOURCE ${PRIMITIVE_TEST_CFILES} PROPERTY COMPILE_FLAGS ${OPTFLAGS})

target_link_libraries(prim_test rt winpr-sysinfo)
if(NOT TESTING_OUTPUT_DIRECTORY)
	set(TESTING_OUTPUT_DIRECTORY .)
endif()
add_test(prim_test ${TESTING_OUTPUT_DIRECTORY}/prim_test functionality)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Test")
