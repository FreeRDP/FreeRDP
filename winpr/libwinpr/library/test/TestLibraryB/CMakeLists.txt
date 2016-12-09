# WinPR: Windows Portable Runtime
# libwinpr-library cmake build script
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

set(MODULE_NAME "TestLibraryB")
set(MODULE_PREFIX "TEST_LIBRARY_B")

set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} TestLibraryB.c)

add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

set_target_properties(${MODULE_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test/Extra")

