# get all project files
file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.c *.h *.m *.java)
# minimum version required
set(_CLANG_FORMAT_MINIMUM_VERSION 7.0.0)

find_program(CLANG_FORMAT
	NAMES
	clang-format-8
	clang-format-7
	clang-format
	)

if (NOT CLANG_FORMAT)
	message(WARNING "clang-format not found in path! code format target not available.")
else()
	execute_process(
		COMMAND ${CLANG_FORMAT} "--version"
		OUTPUT_VARIABLE _CLANG_FORMAT_VERSION
		RESULT_VARIABLE _CLANG_FORMAT_VERSION_FAILED
		)

	if (_CLANG_FORMAT_VERSION_FAILED)
		message(WARNING "A problem was encounterd with ${CLANG_FORMAT}")
		return()
	endif()

	string(REGEX MATCH "([7-9]|[1-9][0-9])\\.[0-9]\\.[0-9]" CLANG_FORMAT_VERSION
		"${_CLANG_FORMAT_VERSION}")

	if (NOT CLANG_FORMAT_VERSION)
		message(WARNING "problem parsing clang-format version for ${CLANG_FORMAT}")
		return()
	endif()

	if (${CLANG_FORMAT_VERSION} VERSION_LESS ${_CLANG_FORMAT_MINIMUM_VERSION})
		message(WARNING "clang-format version ${CLANG_FORMAT_VERSION} not supported")
		message(WARNING "Minimum version required: ${_CLANG_FORMAT_MINIMUM_VERSION}")
		return()
	endif()

	add_custom_target(
			clangformat
			COMMAND ${CLANG_FORMAT}
			-style=file
			-i
			${ALL_SOURCE_FILES}
			)
endif()
