# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp-codec cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(MODULE_NAME "freerdp-codec")
set(MODULE_PREFIX "FREERDP_CODEC")

set(${MODULE_PREFIX}_SRCS
	dsp.c
	color.c
	audio.c
	planar.c
	planar.h
	bitmap_decode.c
	bitmap_encode.c
	rfx_bitstream.h
	rfx_constants.h
	rfx_decode.c
	rfx_decode.h
	rfx_differential.c
	rfx_differential.h
	rfx_dwt.c
	rfx_dwt.h
	rfx_encode.c
	rfx_encode.h
	rfx_quantization.c
	rfx_quantization.h
	rfx_rlgr.c
	rfx_rlgr.h
	rfx_types.h
	rfx.c
	region.c	
	nsc.c
	nsc_encode.c
	nsc_encode.h
	nsc_types.h
	ncrush.c
	xcrush.c
	mppc.c
	jpeg.c)

set(${MODULE_PREFIX}_SSE2_SRCS
	rfx_sse2.c
	rfx_sse2.h
	nsc_sse2.c
	nsc_sse2.h)

set(${MODULE_PREFIX}_NEON_SRCS
	rfx_neon.c
	rfx_neon.h)

if(WITH_SSE2)
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_SSE2_SRCS})

	if(CMAKE_COMPILER_IS_GNUCC)
		set_source_files_properties(${${MODULE_PREFIX}_SSE2_SRCS} PROPERTIES COMPILE_FLAGS "-msse2")
	endif()

	if(MSVC)
		set_source_files_properties(${${MODULE_PREFIX}_SSE2_SRCS} PROPERTIES COMPILE_FLAGS "/arch:SSE2")
	endif()
endif()

if(WITH_NEON)
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_NEON_SRCS})
	set_source_files_properties(${${MODULE_PREFIX}_NEON_SRCS} PROPERTIES COMPILE_FLAGS "-mfpu=neon -mfloat-abi=${ARM_FP_ABI} -Wno-unused-variable")
endif()

if(WITH_JPEG)
    include_directories(${JPEG_INCLUDE_DIR})
    set(FREERDP_JPEG_LIBS ${JPEG_LIBRARIES})
endif()

add_complex_library(MODULE ${MODULE_NAME} TYPE "OBJECT"
	MONOLITHIC ${MONOLITHIC_BUILD}
	SOURCES ${${MODULE_PREFIX}_SRCS}
	EXPORT)

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION} PREFIX "lib")

set(${MODULE_PREFIX}_LIBS
	${FREERDP_JPEG_LIBS})

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD} INTERNAL
	MODULE freerdp
	MODULES freerdp-primitives freerdp-utils)
	
set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-crt winpr-pool winpr-registry winpr-sysinfo winpr-utils)

if(MONOLITHIC_BUILD)
	set(FREERDP_LIBS ${FREERDP_LIBS} ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)
else()
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT FreeRDPTargets)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/libfreerdp")

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

