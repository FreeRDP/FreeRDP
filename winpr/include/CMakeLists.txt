# WinPR: Windows Portable Runtime
# winpr cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

configure_file(config/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/version.h)
configure_file(config/wtypes.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/wtypes.h)
configure_file(config/build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/build-config.h)
configure_file(config/buildflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/buildflags.h)
configure_file(config/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/config.h)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
	DESTINATION ${WINPR_INCLUDE_DIR}
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
	DESTINATION ${WINPR_INCLUDE_DIR}
        FILES_MATCHING PATTERN "*.h")

