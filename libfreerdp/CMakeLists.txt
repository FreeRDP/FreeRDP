# FreeRDP: A Remote Desktop Protocol Client
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

if(WITH_MONOLITHIC_BUILD)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(FREERDP_MODULES
	utils
	gdi
	rail
	cache
	codec
	crypto
	channels
	locale
	core)

foreach(FREERDP_MODULE ${FREERDP_MODULES})
	add_subdirectory(${FREERDP_MODULE})
endforeach()

if(WITH_MONOLITHIC_BUILD)
	
	foreach(FREERDP_MODULE ${FREERDP_MODULES})
		set(FREERDP_OBJECTS ${FREERDP_OBJECTS} "$<TARGET_OBJECTS:freerdp-${FREERDP_MODULE}>")
	endforeach()

	add_library(freerdp ${FREERDP_OBJECTS})
	set_target_properties(freerdp PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

	target_link_libraries(freerdp ${FREERDP_LIBS})
	install(TARGETS freerdp DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()


