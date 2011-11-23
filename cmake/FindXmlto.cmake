# - Find xmlto
# Find the xmlto docbook xslt frontend
#
#  This module defines the following variables:
#     XMLTO_FOUND        - true if xmlto was found
#     XMLTO_EXECUTABLE   - Path to xmlto, if xmlto was found
#

#=============================================================================
# Copyright 2011 Nils Andresen <nils@nils-andresen.de>
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
#=============================================================================
set(XMLTO_FOUND false)

find_program(XMLTO_EXECUTABLE
	NAMES xmlto
	DOC   "docbook xslt frontend")

if(XMLTO_EXECUTABLE)
	set(XMLTO_FOUND true)
	message(STATUS "Found XMLTO: ${XMLTO_EXECUTABLE}")
endif()

mark_as_advanced(XMLTO_EXECUTABLE)
