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

set(WINPR_SSPI_NTLM_SRCS
	NTLM/ntlm_av_pairs.c
	NTLM/ntlm_av_pairs.h
	NTLM/ntlm_compute.c
	NTLM/ntlm_compute.h
	NTLM/ntlm_message.c
	NTLM/ntlm_message.h
	NTLM/ntlm.c
	NTLM/ntlm.h)

set(WINPR_SSPI_NEGOTIATE_SRCS
	Negotiate/negotiate.c
	Negotiate/negotiate.h)

set(WINPR_SSPI_SCHANNEL_SRCS
	Schannel/schannel.c
	Schannel/schannel.h)

set(WINPR_SSPI_CREDSSP_SRCS
	CredSSP/credssp.c)

set(WINPR_SSPI_SRCS
	${WINPR_SSPI_CREDSSP_SRCS}
	sspi.c
	sspi.h)

if(NOT WITH_NATIVE_SSPI)
	set(WINPR_SSPI_SRCS
		${WINPR_SSPI_NTLM_SRCS}
		${WINPR_SSPI_KERBEROS_SRCS}
		${WINPR_SSPI_NEGOTIATE_SRCS}
		${WINPR_SSPI_SCHANNEL_SRCS}
		${WINPR_SSPI_SRCS})
endif()

if(WITH_MONOLITHIC_BUILD)
	add_library(winpr-sspi OBJECT ${WINPR_SSPI_SRCS})
else()
	add_library(winpr-sspi ${WINPR_SSPI_SRCS})
endif()

include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

set_target_properties(winpr-sspi PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

set(WINPR_SSPI_LIBS
	${ZLIB_LIBRARIES}
	${OPENSSL_LIBRARIES})

if(WITH_MONOLITHIC_BUILD)
	set(WINPR_SSPI_LIBS ${WINPR_SSPI_LIBS} winpr)
else()
	if (NOT WIN32)
		set(WINPR_SSPI_LIBS ${WINPR_SSPI_LIBS}
			winpr-crt
			winpr-sysinfo
			winpr-registry)
	endif()
	
	set(WINPR_SSPI_LIBS ${WINPR_SSPI_LIBS} winpr-utils)
endif()

if(WITH_MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${WINPR_SSPI_LIBS} PARENT_SCOPE)
else()
	target_link_libraries(winpr-sspi ${WINPR_SSPI_LIBS})
	install(TARGETS winpr-sspi DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
