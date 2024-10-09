include(CheckCCompilerFlag)

macro (checkCFlag FLAG)
	CHECK_C_COMPILER_FLAG("${FLAG}" CFLAG${FLAG})
	if(CFLAG${FLAG})
		string(APPEND CMAKE_C_FLAGS " ${FLAG}")
	else()
		message(WARNING "compiler does not support ${FLAG}")
	endif()
endmacro()

option(ENABLE_WARNING_VERBOSE "enable -Weveryting (and some exceptions) for compile" OFF)
option(ENABLE_WARNING_ERROR "enable -Werror for compile" OFF)

if (ENABLE_WARNING_VERBOSE)
	if (MSVC)
		# Remove previous warning definitions,
		# NMake is otherwise complaining.
		foreach (flags_var_to_scrub
			CMAKE_C_FLAGS
			CMAKE_C_FLAGS_DEBUG
			CMAKE_C_FLAGS_RELEASE
			CMAKE_C_FLAGS_RELWITHDEBINFO
			CMAKE_C_FLAGS_MINSIZEREL)
			string (REGEX REPLACE "(^| )[/-]W[ ]*[1-9]" " "
			"${flags_var_to_scrub}" "${${flags_var_to_scrub}}")
		endforeach()

		set(C_WARNING_FLAGS
			/W4
			/wo4324
		)
	else()
		set(C_WARNING_FLAGS
			-Weverything
			-Wall
			-Wpedantic
			-Wno-padded
			-Wno-switch-enum
			-Wno-cast-align
			-Wno-declaration-after-statement
			-Wno-unsafe-buffer-usage
			-Wno-reserved-identifier
			-Wno-covered-switch-default
			-Wno-disabled-macro-expansion
		)
	endif()

	foreach(FLAG ${C_WARNING_FLAGS})
		CheckCFlag(${FLAG})
	endforeach()
endif()


if (ENABLE_WARNING_ERROR)
	CheckCFlag(-Werror)
endif()

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} CACHE STRING "default CFLAGS")
message("Using CFLAGS ${CMAKE_C_FLAGS}")
