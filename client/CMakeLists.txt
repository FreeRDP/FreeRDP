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

	find_optional_package(DirectFB)
	if(WITH_DIRECTFB)
		add_subdirectory(DirectFB)
	endif()
endif()

if(NOT WIN32 AND (NOT ANDROID))
	find_suggested_package(X11)
	if(WITH_X11)
		add_subdirectory(X11)
	endif()
endif()

if(APPLE)
	add_subdirectory(Mac)
endif()

if(ANDROID)
	if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Android")
		message(STATUS "Adding Android client")
		add_subdirectory(Android)
	endif()
endif()

