# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
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

include(CMakeDependentOption)

option(CHANNEL_RDP2TCP "build rdp2tcp channel" ON)
CMAKE_DEPENDENT_OPTION(CHANNEL_RDP2TCP_CLIENT "build rdp2tcp client channel" ON
	"CHANNEL_RDP2TCP" OFF)
CMAKE_DEPENDENT_OPTION(CHANNEL_RDP2TCP_SERVER "build rdp2tcp server channel" OFF
	"CHANNEL_RDP2TCP" OFF)
if (CHANNEL_RDP2TCP)
	if (CHANNEL_RDP2TCP_CLIENT)
		add_subdirectory(client)
	endif()
	if (CHANNEL_RDP2TCP_SERVER)
		add_subdirectory(server)
	endif()
endif()
