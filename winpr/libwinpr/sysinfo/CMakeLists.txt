# WinPR: Windows Portable Runtime
# libwinpr-sysinfo cmake build script
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

set(WINPR_SYSINFO_SRCS
	sysinfo.c)

if(WITH_MONOLITHIC_BUILD)
	add_library(winpr-sysinfo OBJECT ${WINPR_SYSINFO_SRCS})
else()
	add_library(winpr-sysinfo ${WINPR_SYSINFO_SRCS})
endif()

set_target_properties(winpr-sysinfo PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

if(WITH_MONOLITHIC_BUILD)

else()
	install(TARGETS winpr-sysinfo DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
