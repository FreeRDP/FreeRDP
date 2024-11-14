include(CheckCXXCompilerFlag)
include(CommonCompilerFlags)

macro (checkCXXFlag FLAG)
	check_cxx_compiler_flag("${FLAG}" CXXFLAG${FLAG})
	if(CXXFLAG${FLAG})
		string(APPEND CMAKE_CXX_FLAGS " ${FLAG}")
	else()
		message(WARNING "compiler does not support ${FLAG}")
	endif()
endmacro()

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
	else()
        list(APPEND COMMON_COMPILER_FLAGS
			-Wno-declaration-after-statement
			-Wno-ctad-maybe-unsupported
			-Wno-c++98-compat
			-Wno-c++98-compat-pedantic
			-Wno-pre-c++17-compat
			-Wno-exit-time-destructors
			-Wno-gnu-zero-variadic-macro-arguments
		)
	endif()
endif()

foreach(FLAG ${COMMON_COMPILER_FLAGS})
	CheckCXXFlag(${FLAG})
endforeach()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.>)
		add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.>)
		add_compile_options($<$<NOT:$<CONFIG:Debug>>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>)
		add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fdebug-prefix-map=${CMAKE_BINARY_DIR}=./build>)
		add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fmacro-prefix-map=${CMAKE_BINARY_DIR}=./build>)
		add_compile_options($<$<NOT:$<CONFIG:Debug>>:-ffile-prefix-map=${CMAKE_BINARY_DIR}=./build>)
endif()

# https://stackoverflow.com/questions/4913922/possible-problems-with-nominmax-on-visual-c
if (WIN32)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-DNOMINMAX>)
endif()

if(MSVC)
    add_compile_options(/Gd)

	set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
	set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})

    add_compile_options("$<$<CONFIG:Debug>:/Zi>")
	add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
endif()

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} CACHE STRING "default CXXFLAGS")
message("Using CXXFLAGS ${CMAKE_CXX_FLAGS}")
message("Using CXXFLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE}")
message("Using CXXFLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG}")
message("Using CXXFLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message("Using CXXFLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
