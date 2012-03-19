# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-sspi cmake build script
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

set(FREERDP_SSPI_NTLM_SRCS
	NTLM/ntlm_av_pairs.c
	NTLM/ntlm_av_pairs.h
	NTLM/ntlm_compute.c
	NTLM/ntlm_compute.h
	NTLM/ntlm_message.c
	NTLM/ntlm_message.h
	NTLM/ntlm.c
	NTLM/ntlm.h)

set(FREERDP_SSPI_KERBEROS_SRCS
	Kerberos/kerberos_crypto.c
	Kerberos/kerberos_crypto.h
	Kerberos/kerberos_decode.c
	Kerberos/kerberos_decode.h
	Kerberos/kerberos_encode.c
	Kerberos/kerberos_encode.h
	Kerberos/kerberos.c
	Kerberos/kerberos.h)
	
set(FREERDP_SSPI_NEGOTIATE_SRCS
	Negotiate/negotiate.c
	Negotiate/negotiate.h)

set(FREERDP_SSPI_SRCS
	${FREERDP_SSPI_NTLM_SRCS}
	${FREERDP_SSPI_KERBEROS_SRCS}
	${FREERDP_SSPI_NEGOTIATE_SRCS}
	sspi.c
	sspi.h
	credssp.c)

add_library(freerdp-sspi ${FREERDP_SSPI_SRCS})

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

set_target_properties(freerdp-sspi PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

target_link_libraries(freerdp-sspi freerdp-utils)
target_link_libraries(freerdp-sspi freerdp-crypto)
target_link_libraries(freerdp-sspi ${ZLIB_LIBRARIES})	

install(TARGETS freerdp-sspi DESTINATION ${CMAKE_INSTALL_LIBDIR})

