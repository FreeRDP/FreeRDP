

include(EchoTarget)
include(CMakeParseArguments)

# - add a new library to a module for export
#  MODULE - module the library belongs to
#  LIBNAME - name of the library
#   - if MODULE isn't set the NAME should must be in the form MODULE-NAME
function(export_complex_library)
	set(PREFIX "EXPORT_COMPLEX_LIBRARY")
	cmake_parse_arguments(${PREFIX}
			""
			"LIBNAME;MODULE"
			""
			${ARGN})

	if (NOT ${PREFIX}_LIBNAME)
		message(FATAL_ERROR "export_complex_library requires a name to be set")
	endif()
	if (NOT ${PREFIX}_MODULE)
		# get the module prefix and remove it from libname
		string(REPLACE "-" ";" LIBNAME_LIST "${${PREFIX}_LIBNAME}")
		list(GET LIBNAME_LIST 0 MODULE)
		list(REMOVE_AT LIBNAME_LIST 0)
		string(REPLACE ";" "-" LIBNAME "${LIBNAME_LIST}")
	else()
		set(MODULE ${${PREFIX}_MODULE})
		set(LIBNAME ${${PREFIX}_LIBNAME})
	endif()
	if (NOT MODULE)
		message(FATAL_ERROR "export_complex_library couldn't identify MODULE")
	endif()
	get_property(MEXPORTS GLOBAL PROPERTY ${MODULE}_EXPORTS)
	list(APPEND MEXPORTS ${LIBNAME})
	set_property(GLOBAL PROPERTY ${MODULE}_EXPORTS "${MEXPORTS}")
endfunction(export_complex_library)

macro(add_complex_library)

	set(PREFIX "COMPLEX_LIBRARY")
	
	cmake_parse_arguments(${PREFIX}
		"EXPORT"
		"MODULE;TYPE;MONOLITHIC"
		"SOURCES"
		${ARGN})

	string(TOUPPER "${${PREFIX}_MODULE}_TYPE" ${PREFIX}_TYPE_OPTION)
	string(REGEX REPLACE "-" "_" ${PREFIX}_TYPE_OPTION ${${PREFIX}_TYPE_OPTION})

	if(${${PREFIX}_MONOLITHIC})
		add_library(${${PREFIX}_MODULE} ${${PREFIX}_TYPE} ${${PREFIX}_SOURCES})
	else()
		if (NOT DEFINED ${${PREFIX}_TYPE_OPTION})
			add_library(${${PREFIX}_MODULE} ${${PREFIX}_SOURCES})
		else()
			add_library(${${PREFIX}_MODULE} ${${${PREFIX}_TYPE_OPTION}} ${${PREFIX}_SOURCES})
		endif()
	endif()
	if (${PREFIX}_EXPORT)
		export_complex_library(LIBNAME ${${PREFIX}_MODULE})
	endif()

endmacro(add_complex_library)

if(${CMAKE_VERSION} VERSION_GREATER 2.8.8)
	set(CMAKE_OBJECT_TARGET_SUPPORT 1)
endif()

function(create_object_cotarget target)

	set(cotarget "${target}-objects")

	get_target_property(${target}_TYPE ${target} TYPE)

	if(NOT ((${target}_TYPE MATCHES "SHARED_LIBRARY") OR (${target}_TYPE MATCHES "SHARED_LIBRARY")))
		return()
	endif()

	get_target_property(${target}_SOURCES ${target} SOURCES)
	get_target_property(${target}_LINK_LIBRARIES ${target} LINK_LIBRARIES)
	get_target_property(${target}_INCLUDE_DIRECTORIES ${target} INCLUDE_DIRECTORIES)

	add_library(${cotarget} "OBJECT" ${${target}_SOURCES})

	set_target_properties(${cotarget} PROPERTIES LINK_LIBRARIES "${${target}_LINK_LIBRARIES}")
	set_target_properties(${cotarget} PROPERTIES INCLUDE_DIRECTORIES "${${target}_INCLUDE_DIRECTORIES}")

	echo_target(${target})
	echo_target(${cotarget})

endfunction()

