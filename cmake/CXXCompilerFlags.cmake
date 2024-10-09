include(CheckCXXCompilerFlag)

macro (checkCXXFlag FLAG)
	check_cxx_compiler_flag("${FLAG}" CXXFLAG${FLAG})
	if(CXXFLAG${FLAG})
		string(APPEND CMAKE_CXX_FLAGS " ${FLAG}")
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
			CMAKE_CXX_FLAGS
			CMAKE_CXX_FLAGS_DEBUG
			CMAKE_CXX_FLAGS_RELEASE
			CMAKE_CXX_FLAGS_RELWITHDEBINFO
			CMAKE_CXX_FLAGS_MINSIZEREL)
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
			-Wno-ctad-maybe-unsupported
			-Wno-c++98-compat
			-Wno-c++98-compat-pedantic
			-Wno-pre-c++17-compat
			-Wno-exit-time-destructors
		)
	endif()

	foreach(FLAG ${C_WARNING_FLAGS})
		CheckCXXFlag(${FLAG})
	endforeach()
endif()


if (ENABLE_WARNING_ERROR)
	CheckCXXFlag(-Werror)
endif()

# https://stackoverflow.com/questions/4913922/possible-problems-with-nominmax-on-visual-c
if (WIN32)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-DNOMINMAX>)
endif()

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} CACHE STRING "default CXXFLAGS")
message("Using CXXFLAGS ${CMAKE_CXX_FLAGS}")
