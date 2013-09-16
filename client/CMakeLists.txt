# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Clients
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

# Clients

add_subdirectory(common)

if(WIN32)
	add_subdirectory(Windows)	
else()
	if(WITH_SAMPLE)
		add_subdirectory(Sample)
	endif()

	if(WITH_DIRECTFB)
		add_subdirectory(DirectFB)
	endif()
endif()

if(WITH_X11)
	add_subdirectory(X11)
endif()

if(APPLE)
	if(IOS)
		if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/iOS")
			message(STATUS "Adding iOS client")
			add_subdirectory(iOS)
		endif()
	else()
		add_subdirectory(Mac)
	endif()
endif()

if(ANDROID)
	message(STATUS "Adding Android client")
	add_subdirectory(Android)
endif()

# Pick up other clients

set(FILENAME "ModuleOptions.cmake")
file(GLOB FILEPATHS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/${FILENAME}")

foreach(FILEPATH ${FILEPATHS})
	if(${FILEPATH} MATCHES "^([^/]*)//${FILENAME}")
		string(REGEX REPLACE "^([^/]*)//${FILENAME}" "\\1" FREERDP_CLIENT ${FILEPATH})
		set(FREERDP_CLIENT_ENABLED 0)
		include(${FILEPATH})
		if(FREERDP_CLIENT_ENABLED)
			if(NOT (${FREERDP_CLIENT_VENDOR} MATCHES "FreeRDP"))
				list(APPEND FREERDP_EXTRA_CLIENTS ${FREERDP_CLIENT})
				if(${FREERDP_CLIENT_VENDOR} MATCHES "${VENDOR}")
					set(CLIENT_VENDOR_PATH "client/${FREERDP_CLIENT}" PARENT_SCOPE)
				endif()
			endif()
		endif()
	endif()
endforeach()

foreach(FREERDP_CLIENT ${FREERDP_EXTRA_CLIENTS})
	add_subdirectory(${FREERDP_CLIENT})
endforeach()


