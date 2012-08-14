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
	ndr_array.c ndr_array.h
	ndr_context.c ndr_context.h
	ndr_correlation.c ndr_correlation.h
	ndr_pointer.c ndr_pointer.h
	ndr_private.c ndr_private.h
	ndr_simple.c ndr_simple.h
	ndr_string.c ndr_string.h
	ndr_structure.c ndr_structure.h
	ndr_union.c ndr_union.h
	midl.c)

if(WITH_MONOLITHIC_BUILD)
	add_library(winpr-rpc OBJECT ${WINPR_RPC_SRCS})
else()
	add_library(winpr-rpc ${WINPR_RPC_SRCS})
endif()

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

set_target_properties(winpr-rpc PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set(WINPR_RPC_LIBS
	${OPENSSL_LIBRARIES})

if(WIN32)
	set(WINPR_RPC_LIBS ${WINPR_RPC_LIBS} ws2_32)
else()
	set(WINPR_RPC_LIBS ${WINPR_RPC_LIBS} ${ZLIB_LIBRARIES})
endif()

if(WITH_MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${WINPR_RPC_LIBS} PARENT_SCOPE)
else()
	target_link_libraries(winpr-rpc ${WINPR_RPC_LIBS})
	install(TARGETS winpr-rpc DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
