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

include_directories(.)
include_directories(../include)

add_definitions(-DL_ENDIAN=1)
add_definitions(-DEXT_PATH="/usr/lib/freerdp/extensions")

set(LIBFREERDP_CORE_SRCS
	nego.c
	nego.h
#	credssp.c
#	credssp.h
#	ntlmssp.c
#	ntlmssp.h
	tcp.c
	tcp.h
	tpdu.c
	tpdu.h
	tpkt.c
	tpkt.h
	transport.c
	transport.h
)

add_library(freerdp-core SHARED ${LIBFREERDP_CORE_SRCS})

install(TARGETS freerdp-core DESTINATION lib)
