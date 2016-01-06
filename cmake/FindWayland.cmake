# - Finds UWAC
# Find the UWAC libraries and its dependencies
#
#  This module defines the following variables:
#     UWAC_FOUND        - true if UWAC has been found
#     UWAC_LIBS         - Set to the full path to UWAC libraries and its dependencies
#     UWAC_INCLUDE_DIRS - Set to the include directories for UWAC
#

#=============================================================================
# Copyright 2015 David Fort <contact@hardening-consulting.com>
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

include(FindPkgConfig)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(UWAC uwac)
  
    # find the complete path to each dependant library
    set(UWAC_LIBS, "")
    foreach(libname ${UWAC_LIBRARIES})
        find_library(FOUND_LIB NAMES ${libname}
                  PATHS ${UWAC_LIBRARY_DIRS}
        )
       
        if (FOUND_LIB)
            list(APPEND UWAC_LIBS ${FOUND_LIB})
        endif()
        unset(FOUND_LIB CACHE)
    endforeach()
    
    
    find_path(UWAC_INCLUDE_DIR NAMES uwac/uwac.h
              PATHS ${UWAC_INCLUDE_DIRS}
              DOC "The UWAC include directory"
    )

endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(UWAC DEFAULT_MSG UWAC_LIBS UWAC_INCLUDE_DIR)
