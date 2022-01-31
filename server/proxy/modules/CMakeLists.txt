# Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

# The third-party directory is meant for third-party components to be built
# as part of the main FreeRDP build system, making separate maintenance easier.
# Subdirectories of the third-party directory are ignored by git, but are
# automatically included by CMake when the -DWITH_THIRD_PARTY=on option is used.

# include proxy header files for proxy modules
include_directories("${PROJECT_SOURCE_DIR}/server/proxy")
include_directories("${PROJECT_SOURCE_DIR}/server/proxy/modules")

# taken from FreeRDP/third-party/CMakeLists.txt
file(GLOB all_valid_subdirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/CMakeLists.txt")

foreach(dir ${all_valid_subdirs})
	if(${dir} MATCHES "^([^/]*)/+CMakeLists.txt")
		string(REGEX REPLACE "^([^/]*)/+CMakeLists.txt" "\\1" dir_trimmed ${dir})
		message(STATUS "Adding proxy module ${dir_trimmed}")
		add_subdirectory(${dir_trimmed})
	endif()
endforeach(dir)
