###############################################################################
#
# FindNPP.cmake
#
# NPP_LIBRARY_ROOT_DIR   -- Path to the NPP dorectory.
# NPP_INCLUDES           -- NPP Include directories.
# NPP_LIBRARIES          -- NPP libraries.
# NPP_VERSION            -- NPP version in format "major.minor.build".
#
# If not found automatically, please set NPP_LIBRARY_ROOT_DIR 
# in CMake or set enviroment varivabe $NPP_ROOT
#
# Author: Anatoly Baksheev, Itseez Ltd.
# 
# The MIT License
#
# License for the specific language governing rights and limitations under
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
###############################################################################

cmake_policy(PUSH)
cmake_minimum_required(VERSION 2.8.0)
cmake_policy(POP)

if(NOT "${NPP_LIBRARY_ROOT_DIR}" STREQUAL "${NPP_LIBRARY_ROOT_DIR_INTERNAL}")
	unset(NPP_INCLUDES CACHE)
	unset(NPP_LIBRARIES CACHE)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)			
	if (UNIX OR APPLE)
		set(NPP_SUFFIX "32")				
	else()
		set(NPP_SUFFIX "-mt")
	endif()
else(CMAKE_SIZEOF_VOID_P EQUAL 4)
	if (UNIX OR APPLE)
		set(NPP_SUFFIX "64")				
	else()
		set(NPP_SUFFIX "-mt-x64")			
	endif()
endif(CMAKE_SIZEOF_VOID_P EQUAL 4)

find_path(CUDA_ROOT_DIR "doc/CUDA_Toolkit_Release_Notes.txt"
	PATHS "/Developer/NVIDIA"
	PATH_SUFFIXES "CUDA-5.0"
	DOC "CUDA root directory")

find_path(NPP_INCLUDES "npp.h"
	PATHS "${CUDA_ROOT_DIR}"
	PATH_SUFFIXES "include"
	DOC "NPP include directory")
mark_as_advanced(NPP_INCLUDES)

find_library(NPP_LIBRARIES
	NAMES "npp" "libnpp" "npp${NPP_SUFFIX}" "libnpp${NPP_SUFFIX}"
	PATHS "${CUDA_ROOT_DIR}"
	PATH_SUFFIXES "lib"
	DOC "NPP library")
mark_as_advanced(NPP_LIBRARIES)

if(EXISTS ${NPP_INCLUDES}/nppversion.h)
	file(STRINGS ${NPP_INCLUDES}/nppversion.h npp_major REGEX "#define NPP_VERSION_MAJOR.*")
	file(STRINGS ${NPP_INCLUDES}/nppversion.h npp_minor REGEX "#define NPP_VERSION_MINOR.*")
	file(STRINGS ${NPP_INCLUDES}/nppversion.h npp_build REGEX "#define NPP_VERSION_BUILD.*")

	string(REGEX REPLACE "#define NPP_VERSION_MAJOR[ \t]+|//.*" "" npp_major ${npp_major})
	string(REGEX REPLACE "#define NPP_VERSION_MINOR[ \t]+|//.*" "" npp_minor ${npp_minor})
	string(REGEX REPLACE "#define NPP_VERSION_BUILD[ \t]+|//.*" "" npp_build ${npp_build})

	string(REGEX MATCH "[0-9]+" npp_major ${npp_major}) 
	string(REGEX MATCH "[0-9]+" npp_minor ${npp_minor}) 
	string(REGEX MATCH "[0-9]+" npp_build ${npp_build}) 	
	set(NPP_VERSION "${npp_major}.${npp_minor}.${npp_build}")	
endif()

if(NOT EXISTS ${NPP_LIBRARIES} OR NOT EXISTS ${NPP_INCLUDES}/npp.h)
	set(NPP_FOUND FALSE)	
	message(WARNING "NPP headers/libraries are not found. Please specify NPP_LIBRARY_ROOT_DIR in CMake or set $NPP_ROOT_DIR.")	
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(NPP 
    REQUIRED_VARS
        NPP_INCLUDES
        NPP_LIBRARIES
        NPP_VERSION)

if(APPLE)
	# We need to add the path to cudart to the linker using rpath, since the library name for the cuda libraries is prepended with @rpath.
	get_filename_component(_cuda_path_to_npp "${NPP_LIBRARIES}" PATH)
	if(_cuda_path_to_npp)
		list(APPEND NPP_LIBRARIES -Wl,-rpath "-Wl,${_cuda_path_to_npp}")
	endif()
endif()

set(NPP_FOUND TRUE)

set(NPP_LIBRARY_ROOT_DIR_INTERNAL "${NPP_LIBRARY_ROOT_DIR}" CACHE INTERNAL
	"This is the value of the last time NPP_LIBRARY_ROOT_DIR was set successfully." FORCE)

