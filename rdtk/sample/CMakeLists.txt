# RdTk: Remote Desktop Toolkit
# rdtk cmake build script
#
# Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(MODULE_NAME "rdtk-sample")
set(MODULE_PREFIX "RDTK_SAMPLE")

find_package(X11 REQUIRED)

include_directories(SYSTEM ${X11_INCLUDE_DIR})

set(SRCS rdtk_x11.c)

addtargetwithresourcefile(${MODULE_NAME} TRUE "${RDTK_VERSION}" SRCS)

set(LIBS rdtk)

list(APPEND LIBS ${X11_LIBRARIES})

target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS} winpr)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "RdTk")
