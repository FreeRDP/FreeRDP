# Central location to check for cmake (version) requirements
#
#=============================================================================
# Copyright 2012 Bernhard Miklautz <bmiklautz@thinstuff.com>
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

# If WITH_MONOLITHIC_BUILD is used with cmake < 2.8.8 build fails
if (WITH_MONOLITHIC_BUILD)
	if(${CMAKE_VERSION} VERSION_LESS 2.8.8)
		message(FATAL_ERROR "CMAKE version >= 2.8.8 required for WITH_MONOLITHIC_BUILD")
	endif()
endif(WITH_MONOLITHIC_BUILD)
