# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP SDL Client
#
# Copyright 2024 Armin Novak <anovak@thincast.com>
# Copyright 2024 Thincast Technologies GmbH
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
set(SRCS sdl3_resource_manager.cpp sdl3_resource_manager.hpp)

add_library(sdl3_client_res STATIC ${SRCS})
if(NOT WITH_SDL_LINK_SHARED)
  target_link_libraries(sdl3_client_res ${SDL3_STATIC_LIBRARIES})
else()
  target_link_libraries(sdl3_client_res ${SDL3_LIBRARIES})
endif()

target_link_libraries(sdl3_client_res sdl-common-client-res)

set_property(TARGET sdl3_client_res PROPERTY POSITION_INDEPENDENT_CODE ON)
