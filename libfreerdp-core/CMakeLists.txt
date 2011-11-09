# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-core cmake build script
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

add_definitions(-DEXT_PATH="/usr/lib/freerdp/extensions")
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

set(LIBFREERDP_CORE_SRCS
	activation.c
	activation.h
	extension.c
	extension.h
	ber.c
	ber.h
	gcc.c
	gcc.h
	mcs.c
	mcs.h
	nego.c
	nego.h
	info.c
	info.h
	input.c
	input.h
	crypto.c
	crypto.h
	credssp.c
	credssp.h
	ntlmssp.c
	ntlmssp.h
	license.c
	license.h
	errinfo.c
	errinfo.h
	security.c
	security.h
	settings.c
	orders.c
	orders.h
	freerdp.c
	graphics.c
	capabilities.c
	capabilities.h
	certificate.c
	certificate.h
	connection.c
	connection.h
	redirection.c
	redirection.h
	rdp.c
	rdp.h
	per.c
	per.h
	tcp.c
	tcp.h
	tls.c
	tls.h
	tpdu.c
	tpdu.h
	tpkt.c
	tpkt.h
	fastpath.c
	fastpath.h
	surface.c
	surface.h
	transport.c
	transport.h
	update.c
	update.h
	channel.c
	channel.h
	window.c
	window.h
	listener.c
	listener.h
	peer.c
	peer.h
    mppc.c
)

add_library(freerdp-core ${LIBFREERDP_CORE_SRCS})

set_target_properties(freerdp-core PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

if(WIN32)
	target_link_libraries(freerdp-core ws2_32)
else()
	target_link_libraries(freerdp-core ${ZLIB_LIBRARIES})	
endif()

target_link_libraries(freerdp-core ${OPENSSL_LIBRARIES})
target_link_libraries(freerdp-core freerdp-utils)
target_link_libraries(freerdp-core freerdp-codec)

install(TARGETS freerdp-core DESTINATION ${CMAKE_INSTALL_LIBDIR})
