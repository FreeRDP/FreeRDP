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

freerdp_definition_add(-DEXT_PATH="${FREERDP_EXTENSION_PATH}")

freerdp_include_directory_add(${OPENSSL_INCLUDE_DIR})
freerdp_include_directory_add(${GSS_INCLUDE_DIR})

set(${MODULE_PREFIX}_GATEWAY_DIR "gateway")

set(${MODULE_PREFIX}_GATEWAY_SRCS
	${${MODULE_PREFIX}_GATEWAY_DIR}/tsg.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/tsg.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/rdg.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/rdg.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/rpc.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/rpc.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/rpc_bind.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/rpc_bind.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/rpc_client.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/rpc_client.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/rpc_fault.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/rpc_fault.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/rts.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/rts.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/rts_signature.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/rts_signature.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/ntlm.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/ntlm.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/http.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/http.h
	${${MODULE_PREFIX}_GATEWAY_DIR}/ncacn_http.c
	${${MODULE_PREFIX}_GATEWAY_DIR}/ncacn_http.h)

set(${MODULE_PREFIX}_SRCS
	bulk.c
	bulk.h
	activation.c
	activation.h
	gcc.c
	gcc.h
	mcs.c
	mcs.h
	nla.c
	nla.h
	nego.c
	nego.h
	info.c
	info.h
	input.c
	input.h
	license.c
	license.h
	errinfo.c
	errbase.c
	errconnect.c
	errinfo.h
	security.c
	security.h
	settings.c
	orders.c
	orders.h
	freerdp.c
	graphics.c
	client.c
	client.h
	server.c
	server.h
	codecs.c
	metrics.c
	capabilities.c
	capabilities.h
	certificate.c
	certificate.h
	connection.c
	connection.h
	redirection.c
	redirection.h
	autodetect.c
	autodetect.h
	heartbeat.c
	heartbeat.h
	multitransport.c
	multitransport.h
	timezone.c
	timezone.h
	rdp.c
	rdp.h
	tcp.c
	tcp.h
	proxy.c
	proxy.h
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
	message.c
	message.h
	channels.c
	channels.h
	window.c
	window.h
	listener.c
	listener.h
	peer.c
	peer.h)

set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_GATEWAY_SRCS})

freerdp_module_add(${${MODULE_PREFIX}_SRCS})

if(WIN32)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ws2_32)
	freerdp_library_add(ws2_32)
endif()

freerdp_library_add(${OPENSSL_LIBRARIES})

if(WITH_GSSAPI)
	freerdp_library_add(${GSS_LIBRARIES})
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
