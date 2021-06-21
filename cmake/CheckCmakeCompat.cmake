# Central location to check for cmake (version) requirements
#
#=============================================================================
# Copyright 2012 Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

macro(enable_cmake_compat CMVERSION)
	if(${CMAKE_VERSION} VERSION_LESS ${CMVERSION})
		LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/compat_${CMVERSION}/")
	endif()
endmacro()

# Compatibility includes - order does matter!
enable_cmake_compat(3.7.0)
