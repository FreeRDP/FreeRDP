# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-primitives cmake build script
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

set(MODULE_NAME "freerdp-primitives")
set(MODULE_PREFIX "FREERDP_PRIMITIVES")

set(${MODULE_PREFIX}_SRCS
	prim_add.c
	prim_andor.c
	prim_alphaComp.c
	prim_colors.c
	prim_copy.c
	prim_set.c
	prim_shift.c
	prim_sign.c
	primitives.c
	prim_internal.h)

set(${MODULE_PREFIX}_OPT_SRCS
	prim_add_opt.c
	prim_andor_opt.c
	prim_alphaComp_opt.c
	prim_colors_opt.c
	prim_set_opt.c
	prim_shift_opt.c
	prim_sign_opt.c)

add_definitions(-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

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
	    set(OPTIMIZATION "${OPTIMIZATION} -mfpu=neon -mfloat-abi=${ARM_FP_ABI}")
	endif()
	# TODO: Add MSVC equivalent
endif()

set_property(SOURCE ${${MODULE_PREFIX}_OPT_SRCS} PROPERTY COMPILE_FLAGS ${OPTIMIZATION})

set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_OPT_SRCS})

add_complex_library(MODULE ${MODULE_NAME} TYPE "OBJECT"
	MONOLITHIC ${MONOLITHIC_BUILD}
	SOURCES ${${MODULE_PREFIX}_SRCS}
	EXPORT)

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION} PREFIX "lib")

if(IPP_FOUND)
	include_directories(${IPP_INCLUDE_DIRS})
    foreach(ipp_lib ${IPP_LIBRARIES})
		set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} "${ipp_lib}_imported")
	endforeach()
endif()

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
       MONOLITHIC ${MONOLITHIC_BUILD}
       MODULE winpr
       MODULES winpr-sysinfo)
	   
if(MONOLITHIC_BUILD)
	set(FREERDP_LIBS ${FREERDP_LIBS} ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)
else()
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT FreeRDPTargets)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/libfreerdp")

if(BUILD_TESTING AND ((NOT WIN32) AND (NOT APPLE)))
#	add_subdirectory(test)
endif()

