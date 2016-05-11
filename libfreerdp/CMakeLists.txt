# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp cmake build script
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

set(MODULE_NAME "freerdp")
set(MODULE_PREFIX "FREERDP")

# Create imported targets for Intel IPP libraries

if(IPP_FOUND)
		foreach(ipp_lib ${IPP_LIBRARIES})
		add_library("${ipp_lib}_imported" STATIC IMPORTED)
		set_property(TARGET "${ipp_lib}_imported" PROPERTY IMPORTED_LOCATION "${IPP_LIBRARY_DIRS}/${ipp_lib}")
	endforeach()
endif()

set(LIBFREERDP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBFREERDP_SRCS "")
set(LIBFREERDP_LIBS "")
set(LIBFREERDP_INCLUDES "")
set(LIBFREERDP_DEFINITIONS "")

macro (freerdp_module_add)
		file (RELATIVE_PATH _relPath "${LIBFREERDP_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
		foreach (_src ${ARGN})
				if (_relPath)
						list (APPEND LIBFREERDP_SRCS "${_relPath}/${_src}")
				else()
						list (APPEND LIBFREERDP_SRCS "${_src}")
				endif()
		endforeach()
		if (_relPath)
				set (LIBFREERDP_SRCS ${LIBFREERDP_SRCS} PARENT_SCOPE)
		endif()
endmacro()

macro (freerdp_include_directory_add)
		file (RELATIVE_PATH _relPath "${LIBFREERDP_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
		foreach (_inc ${ARGN})
				if (IS_ABSOLUTE ${_inc})
							list (APPEND LIBFREERDP_INCLUDES "${_inc}")
				else()
					if (_relPath)
							list (APPEND LIBFREERDP_INCLUDES "${_relPath}/${_inc}")
					else()
							list (APPEND LIBFREERDP_INCLUDES "${_inc}")
					endif()
				endif()
		endforeach()
		if (_relPath)
				set (LIBFREERDP_INCLUDES ${LIBFREERDP_INCLUDES} PARENT_SCOPE)
		endif()
endmacro()

macro (freerdp_library_add)
		foreach (_lib ${ARGN})
						list (APPEND LIBFREERDP_LIBS "${_lib}")
		endforeach()
		 set (LIBFREERDP_LIBS ${LIBFREERDP_LIBS} PARENT_SCOPE)
endmacro()

macro (freerdp_definition_add)
		foreach (_define ${ARGN})
						list (APPEND LIBFREERDP_DEFINITIONS "${_define}")
		endforeach()
		 set (LIBFREERDP_DEFINITIONS ${LIBFREERDP_DEFINITIONS} PARENT_SCOPE)
endmacro()

set(${MODULE_PREFIX}_SUBMODULES
	utils
	common
	gdi
	cache
	crypto
	locale
	core)

foreach(${MODULE_PREFIX}_SUBMODULE ${${MODULE_PREFIX}_SUBMODULES})
	add_subdirectory(${${MODULE_PREFIX}_SUBMODULE})
endforeach()

## cmake source properties are only seen by targets in the same CMakeLists.txt
## therefore primitives and codecs need to be defined here

# codec
set(CODEC_SRCS
	codec/dsp.c
	codec/color.c
	codec/audio.c
	codec/planar.c
	codec/bitmap.c
	codec/interleaved.c
	codec/progressive.c
	codec/rfx_bitstream.h
	codec/rfx_constants.h
	codec/rfx_decode.c
	codec/rfx_decode.h
	codec/rfx_differential.c
	codec/rfx_differential.h
	codec/rfx_dwt.c
	codec/rfx_dwt.h
	codec/rfx_encode.c
	codec/rfx_encode.h
	codec/rfx_quantization.c
	codec/rfx_quantization.h
	codec/rfx_rlgr.c
	codec/rfx_rlgr.h
	codec/rfx_types.h
	codec/rfx.c
	codec/region.c
	codec/nsc.c
	codec/nsc_encode.c
	codec/nsc_encode.h
	codec/nsc_types.h
	codec/ncrush.c
	codec/xcrush.c
	codec/mppc.c
	codec/zgfx.c
	codec/clear.c
	codec/jpeg.c
	codec/h264.c)

set(CODEC_SSE2_SRCS
	codec/rfx_sse2.c
	codec/rfx_sse2.h
	codec/nsc_sse2.c
	codec/nsc_sse2.h)

set(CODEC_NEON_SRCS
	codec/rfx_neon.c
	codec/rfx_neon.h)

if(WITH_SSE2)
	set(CODEC_SRCS ${CODEC_SRCS} ${CODEC_SSE2_SRCS})

	if(CMAKE_COMPILER_IS_GNUCC)
		set_source_files_properties(${CODEC_SSE2_SRCS} PROPERTIES COMPILE_FLAGS "-msse2" )
	endif()

	if(MSVC)
		set_source_files_properties(${CODEC_SSE2_SRCS} PROPERTIES COMPILE_FLAGS "/arch:SSE2" )
	endif()
endif()

if(WITH_NEON)
	set_source_files_properties(${CODEC_NEON_SRCS} PROPERTIES COMPILE_FLAGS "-mfpu=neon -Wno-unused-variable" )
	set(CODEC_SRCS ${CODEC_SRCS} ${CODEC_NEON_SRCS})
endif()

if(WITH_JPEG)
	freerdp_include_directory_add(${JPEG_INCLUDE_DIR})
	freerdp_library_add(${JPEG_LIBRARIES})
endif()

if(WITH_X264)
	freerdp_definition_add(-DWITH_X264)
	freerdp_include_directory_add(${X264_INCLUDE_DIR})
	freerdp_library_add(${X264_LIBRARIES})
endif()

if(WITH_OPENH264)
	freerdp_definition_add(-DWITH_OPENH264)
	freerdp_include_directory_add(${OPENH264_INCLUDE_DIR})
	freerdp_library_add(${OPENH264_LIBRARIES})
endif()

if(WITH_LIBAVCODEC)
	freerdp_definition_add(-DWITH_LIBAVCODEC)
	find_library(LIBAVCODEC_LIB avcodec)
	find_library(LIBAVUTIL_LIB avutil)
	freerdp_library_add(${LIBAVCODEC_LIB} ${LIBAVUTIL_LIB})
endif()

freerdp_module_add(${CODEC_SRCS})

if(BUILD_TESTING)
	add_subdirectory(codec/test)
endif()

# /codec

# primitives

set(PRIMITIVES_SRCS
	primitives/prim_16to32bpp.c
	primitives/prim_add.c
	primitives/prim_andor.c
	primitives/prim_alphaComp.c
	primitives/prim_colors.c
	primitives/prim_copy.c
	primitives/prim_set.c
	primitives/prim_shift.c
	primitives/prim_sign.c
	primitives/prim_YUV.c
	primitives/prim_YCoCg.c
	primitives/primitives.c
	primitives/prim_internal.h)

set(PRIMITIVES_OPT_SRCS
	primitives/prim_16to32bpp_opt.c
	primitives/prim_add_opt.c
	primitives/prim_andor_opt.c
	primitives/prim_alphaComp_opt.c
	primitives/prim_colors_opt.c
	primitives/prim_set_opt.c
	primitives/prim_shift_opt.c
	primitives/prim_sign_opt.c
	primitives/prim_YUV_opt.c
	primitives/prim_YCoCg_opt.c)

freerdp_definition_add(-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

### IPP Variable debugging
if(WITH_IPP)
	if(CMAKE_COMPILER_IS_GNUCC)
		foreach(INCLDIR ${IPP_INCLUDE_DIRS})
			set(OPTIMIZATION "${OPTIMIZATION} -I${INCLDIR}")
		endforeach(INCLDIR)
	endif()
endif()

if(WITH_SSE2)
	if(CMAKE_COMPILER_IS_GNUCC)
		set(OPTIMIZATION "${OPTIMIZATION} -msse2 -mssse3 -Wdeclaration-after-statement")
	endif()

	if(MSVC)
		set(OPTIMIZATION "${OPTIMIZATION} /arch:SSE2")
	endif()
elseif(WITH_NEON)
	if(CMAKE_COMPILER_IS_GNUCC)
		set(OPTIMIZATION "${OPTIMIZATION} -mfpu=neon")
	endif()
	# TODO: Add MSVC equivalent
endif()

if(DEFINED OPTIMIZATION)
	set_source_files_properties(${PRIMITIVES_OPT_SRCS} PROPERTIES COMPILE_FLAGS ${OPTIMIZATION})
endif()

set(PRIMITIVES_SRCS ${PRIMITIVES_SRCS} ${PRIMITIVES_OPT_SRCS})

freerdp_module_add(${PRIMITIVES_SRCS})

if(IPP_FOUND)
	freerdp_include_directory_add(${IPP_INCLUDE_DIRS})
		foreach(ipp_lib ${IPP_LIBRARIES})
		freerdp_library_add("${ipp_lib}_imported")
	endforeach()
endif()

if(BUILD_TESTING AND NOT WIN32 AND NOT APPLE)
	add_subdirectory(primitives/test)
endif()


# /primitives

list(REMOVE_DUPLICATES LIBFREERDP_DEFINITIONS)
list(REMOVE_DUPLICATES LIBFREERDP_LIBS)
list(REMOVE_DUPLICATES LIBFREERDP_INCLUDES)
include_directories(${LIBFREERDP_INCLUDES})

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set (LIBFREERDP_SRCS ${LIBFREERDP_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${LIBFREERDP_SRCS})
add_definitions(${LIBFREERDP_DEFINITIONS})

set_target_properties(${MODULE_NAME} PROPERTIES LINKER_LANGUAGE C)

if (WITH_LIBRARY_VERSIONING)
    set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
endif()


target_link_libraries(${MODULE_NAME} ${PRIVATE_KEYWORD} ${LIBFREERDP_LIBS} winpr)
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries EXPORT FreeRDPTargets)
if (WITH_DEBUG_SYMBOLS AND MSVC AND BUILD_SHARED_LIBS)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/libfreerdp")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/freerdp.pc.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp${FREERDP_VERSION_MAJOR}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp${FREERDP_VERSION_MAJOR}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

## cmake project
if(${CMAKE_VERSION} VERSION_GREATER "2.8.10")

	export(PACKAGE freerdp)

	set(FREERDP_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/FreeRDP${FREERDP_VERSION_MAJOR}")

	configure_package_config_file(FreeRDPConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/FreeRDPConfig.cmake
		INSTALL_DESTINATION ${FREERDP_CMAKE_INSTALL_DIR}
		PATH_VARS FREERDP_INCLUDE_DIR)

	write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/FreeRDPConfigVersion.cmake
		VERSION ${FREERDP_VERSION} COMPATIBILITY SameMajorVersion)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FreeRDPConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/FreeRDPConfigVersion.cmake
		DESTINATION ${FREERDP_CMAKE_INSTALL_DIR})

	install(EXPORT FreeRDPTargets DESTINATION ${FREERDP_CMAKE_INSTALL_DIR})

endif()
