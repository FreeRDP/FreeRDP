# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-nsc cmake build script
#
# CopyRight SamSung
# Author Jiten Pathy
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

set(LIBFREERDP_NSC_SRCS
	libnsc.c
)

add_library(freerdp-nsc ${LIBFREERDP_NSC_SRCS})

set_target_properties(freerdp-nsc PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

target_link_libraries(freerdp-nsc freerdp-utils)

install(TARGETS freerdp-nsc DESTINATION ${CMAKE_INSTALL_LIBDIR})
