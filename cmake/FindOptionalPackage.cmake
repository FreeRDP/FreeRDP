# - FindOptionalPackage
# Enable or disable optional packages. Also force optional packages.
#
#  This module defines the following variables:
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


macro(find_optional_package _normal_package)
	STRING(TOUPPER ${_normal_package} _upper_package)
	OPTION(WITH_${_upper_package} "Force dependencies to ${_normal_package}" OFF)
	OPTION(WITHOUT_${_upper_package} "Never depend on ${_normal_package}" OFF)

	if(WITH_${_upper_package})
		find_package(${_normal_package} REQUIRED)
	elseif(NOT WITHOUT_${_upper_package})
		find_package(${_normal_package})
	endif(WITH_${_upper_package})
endmacro(find_optional_package)
