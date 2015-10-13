# WinPR: Windows Portable Runtime
# libwinpr-sspi cmake build script
#
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

set(MODULE_PREFIX "WINPR_SSPI")

set(${MODULE_PREFIX}_NTLM_SRCS
	NTLM/ntlm_av_pairs.c
	NTLM/ntlm_av_pairs.h
	NTLM/ntlm_compute.c
	NTLM/ntlm_compute.h
	NTLM/ntlm_message.c
	NTLM/ntlm_message.h
	NTLM/ntlm.c
	NTLM/ntlm.h)

set(${MODULE_PREFIX}_NEGOTIATE_SRCS
	Negotiate/negotiate.c
	Negotiate/negotiate.h)

set(${MODULE_PREFIX}_SCHANNEL_SRCS
	Schannel/schannel_openssl.c
	Schannel/schannel_openssl.h
	Schannel/schannel.c
	Schannel/schannel.h)

set(${MODULE_PREFIX}_CREDSSP_SRCS
	CredSSP/credssp.c)

set(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_CREDSSP_SRCS}
	sspi_winpr.c
	sspi_winpr.h
	sspi_export.c
	sspi.c
	sspi.h)

winpr_module_add(${${MODULE_PREFIX}_NTLM_SRCS}
	${${MODULE_PREFIX}_KERBEROS_SRCS}
	${${MODULE_PREFIX}_NEGOTIATE_SRCS}
	${${MODULE_PREFIX}_SCHANNEL_SRCS}
	${${MODULE_PREFIX}_SRCS})

if(OPENSSL_FOUND)
	winpr_include_directory_add(${OPENSSL_INCLUDE_DIR})
	winpr_library_add(${OPENSSL_LIBRARIES})
endif()

if(MBEDTLS_FOUND)
	winpr_include_directory_add(${MBEDTLS_INCLUDE_DIR})
	winpr_library_add(${MBEDTLS_LIBRARIES})
endif()

if(WIN32)
	winpr_library_add(ws2_32)
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
