
function(windows_rc_generate_version_info)

    set(RC_ARGS_OPTIONS "")
    set(RC_ARGS_SINGLE "NAME;TYPE;VERSION;VENDOR;FILENAME;COPYRIGHT;OUTPUT")
    set(RC_ARGS_MULTI "")

    cmake_parse_arguments(RC "${RC_ARGS_OPTIONS}" "${RC_ARGS_SINGLE}" "${RC_ARGS_MULTI}" ${ARGN})

    if(NOT DEFINED RC_TYPE OR "${RC_TYPE}" STREQUAL "")
        set(RC_TYPE "APP")
    endif()

    if(NOT DEFINED RC_VERSION OR "${RC_VERSION}" STREQUAL "")
        set(RC_VERSION "1.0.0")
    endif()

    # File Version

    if(NOT DEFINED RC_FILE_VERSION OR "${RC_FILE_VERSION}" STREQUAL "")
        set(RC_FILE_VERSION ${RC_VERSION})
    endif()

    string(REPLACE "." ";" RC_FILE_VERSION_NUMBERS ${RC_FILE_VERSION})
    list(LENGTH RC_FILE_VERSION_NUMBERS RC_FILE_VERSION_LENGTH)
    
    if(RC_FILE_VERSION_LENGTH LESS 4)
        list(APPEND RC_FILE_VERSION_NUMBERS "0")
    endif()

    string(REPLACE ";" "," RC_FILE_VERSION_NUMBER "${RC_FILE_VERSION_NUMBERS}")
    string(REPLACE ";" "." RC_FILE_VERSION_STRING "${RC_FILE_VERSION_NUMBERS}")

    # Product Version

    if(NOT DEFINED RC_PRODUCT_VERSION OR "${RC_PRODUCT_VERSION}" STREQUAL "")
        set(RC_PRODUCT_VERSION ${RC_VERSION})
    endif()

    string(REPLACE "." ";" RC_PRODUCT_VERSION_NUMBERS ${RC_PRODUCT_VERSION})
    list(LENGTH RC_PRODUCT_VERSION_NUMBERS RC_PRODUCT_VERSION_LENGTH)
    
    if(RC_PRODUCT_VERSION_LENGTH LESS 4)
        list(APPEND RC_PRODUCT_VERSION_NUMBERS "0")
    endif()

    string(REPLACE ";" "," RC_PRODUCT_VERSION_NUMBER "${RC_PRODUCT_VERSION_NUMBERS}")
    string(REPLACE ";" "." RC_PRODUCT_VERSION_STRING "${RC_PRODUCT_VERSION_NUMBERS}")

    set(RC_FILEOS "0x40004L")

    if(RC_TYPE STREQUAL "APP")
        set(RC_FILETYPE "0x1L") # VFT_APP
    elseif(RC_TYPE STREQUAL "DLL")
        set(RC_FILETYPE "0x2L") # VFT_DLL
    elseif(RC_TYPE STREQUAL "DRV")
        set(RC_FILETYPE "0x3L") # VFT_DRV
    else()
        set(RC_FILETYPE "0x0L") # VFT_UNKNOWN
    endif()

    set(RC_FILESUBTYPE "0x0L")

    set(RC_COMPANY_NAME "${RC_VENDOR}")
    set(RC_FILE_DESCRIPTION "${RC_NAME}")
    set(RC_PRODUCT_NAME "${RC_NAME}")
    set(RC_INTERNAL_NAME ${RC_FILENAME})
    set(RC_ORIGINAL_FILENAME ${RC_FILENAME})
    set(RC_LEGAL_COPYRIGHT ${RC_COPYRIGHT})

    # VERSIONINFO resource
    # https://msdn.microsoft.com/en-us/library/windows/desktop/aa381058

    set(LINES "")
    list(APPEND LINES "")
    list(APPEND LINES "#include <winresrc.h>")
    list(APPEND LINES "")
    list(APPEND LINES "VS_VERSION_INFO VERSIONINFO")
    list(APPEND LINES " FILEVERSION ${RC_FILE_VERSION_NUMBER}")
    list(APPEND LINES " PRODUCTVERSION ${RC_PRODUCT_VERSION_NUMBER}")
    list(APPEND LINES " FILEFLAGSMASK 0x3fL")
    list(APPEND LINES "#ifdef _DEBUG")
    list(APPEND LINES " FILEFLAGS 0x1L")
    list(APPEND LINES "#else")
    list(APPEND LINES " FILEFLAGS 0x0L")
    list(APPEND LINES "#endif")
    list(APPEND LINES " FILEOS ${RC_FILEOS}")
    list(APPEND LINES " FILETYPE ${RC_FILETYPE}")
    list(APPEND LINES " FILESUBTYPE ${RC_FILESUBTYPE}")
    list(APPEND LINES "BEGIN")
    list(APPEND LINES "    BLOCK \"StringFileInfo\"")
    list(APPEND LINES "    BEGIN")
    list(APPEND LINES "        BLOCK \"040904b0\"")
    list(APPEND LINES "        BEGIN")
    list(APPEND LINES "            VALUE \"CompanyName\", \"${RC_COMPANY_NAME}\"")
    list(APPEND LINES "            VALUE \"FileDescription\", \"${RC_FILE_DESCRIPTION}\"")
    list(APPEND LINES "            VALUE \"FileVersion\", \"${RC_FILE_VERSION_STRING}\"")
    list(APPEND LINES "            VALUE \"InternalName\", \"${RC_INTERNAL_NAME}\"")
    list(APPEND LINES "            VALUE \"LegalCopyright\", \"${RC_LEGAL_COPYRIGHT}\"")
    list(APPEND LINES "            VALUE \"OriginalFilename\", \"${RC_ORIGINAL_FILENAME}\"")
    list(APPEND LINES "            VALUE \"ProductName\", \"${RC_PRODUCT_NAME}\"")
    list(APPEND LINES "            VALUE \"ProductVersion\", \"${RC_PRODUCT_VERSION_STRING}\"")
    list(APPEND LINES "        END")
    list(APPEND LINES "    END")
    list(APPEND LINES "    BLOCK \"VarFileInfo\"")
    list(APPEND LINES "    BEGIN")
    list(APPEND LINES "        VALUE \"Translation\", 0x409, 1200")
    list(APPEND LINES "    END")
    list(APPEND LINES "END")
    list(APPEND LINES "")

    set(RC_FILE "")
    foreach(LINE ${LINES})
        set(RC_FILE "${RC_FILE}${LINE}\n")
    endforeach()

    file(WRITE "${RC_OUTPUT}" ${RC_FILE})
endfunction()
