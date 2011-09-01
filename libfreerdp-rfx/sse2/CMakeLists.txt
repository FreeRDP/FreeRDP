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

set(LIBFREERDP_RFX_SSE2_SRCS
	rfx_sse2.c
	rfx_sse2.h
)

include_directories(..)

add_library(freerdp-rfx-sse2 ${LIBFREERDP_RFX_SSE2_SRCS})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set_target_properties(freerdp-rfx-sse2 PROPERTIES COMPILE_FLAGS "-fPIC")
endif()
