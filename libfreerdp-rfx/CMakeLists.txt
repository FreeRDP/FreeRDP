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

set(LIBFREERDP_RFX_SRCS
	rfx_bitstream.c
	rfx_bitstream.h
	rfx_constants.h
	rfx_decode.c
	rfx_decode.h
	rfx_differential.c
	rfx_differential.h
	rfx_dwt.c
	rfx_dwt.h
	rfx_encode.c
	rfx_encode.h
	rfx_pool.c
	rfx_pool.h
	rfx_quantization.c
	rfx_quantization.h
	rfx_rlgr.c
	rfx_rlgr.h
	rfx_types.h
	librfx.c
)

add_library(freerdp-rfx ${LIBFREERDP_RFX_SRCS})

set_target_properties(freerdp-rfx PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

target_link_libraries(freerdp-rfx freerdp-utils)

if(WITH_SSE2)
	include_directories(sse2)
	add_subdirectory(sse2)
	target_link_libraries(freerdp-rfx freerdp-rfx-sse2)
endif()

install(TARGETS freerdp-rfx DESTINATION ${CMAKE_INSTALL_LIBDIR})
