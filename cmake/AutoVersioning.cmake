# - AutoVersioning
# Gather version from tarball or SCM
#
#  This module defines the following variables:
#     PRODUCT_VERSION    - Version of product
#

#=============================================================================
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
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

if(EXISTS "${CMAKE_SOURCE_DIR}/.version" )
  file(READ ${CMAKE_SOURCE_DIR}/.version PRODUCT_VERSION)

  string(STRIP ${PRODUCT_VERSION} PRODUCT_VERSION)
else()
  execute_process(COMMAND git describe --match "v[0-9]*" --abbrev=4
                  OUTPUT_VARIABLE PRODUCT_VERSION
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)

  if(PRODUCT_VERSION)
	string(REGEX REPLACE "^v(.*)" "\\1" PRODUCT_VERSION ${PRODUCT_VERSION})
  else()
    # GIT is the default version
    set(PRODUCT_VERSION GIT)
  endif()

  # Check if has not commited changes
  execute_process(COMMAND git update-index -q --refresh)
  execute_process(COMMAND git diff-index --name-only HEAD --
                  OUTPUT_VARIABLE CHANGED_SOURCE
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)

  if(CHANGED_SOURCE)
    set(PRODUCT_VERSION ${PRODUCT_VERSION}-dirty)
  endif()
endif()

message(STATUS "${CMAKE_PROJECT_NAME} ${PRODUCT_VERSION}")
