# WinPR: Windows Portable Runtime
# libwinpr-nt cmake build script
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

set(MODULE_NAME "winpr-nt")
set(MODULE_PREFIX "WINPR_NT")

set(${MODULE_PREFIX}_SRCS
	nt.c)

if(MSVC AND (NOT MONOLITHIC_BUILD))
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} module.def)
endif()

add_complex_library(MODULE ${MODULE_NAME} TYPE "OBJECT"
	MONOLITHIC ${MONOLITHIC_BUILD}
	SOURCES ${${MODULE_PREFIX}_SRCS}
	EXPORT)

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

set(${MODULE_PREFIX}_LIBS
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS})

if(${CMAKE_SYSTEM_NAME} MATCHES SunOS)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} rt)
endif()

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD} INTERNAL
	MODULE winpr
	MODULES winpr-crt)

if(MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)
else()
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT WinPRTargets)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR")

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
