# This script returns the current date in ISO format
#
# YYYY-MM-DD
#
MACRO (TODAY RESULT)
    if (DEFINED ENV{SOURCE_DATE_EPOCH} AND NOT WIN32)
        EXECUTE_PROCESS(COMMAND "date" "-u" "-d" "@$ENV{SOURCE_DATE_EPOCH}" "+%Y-%m-%d"
                        OUTPUT_VARIABLE ${RESULT} OUTPUT_STRIP_TRAILING_WHITESPACE)
    elseif(CMAKE_VERSION VERSION_LESS "2.8.11")
	if (WIN32)
		message(FATAL_ERROR "Your CMake version is too old. Please update to a more recent version >= 2.8.11")
	else()
		EXECUTE_PROCESS(COMMAND "date" "-u" "+%Y-%m-%d"
                        OUTPUT_VARIABLE ${RESULT} OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()
    else()
        STRING(TIMESTAMP ${RESULT} "%Y-%m-%d" UTC)
    endif()
ENDMACRO (TODAY)
