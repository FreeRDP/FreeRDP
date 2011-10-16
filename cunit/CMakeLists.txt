# FreeRDP: A Remote Desktop Protocol Client
# cunit cmake build script
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

include_directories(${CUNIT_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}) # for some internal tests

include_directories(../libfreerdp-core)
include_directories(../libfreerdp-gdi)
include_directories(../libfreerdp-cache)
include_directories(../libfreerdp-codec)

add_executable(test_freerdp
	test_per.c
	test_per.h
	test_ber.c
	test_ber.h
	test_gcc.c
	test_gcc.h
	test_mcs.c
	test_mcs.h
	test_color.c
	test_color.h
	test_bitmap.c
	test_bitmap.h
	test_libgdi.c
	test_libgdi.h
	test_list.c
	test_list.h
	test_orders.c
	test_orders.h
	test_pcap.c
	test_pcap.h
	test_license.c
	test_license.h
	test_stream.c
	test_stream.h
	test_utils.c
	test_utils.h
	test_channels.c
	test_channels.h
	test_cliprdr.c
	test_cliprdr.h
	test_drdynvc.c
	test_drdynvc.h
	test_librfx.c
	test_librfx.h
	test_freerdp.c
	test_freerdp.h
	test_rail.c
	test_rail.h
	test_mppc)

target_link_libraries(test_freerdp ${CUNIT_LIBRARIES})

target_link_libraries(test_freerdp freerdp-core)
target_link_libraries(test_freerdp freerdp-gdi)
target_link_libraries(test_freerdp freerdp-utils)
target_link_libraries(test_freerdp freerdp-channels)
target_link_libraries(test_freerdp freerdp-codec)

add_test(CUnitTests ${EXECUTABLE_OUTPUT_PATH}/test_freerdp)
