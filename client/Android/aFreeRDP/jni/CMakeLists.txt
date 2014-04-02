# FreeRDP: A Remote Desktop Protocol Implementation
# Android Client
#
# Copyright 2013 Armin Novak <armin.novak@thincast.com>
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

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/Application.mk.cmake
	${CMAKE_CURRENT_BINARY_DIR}/Application.mk @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/Android.mk.cmake
	${CMAKE_CURRENT_BINARY_DIR}/Android.mk @ONLY)

