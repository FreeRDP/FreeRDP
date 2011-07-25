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

set(LIBFREERDP_CORE_SRCS
	activation.c
	activation.h
	bitmap.c
	bitmap.h
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
	crypto.c
	crypto.h
	credssp.c
	credssp.h
	ntlmssp.c
	ntlmssp.h
	license.c
	license.h
	registry.c
	registry.h
	security.c
	security.h
	settings.c
	orders.c
	orders.h
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
	transport.c
	transport.h
	update.c
	update.h
)

add_library(freerdp-core SHARED ${LIBFREERDP_CORE_SRCS})

target_link_libraries(freerdp-core z)
target_link_libraries(freerdp-core ssl)
target_link_libraries(freerdp-core crypto)
target_link_libraries(freerdp-core freerdp-utils)

install(TARGETS freerdp-core DESTINATION lib)
