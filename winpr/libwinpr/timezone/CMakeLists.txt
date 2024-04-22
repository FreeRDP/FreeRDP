# WinPR: Windows Portable Runtime
# libwinpr-timezone cmake build script
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

option(WITH_XML "Add support to read XML windowsTimeZones.xml" ON)
if (WITH_XML)
    find_package(LibXml2 REQUIRED)

    winpr_definition_add(-DWITH_XML)
    winpr_include_directory_add(${LIBXML2_INCLUDE_DIRS})
    winpr_library_add_private(${LIBXML2_LIBRARIES})
endif()

winpr_module_add(timezone.c TimeZones.c WindowsZones.c TimeZones.h WindowsZones.h)
