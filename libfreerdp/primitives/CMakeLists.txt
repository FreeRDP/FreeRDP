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

set(FREERDP_PRIMITIVE_CFILES
    prim_add.c
    prim_andor.c
    prim_alphaComp.c
    prim_colors.c
    prim_copy.c
    prim_set.c
    prim_shift.c
    prim_sign.c
	primitives.c
    )
set(FREERDP_PRIMITIVE_HEADERS
    prim_internal.h
    )
set(FREERDP_PRIMITIVE_SRCS
    ${FREERDP_PRIMITIVE_CFILES}
    ${FREERDP_PRIMITIVE_HEADERS}
    )

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
	    set(OPTIMZATION "${OPTIMIZATION} -mfpu=neon -mfloat-abi=softfp")
	endif()
	# TODO: Add MSVC equivalent
endif()
set_property(SOURCE ${FREERDP_PRIMITIVE_CFILES} PROPERTY COMPILE_FLAGS ${OPTIMIZATION})

if(WITH_MONOLITHIC_BUILD)
	add_library(freerdp-primitives OBJECT ${FREERDP_PRIMITIVE_SRCS})
else()
	add_library(freerdp-primitives ${FREERDP_PRIMITIVE_SRCS})
endif()

set_target_properties(freerdp-primitives PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

#set(FREERDP_PRIMITIVE_LIBS, rt)

if(WITH_MONOLITHIC_BUILD)
	set(FREERDP_LIBS ${FREERDP_LIBS} ${FREERDP_PRIMITIVE_LIBS} PARENT_SCOPE)
else()
	target_link_libraries(freerdp-primitives ${FREERDP_PRIMITIVE_LIBS})
	install(TARGETS freerdp-primitives DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif(BUILD_TESTING)
