# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
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

define_channel_client_subsystem("rdpsnd" "emscripten" "")

set(${MODULE_PREFIX}_SRCS rdpsnd_emscripten.c)

set(${MODULE_PREFIX}_LIBS winpr freerdp)

include_directories(..)

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")

# The JavaScript side, for the executable's link: a link item, so that it also reaches the
# executable through the static libraries (link options of their private dependencies don't).
set(_js rdpsnd_emscripten.js)
target_link_libraries(
  ${MODULE_NAME}
  INTERFACE
    "--js-library=$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${FREERDP_ADDIN_PATH}>/${_js}"
)
if(NOT BUILD_SHARED_LIBS)
  install(FILES ${_js} DESTINATION ${FREERDP_ADDIN_PATH})
endif()
