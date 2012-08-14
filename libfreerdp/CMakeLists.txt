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

add_subdirectory(utils)
add_subdirectory(gdi)
add_subdirectory(rail)
add_subdirectory(cache)
add_subdirectory(codec)
add_subdirectory(crypto)
add_subdirectory(channels)
add_subdirectory(locale)
add_subdirectory(core)

if(WITH_MONOLITHIC_BUILD)
	add_library(freerdp
		$<TARGET_OBJECTS:freerdp-utils>
		$<TARGET_OBJECTS:freerdp-gdi>
		$<TARGET_OBJECTS:freerdp-rail>
		$<TARGET_OBJECTS:freerdp-cache>
		$<TARGET_OBJECTS:freerdp-codec>
		$<TARGET_OBJECTS:freerdp-crypto>
		$<TARGET_OBJECTS:freerdp-channels>
		$<TARGET_OBJECTS:freerdp-locale>
		$<TARGET_OBJECTS:freerdp-core>)
	
	set_target_properties(freerdp PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")
	target_link_libraries(freerdp ${FREERDP_LIBS})
endif()


