# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP Test UI cmake build script
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

set(FREERDP_CLIENT_TEST_SRCS
	freerdp.c)

add_executable(freerdp-test ${FREERDP_CLIENT_TEST_SRCS})

set(FREERDP_CLIENT_TEST_LIBS ${FREERDP_CLIENT_TEST_LIBS} ${CMAKE_DL_LIBS})

if(WITH_MONOLITHIC_BUILD)
	set(FREERDP_CLIENT_TEST_LIBS ${FREERDP_CLIENT_TEST_LIBS} freerdp)
else()
	set(FREERDP_CLIENT_TEST_LIBS ${FREERDP_CLIENT_TEST_LIBS}
		freerdp-core
		freerdp-gdi
		freerdp-utils
		freerdp-channels)
endif()

target_link_libraries(freerdp-test ${FREERDP_CLIENT_TEST_LIBS})
