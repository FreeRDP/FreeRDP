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

add_subdirectory(crt)
add_subdirectory(utils)
add_subdirectory(heap)
add_subdirectory(path)
add_subdirectory(io)
add_subdirectory(file)
add_subdirectory(error)
add_subdirectory(environment)
add_subdirectory(interlocked)
add_subdirectory(handle)
add_subdirectory(synch)
add_subdirectory(thread)
add_subdirectory(sysinfo)
add_subdirectory(bcrypt)
add_subdirectory(dsparse)
add_subdirectory(asn1)
add_subdirectory(rpc)
add_subdirectory(sspicli)
add_subdirectory(sspi)
add_subdirectory(registry)
add_subdirectory(library)
add_subdirectory(timezone)
add_subdirectory(winsock)
add_subdirectory(winhttp)

if(WITH_MONOLITHIC_BUILD)
	add_library(winpr
		$<TARGET_OBJECTS:winpr-crt>
		$<TARGET_OBJECTS:winpr-utils>
		$<TARGET_OBJECTS:winpr-heap>
		$<TARGET_OBJECTS:winpr-path>
		$<TARGET_OBJECTS:winpr-io>
		$<TARGET_OBJECTS:winpr-file>
		$<TARGET_OBJECTS:winpr-error>
		$<TARGET_OBJECTS:winpr-environment>
		$<TARGET_OBJECTS:winpr-interlocked>
		$<TARGET_OBJECTS:winpr-handle>
		$<TARGET_OBJECTS:winpr-synch>
		$<TARGET_OBJECTS:winpr-thread>
		$<TARGET_OBJECTS:winpr-sysinfo>
		$<TARGET_OBJECTS:winpr-bcrypt>
		$<TARGET_OBJECTS:winpr-dsparse>
		$<TARGET_OBJECTS:winpr-asn1>
		$<TARGET_OBJECTS:winpr-rpc>
		$<TARGET_OBJECTS:winpr-sspicli>
		$<TARGET_OBJECTS:winpr-sspi>
		$<TARGET_OBJECTS:winpr-registry>
		$<TARGET_OBJECTS:winpr-library>
		$<TARGET_OBJECTS:winpr-timezone>
		$<TARGET_OBJECTS:winpr-winsock>
		$<TARGET_OBJECTS:winpr-winhttp>)
	
	set_target_properties(winpr PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

	target_link_libraries(winpr ${WINPR_LIBS})
	install(TARGETS winpr DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

