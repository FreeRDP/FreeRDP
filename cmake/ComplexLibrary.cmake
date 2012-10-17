
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
