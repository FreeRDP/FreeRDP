# WinPR: Windows Portable Runtime
# libwinpr-rpc cmake build script
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

set(WINPR_RPC_SRCS
	rpc.c
	ndr.c
	midl.c)

add_library(winpr-rpc ${WINPR_RPC_SRCS})

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

set_target_properties(winpr-rpc PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

if(WIN32)
	target_link_libraries(winpr-rpc ws2_32)
else()
	target_link_libraries(winpr-rpc ${ZLIB_LIBRARIES})	
endif()

target_link_libraries(winpr-rpc ${OPENSSL_LIBRARIES})

install(TARGETS winpr-rpc DESTINATION ${CMAKE_INSTALL_LIBDIR})
