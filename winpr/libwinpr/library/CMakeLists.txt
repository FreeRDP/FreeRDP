# WinPR: Windows Portable Runtime
# libwinpr-library cmake build script
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

set(MODULE_NAME "winpr-library")
set(MODULE_PREFIX "WINPR_LIBRARY")

set(${MODULE_PREFIX}_SRCS
	library.c)

if(WITH_MONOLITHIC_BUILD)
	add_library(${MODULE_NAME} OBJECT ${${MODULE_PREFIX}_SRCS})
else()
	add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
endif()

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

if(WITH_MONOLITHIC_BUILD)

else()
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR")

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
