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
		LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/compat_${CMVERSION}/")
	endif()
endmacro()

# Compatibility includes - order does matter!
enable_cmake_compat(3.7.0)
enable_cmake_compat(2.8.11)
enable_cmake_compat(2.8.6)
enable_cmake_compat(2.8.3)
enable_cmake_compat(2.8.2)

# GetGitRevisionDescription requires FindGit which was added in version 2.8.2
# build won't fail but GIT_REVISION is set to n/a
if(${CMAKE_VERSION} VERSION_LESS 2.8.2)
	message(WARNING "GetGitRevisionDescription reqires (FindGit) cmake >= 2.8.2 to work properly - GIT_REVISION will be set to n/a")
endif()

# Since cmake 2.8.9 modules/library names without lib/.so can be used
# for dependencies
if(IOS AND ${CMAKE_VERSION} VERSION_LESS 2.8.9)
	message(FATAL_ERROR "CMAKE version >= 2.8.9 required to build the IOS client")
endif()
