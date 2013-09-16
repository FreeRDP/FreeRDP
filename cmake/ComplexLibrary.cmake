

include(EchoTarget)
include(CMakeParseArguments)

macro(set_complex_link_libraries)

	set(PREFIX "COMPLEX_LIBRARY")
	
	cmake_parse_arguments(${PREFIX}
		"INTERNAL"
		"MODULE;VARIABLE;MONOLITHIC"
		"MODULES"
		${ARGN})
		
	if(NOT DEFINED ${PREFIX}_MONOLITHIC)
		set(${PREFIX}_MONOLITHIC FALSE)
	endif()
	
	if(${${PREFIX}_MONOLITHIC})
		if(${${PREFIX}_INTERNAL})
			set(${PREFIX}_LIBS)
		else()
			set(${PREFIX}_LIBS ${${PREFIX}_MODULE})
		endif()
	else()
		set(${PREFIX}_LIBS ${${PREFIX}_MODULES})
	endif()

	set(${${PREFIX}_VARIABLE} ${${${PREFIX}_VARIABLE}} ${${PREFIX}_LIBS})
	
endmacro(set_complex_link_libraries)

macro(add_complex_library)

	set(PREFIX "COMPLEX_LIBRARY")
	
	cmake_parse_arguments(${PREFIX}
		""
		"MODULE;TYPE;MONOLITHIC"
		"SOURCES"
		${ARGN})
		
	if(${${PREFIX}_MONOLITHIC})
		add_library(${${PREFIX}_MODULE} ${${PREFIX}_TYPE} ${${PREFIX}_SOURCES})
	else()
		add_library(${${PREFIX}_MODULE} ${${PREFIX}_SOURCES})
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

