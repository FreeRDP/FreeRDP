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

if(MONOLITHIC_BUILD)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Create imported targets for Intel IPP libraries

if(IPP_FOUND)
    foreach(ipp_lib ${IPP_LIBRARIES})
    	add_library("${ipp_lib}_imported" STATIC IMPORTED)
		set_property(TARGET "${ipp_lib}_imported" PROPERTY IMPORTED_LOCATION "${IPP_LIBRARY_DIRS}/${ipp_lib}")
	endforeach()
endif()

set(${MODULE_PREFIX}_SUBMODULES
	utils
	common
	gdi
	rail
	cache
	codec
	crypto
	locale
	primitives
	core)

foreach(${MODULE_PREFIX}_SUBMODULE ${${MODULE_PREFIX}_SUBMODULES})
	add_subdirectory(${${MODULE_PREFIX}_SUBMODULE})
endforeach()

if(MONOLITHIC_BUILD)
	foreach(${MODULE_PREFIX}_SUBMODULE ${${MODULE_PREFIX}_SUBMODULES})
		set(${MODULE_PREFIX}_OBJECTS ${${MODULE_PREFIX}_OBJECTS} "$<TARGET_OBJECTS:${MODULE_NAME}-${${MODULE_PREFIX}_SUBMODULE}>")
	endforeach()

	add_library(${MODULE_NAME} dummy.c ${${MODULE_PREFIX}_OBJECTS})

	set_target_properties(${MODULE_NAME} PROPERTIES LINKER_LANGUAGE C)
	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION} PREFIX "lib")

	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
	
	set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/libfreerdp")
endif()

