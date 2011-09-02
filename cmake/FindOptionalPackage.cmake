# - FindOptionalPackage
# Enable or disable optional packages. Also force optional packages.
#
#  This module defines the following macros:
#    find_required_package   : find a required package, can not be disabled
#    find_suggested_package  : find a suggested package - required but can be disabled
#    find_optional_package   : find an optional package - required only if enabled
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

macro(find_required_package _normal_package)
	find_package(${_normal_package} REQUIRED)
endmacro(find_required_package)

macro(find_suggested_package _normal_package)
	string(TOUPPER ${_normal_package} _upper_package)
	option(WITH_${_upper_package} "Add dependency to ${_normal_package} - recommended" ON)
	
	if(WITH_${_upper_package})
		message(STATUS "Finding suggested package ${_normal_package}.")
		message(STATUS "  Disable this using \"-DWITH_${_upper_package}=OFF\".")
		find_package(${_normal_package} REQUIRED)
	endif(WITH_${_upper_package})
endmacro(find_suggested_package)

macro(find_optional_package _normal_package)
	string(TOUPPER ${_normal_package} _upper_package)
	option(WITH_${_upper_package} "Add dependency to ${_normal_package}" OFF)

	if(WITH_${_upper_package})
		find_package(${_normal_package} REQUIRED)
	else(WITH_${_upper_package})
		message(STATUS "Skipping optional package ${_normal_package}.")
		message(STATUS "  Enable this using \"-DWITH_${_upper_package}=ON\".")
	endif(WITH_${_upper_package})
endmacro(find_optional_package)
