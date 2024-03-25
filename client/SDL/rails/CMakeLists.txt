# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP SDL Client
#
# Copyright 2023 Armin Novak <anovak@thincast.com>
# Copyright 2023 Thincast Technologies GmbH
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


set(SRCS
	sdl_rail.hpp
	sdl_rail.cpp
	sdl_rail_icon.hpp
	sdl_rail_icon.cpp
	sdl_rail_icon_cache.hpp
	sdl_rail_icon_cache.cpp
	sdl_rail_window.cpp
	sdl_rail_window.hpp
)

add_library(rails STATIC
    ${SRCS}
)
