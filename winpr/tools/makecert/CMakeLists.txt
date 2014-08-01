# WinPR: Windows Portable Runtime
# winpr-makecert cmake build script
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

set(MODULE_NAME "winpr-makecert-tool")
set(MODULE_PREFIX "WINPR_MAKECERT_TOOL")

set(${MODULE_PREFIX}_SRCS makecert.c)

include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS}) 

set(${MODULE_PREFIX}_LIBS
	${ZLIB_LIBRARIES}
	${OPENSSL_LIBRARIES})

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-crt winpr-utils winpr-sysinfo winpr-file winpr-path)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries EXPORT WinPRTargets)

add_subdirectory(cli)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Tools")
export_complex_library(LIBNAME ${MODULE_NAME})
