# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-crypto cmake build script
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

set(FREERDP_CRYPTO_SRCS
	er.c
	der.c
	ber.c
	per.c
	nla.c
	certificate.c
	crypto.c
	tls.c)

if(WITH_MONOLITHIC_BUILD)
	add_library(freerdp-crypto OBJECT ${FREERDP_CRYPTO_SRCS})
else()
	add_library(freerdp-crypto ${FREERDP_CRYPTO_SRCS})
endif()

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

set_target_properties(freerdp-crypto PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set(FREERDP_CRYPTO_LIBS
	${OPENSSL_LIBRARIES}
	winpr-sspi)

if(WIN32)
	set(FREERDP_CRYPTO_LIBS ${FREERDP_CRYPTO_LIBS} ws2_32)
else()
	set(FREERDP_CRYPTO_LIBS ${FREERDP_CRYPTO_LIBS} winpr-library)
	set(FREERDP_CRYPTO_LIBS ${FREERDP_CRYPTO_LIBS} ${ZLIB_LIBRARIES})
endif()

if(WITH_MONOLITHIC_BUILD)
	
else()
	set(FREERDP_CRYPTO_LIBS ${FREERDP_CRYPTO_LIBS} freerdp-utils)
	target_link_libraries(freerdp-crypto ${FREERDP_CRYPTO_LIBS})
	install(TARGETS freerdp-crypto DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
