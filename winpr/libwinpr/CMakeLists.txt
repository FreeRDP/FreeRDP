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

if (APPLE)
	# flat_namespace should be avoided, but is required for -undefined warning. Since WinPR currently has
	# a lot of undefined symbols in use, use this hack until they're filled out.
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-flat_namespace,-undefined,warning")	
endif()

if(WITH_MONOLITHIC_BUILD)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(FILENAME "ModuleOptions.cmake")
file(GLOB FILEPATHS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/${FILENAME}")

foreach(FILEPATH ${FILEPATHS})
	if(${FILEPATH} MATCHES "^([^/]*)//${FILENAME}")
		string(REGEX REPLACE "^([^/]*)//${FILENAME}" "\\1" WINPR_MODULE ${FILEPATH})
		set(WINPR_MODULES ${WINPR_MODULES} ${WINPR_MODULE})
	endif()
endforeach(FILEPATH)

foreach(WINPR_MODULE ${WINPR_MODULES})
	add_subdirectory(${WINPR_MODULE})
endforeach()

if(WITH_MONOLITHIC_BUILD)

	foreach(WINPR_MODULE ${WINPR_MODULES})
		set(WINPR_OBJECTS ${WINPR_OBJECTS} "$<TARGET_OBJECTS:winpr-${WINPR_MODULE}>")
	endforeach()

	add_library(winpr ${WINPR_OBJECTS})
	set_target_properties(winpr PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

	target_link_libraries(winpr ${WINPR_LIBS})
	install(TARGETS winpr DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

