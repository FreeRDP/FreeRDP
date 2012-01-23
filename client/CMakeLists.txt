# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP Client User Interfaces
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

# User Interfaces

if(NOT WIN32)
	# Build Test Client
	add_subdirectory(test)

	# Build X11 Client
	find_suggested_package(X11)
	if(WITH_X11)
		add_subdirectory(X11)
	endif()

	# Build DirectFB Client
	find_optional_package(DirectFB)
	if(WITH_DIRECTFB)
		add_subdirectory(DirectFB)
	endif()
else()
	# Build Windows Client
	add_subdirectory(Windows)
endif()
