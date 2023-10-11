include(CheckCCompilerFlag)

macro (checkCFlag FLAG)
	CHECK_C_COMPILER_FLAG("${FLAG}" CFLAG${FLAG})
	if(CFLAG${FLAG})
		string(APPEND CMAKE_C_FLAGS " ${FLAG}")
	else()
		message(WARNING "compiler does not support ${FLAG}")
	endif()
endmacro()

option(ENABLE_WARNING_VERBOSE "enable -Weveryting (and some exceptions) for compile" ON)
option(ENABLE_WARNING_ERROR "enable -Werror for compile" OFF)

if (ENABLE_WARNING_VERBOSE)
	if (MSVC)
		set(C_WARNING_FLAGS
			/Wall
		)
	else()
		set(C_WARNING_FLAGS
			-Weverything
			-Wall
			-Wpedantic
			-Wno-padded
			-Wno-cast-align
			-Wno-declaration-after-statement
			-Wno-unsafe-buffer-usage
			-Wno-reserved-identifier
			-Wno-covered-switch-default
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
