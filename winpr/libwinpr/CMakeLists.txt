# WinPR: Windows Portable Runtime
# winpr cmake build script
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

set(MODULE_NAME "winpr")
set(MODULE_PREFIX "WINPR")

if (APPLE)
	# flat_namespace should be avoided, but is required for -undefined warning. Since WinPR currently has
	# a lot of undefined symbols in use, use this hack until they're filled out.
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-flat_namespace,-undefined,warning")	
endif()

if(MONOLITHIC_BUILD)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(FILENAME "ModuleOptions.cmake")
file(GLOB FILEPATHS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/${FILENAME}")

foreach(FILEPATH ${FILEPATHS})
	if(${FILEPATH} MATCHES "^([^/]*)//${FILENAME}")
		string(REGEX REPLACE "^([^/]*)//${FILENAME}" "\\1" ${MODULE_PREFIX}_SUBMODULE ${FILEPATH})
		set(${MODULE_PREFIX}_SUBMODULES ${${MODULE_PREFIX}_SUBMODULES} ${${MODULE_PREFIX}_SUBMODULE})
	endif()
endforeach(FILEPATH)

foreach(${MODULE_PREFIX}_SUBMODULE ${${MODULE_PREFIX}_SUBMODULES})
	add_subdirectory(${${MODULE_PREFIX}_SUBMODULE})
endforeach()

if(MONOLITHIC_BUILD)

	foreach(${MODULE_PREFIX}_SUBMODULE ${${MODULE_PREFIX}_SUBMODULES})
		set(${MODULE_PREFIX}_OBJECTS ${${MODULE_PREFIX}_OBJECTS} "$<TARGET_OBJECTS:${MODULE_NAME}-${${MODULE_PREFIX}_SUBMODULE}>")
	endforeach()

	add_library(${MODULE_NAME} dummy.c ${${MODULE_PREFIX}_OBJECTS})

	set_target_properties(${MODULE_NAME} PROPERTIES LINKER_LANGUAGE C)
	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

	target_link_libraries(${MODULE_NAME} ${WINPR_LIBS})

	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)

	set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/libwinpr")
endif()
