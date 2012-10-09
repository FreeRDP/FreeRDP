# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp-core cmake build script
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

set(MODULE_NAME "freerdp-core")
set(MODULE_PREFIX "FREERDP_CORE")

add_definitions(-DEXT_PATH="${FREERDP_EXTENSION_PATH}")

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

set(${MODULE_PREFIX}_SRCS
	activation.c
	activation.h
	extension.c
	extension.h
	gcc.c
	gcc.h
	mcs.c
	mcs.h
	nego.c
	nego.h
	info.c
	info.h
	http.c
	http.h
	rpc.c
	rpc.h
	rts.c
	rts.h
	input.c
	input.h
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
	timezone.c
	timezone.h
	rdp.c
	rdp.h
	tcp.c
	tcp.h
	tsg.c
	tsg.h
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
	peer.h)

if(MONOLITHIC_BUILD)
	add_library(${MODULE_NAME} OBJECT ${${MODULE_PREFIX}_SRCS})
else()
	add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
endif()

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

if(MONOLITHIC_BUILD)
	set(${MODULE_PREFIX}_LIBS winpr)
else()
	set(${MODULE_PREFIX}_LIBS
		winpr-registry
		winpr-utils
		winpr-sspi
		winpr-crt)
endif()

if(WIN32)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ws2_32)
else()
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${ZLIB_LIBRARIES})
endif()

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${OPENSSL_LIBRARIES})

if(MONOLITHIC_BUILD)
	set(FREERDP_LIBS ${FREERDP_LIBS} ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)
else()
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS}
		freerdp-utils
		freerdp-codec
		freerdp-crypto
		freerdp-locale)
		
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/libfreerdp")
