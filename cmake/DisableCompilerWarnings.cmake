if (MSVC)
	set(COMPILE_WARN_OPTS "/W0")
else()
	set(COMPILE_WARN_OPTS "-w")
endif()
set(COMPILE_WARN_OPTS "${COMPILE_WARN_OPTS}" CACHE STRING "cached value")

function (disable_warnings_for_directory dir)
	if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.19.0")
		set_property(
			DIRECTORY "${dir}"
			PROPERTY COMPILE_OPTIONS ${COMPILE_WARN_OPTS}
		)
	endif()

	file(MAKE_DIRECTORY "${dir}")
	set(ctidy "${dir}/.clang-tidy")
	file(WRITE ${ctidy} "Checks: '-*,misc-definitions-in-headers'\n")
	file(APPEND ${ctidy} "CheckOptions:\n")
	file(APPEND ${ctidy} "\t- { key: HeaderFileExtensions,          value: \"x\" }\n")
endfunction()
