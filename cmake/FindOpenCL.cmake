# - Try to find the OpenCL library
# Once done this will define
#
#  OPENCL_ROOT - A list of search hints
#
#  OPENCL_FOUND - system has OpenCL
#  OPENCL_INCLUDE_DIR - the OpenCL include directory
#  OPENCL_LIBRARIES - opencl library

if (OPENCL_INCLUDE_DIR AND OPENCL_LIBRARY)
	set(OPENCL_FIND_QUIETLY TRUE)
endif()

find_path(OPENCL_INCLUDE_DIR NAMES OpenCL/opencl.h CL/cl.h
	PATH_SUFFIXES include
	HINTS ${OPENCL_ROOT})
find_library(OPENCL_LIBRARY
	 NAMES OpenCL
	 PATH_SUFFIXES lib
	 HINTS ${OPENCL_ROOT})

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenCL DEFAULT_MSG OPENCL_LIBRARY OPENCL_INCLUDE_DIR)

if (OPENCL_INCLUDE_DIR AND OPENCL_LIBRARY)
	set(OPENCL_FOUND TRUE)
	set(OPENCL_LIBRARIES ${OPENCL_LIBRARY})
endif()

if (OPENCL_FOUND)
	if (NOT OPENCL_FIND_QUIETLY)
		message(STATUS "Found OpenCL: ${OPENCL_LIBRARIES}")
	endif()
else()
	if (OPENCL_FIND_REQUIRED)
		message(FATAL_ERROR "OpenCL was not found")
	endif()
endif()

mark_as_advanced(OPENCL_INCLUDE_DIR OPENCL_LIBRARY)

