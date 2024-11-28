# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2024 David Fort <contact@hardening-consulting.com>
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

add_library(rdpear-common INTERFACE)
add_library(
  rdpear-common-obj OBJECT ndr.c rdpear_asn1.c rdpear_common.c rdpear-common/ndr.h rdpear-common/rdpear_asn1.h
                           rdpear-common/rdpear_common.h
)

target_include_directories(rdpear-common SYSTEM INTERFACE ${KRB5_INCLUDE_DIRS})
target_compile_options(rdpear-common INTERFACE ${KRB5_CFLAGS})
target_link_options(rdpear-common INTERFACE ${KRB5_LDFLAGS})

target_include_directories(rdpear-common-obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(rdpear-common-obj SYSTEM PRIVATE ${KRB5_INCLUDE_DIRS})
target_link_directories(rdpear-common INTERFACE ${KRB5_LIBRARY_DIRS})
target_link_libraries(rdpear-common INTERFACE ${KRB5_LIBRARIES} $<TARGET_OBJECTS:rdpear-common-obj>)

channel_install(rdpear-common ${FREERDP_ADDIN_PATH} "FreeRDPTargets")

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  add_subdirectory(test)
endif()
