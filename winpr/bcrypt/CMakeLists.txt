# WinPR: Windows Portable Runtime
# libwinpr-bcrypt cmake build script
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

set(WINPR_BCRYPT_SRCS
	bcrypt.c)

if(WITH_MONOLITHIC_BUILD)
	add_library(winpr-bcrypt OBJECT ${WINPR_BCRYPT_SRCS})
else()
	add_library(winpr-bcrypt ${WINPR_BCRYPT_SRCS})
endif()

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

set_target_properties(winpr-bcrypt PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set(WINPR_BCRYPT_LIBS
	${ZLIB_LIBRARIES})

if(WITH_MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${WINPR_BCRYPT_LIBS} PARENT_SCOPE)
else()
	if(NOT WIN32)
		set(WINPR_BCRYPT_LIBS ${WINPR_BCRYPT_LIBS} winpr-crt)
	endif()
	
	set(WINPR_BCRYPT_LIBS ${WINPR_BCRYPT_LIBS} winpr-utils)
	
	target_link_libraries(winpr-bcrypt ${WINPR_BCRYPT_LIBS})
	install(TARGETS winpr-bcrypt DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
