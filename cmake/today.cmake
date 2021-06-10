# This script returns the current date in ISO format
#
# YYYY-MM-DD
#
MACRO (TODAY RESULT)
    if (DEFINED ENV{SOURCE_DATE_EPOCH} AND NOT WIN32)
        EXECUTE_PROCESS(COMMAND "date" "-u" "-d" "@$ENV{SOURCE_DATE_EPOCH}" "+%Y-%m-%d"
                        OUTPUT_VARIABLE ${RESULT} OUTPUT_STRIP_TRAILING_WHITESPACE)
        STRING(TIMESTAMP ${RESULT} "%Y-%m-%d" UTC)
    endif()
ENDMACRO (TODAY)
