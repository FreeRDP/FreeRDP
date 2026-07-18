include(${CMAKE_CURRENT_LIST_DIR}/BundleUtilities.cmake)

fixup_bundle("${BUNDLE_PATH}" "" "${BUNDLE_SEARCH_PATHS}")

if(DEFINED ENV{EXPANDED_CODE_SIGN_IDENTITY} AND NOT "$ENV{EXPANDED_CODE_SIGN_IDENTITY}" STREQUAL ""
   AND EXISTS "${BUNDLE_PATH}/Frameworks"
)
  execute_process(
    COMMAND /usr/bin/find "${BUNDLE_PATH}/Frameworks" -type f -perm -111 -exec /usr/bin/codesign --force --sign
            "$ENV{EXPANDED_CODE_SIGN_IDENTITY}" --timestamp=none "{}" + COMMAND_ERROR_IS_FATAL ANY
  )
endif()
