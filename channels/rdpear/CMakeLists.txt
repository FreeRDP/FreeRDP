# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2023 David Fort <contact@hardening-consulting.com>
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

if(NOT IOS AND NOT WIN32 AND NOT ANDROID)
  find_package(KRB5)

  if(KRB5_FOUND)
    define_channel("rdpear")

    include_directories(common)

    if(WITH_CLIENT_CHANNELS OR WITH_SERVER_CHANNELS)
      add_subdirectory(common)
    endif()

    if(WITH_CLIENT_CHANNELS)
      add_channel_client(${MODULE_PREFIX} ${CHANNEL_NAME})
    endif()

    #if(WITH_SERVER_CHANNELS)
    #	add_channel_server(${MODULE_PREFIX} ${CHANNEL_NAME})
    #endif()
  endif()
endif()
