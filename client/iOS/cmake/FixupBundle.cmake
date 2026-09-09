include(${CMAKE_CURRENT_LIST_DIR}/BundleUtilities.cmake)

fixup_bundle("${BUNDLE_PATH}" "" "${BUNDLE_SEARCH_PATHS}")

## package normal dylib to framework

set(FW_ROOT "${BUNDLE_PATH}/Frameworks")
get_filename_component(APP_NAME "${BUNDLE_PATH}" NAME_WE)
set(MAIN_EXE "${BUNDLE_PATH}/${APP_NAME}")

# libname.1.2.3.dylib -> name
macro(_fw_name out base)
  string(REGEX REPLACE "^lib([^.]+)\\..*$" "\\1" ${out} "${base}")
endmacro()

# generate plist for framework
function(_fw_write_plist fw_dir fw_name)
  set(FW_NAME "${fw_name}")
  string(REGEX REPLACE "[^A-Za-z0-9.-]" "-" FW_BID "${FW_BUNDLE_ID_PREFIX}.${fw_name}")
  configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Framework.plist.in" "${fw_dir}/Info.plist" @ONLY)
endfunction()

# get real dylib name (ex : abc.1.2.3.dylib, abc.1.dylib, abc.dylib --> abc)
file(GLOB _dylibs LIST_DIRECTORIES false "${FW_ROOT}/*.dylib")
set(_real_dylibs)
foreach(_dylib IN LISTS _dylibs)
  file(REAL_PATH "${_dylib}" _real_dylib)
  list(APPEND _real_dylibs "${_real_dylib}")
endforeach()
list(REMOVE_DUPLICATES _real_dylibs)

# set dylib dependencies
set(_bins "${MAIN_EXE}" ${_real_dylibs})
foreach(_bin IN LISTS _bins)
  execute_process(COMMAND otool -L "${_bin}" OUTPUT_VARIABLE _otool ERROR_QUIET)
  string(REPLACE "\n" ";" _otool "${_otool}")
  foreach(_line IN LISTS _otool)
    if(NOT _line MATCHES "^[ \t]*([^ \t]+)[ \t]+\\(compatibility")
      continue()
    endif()

    set(_dep "${CMAKE_MATCH_1}")
    get_filename_component(_depbase "${_dep}" NAME)
    if(NOT EXISTS "${FW_ROOT}/${_depbase}")
      continue()
    endif()

    _fw_name(_fwname "${_depbase}")
    execute_process(
      COMMAND install_name_tool -change "${_dep}" "@executable_path/Frameworks/${_fwname}.framework/${_fwname}"
              "${_bin}" ERROR_QUIET
    )
  endforeach()
endforeach()

# create framework (create directory ,change self @rpath, generate plist)
foreach(_real IN LISTS _real_dylibs)
  get_filename_component(_realbase "${_real}" NAME)
  _fw_name(_fwname "${_realbase}")
  set(_newref "@executable_path/Frameworks/${_fwname}.framework/${_fwname}")
  execute_process(COMMAND install_name_tool -id "${_newref}" "${_real}" ERROR_QUIET)

  set(_fwdir "${FW_ROOT}/${_fwname}.framework")
  file(MAKE_DIRECTORY "${_fwdir}")
  file(RENAME "${_real}" "${_fwdir}/${_fwname}")
  _fw_write_plist("${_fwdir}" "${_fwname}")
endforeach()

# remove unused dylib (like abc.1.2.3.dylib symbolic)
file(GLOB _leftover_dylibs LIST_DIRECTORIES false "${FW_ROOT}/*.dylib")
file(REMOVE ${_leftover_dylibs})

# BundleUtilities full-framework copy assumes a versioned layout.
# ios require flat frameworks. so just restore Info.plist
file(GLOB _frameworks LIST_DIRECTORIES true "${FW_ROOT}/*.framework")
foreach(_framework IN LISTS _frameworks)
  if(EXISTS "${_framework}/Info.plist")
    continue()
  endif()

  get_filename_component(_framework_name "${_framework}" NAME)
  set(_source_plist "${BUNDLE_SEARCH_PATHS}/${_framework_name}/Info.plist")
  if(EXISTS "${_source_plist}")
    # if original plist exists (png.framework) copy it
    file(COPY_FILE "${_source_plist}" "${_framework}/Info.plist" ONLY_IF_DIFFERENT)
  else()
    # fallback
    get_filename_component(_fwname "${_framework}" NAME_WE)
    _fw_write_plist("${_framework}" "${_fwname}")
  endif()
endforeach()

# codesign
if(DEFINED ENV{EXPANDED_CODE_SIGN_IDENTITY} AND NOT "$ENV{EXPANDED_CODE_SIGN_IDENTITY}" STREQUAL "")
  foreach(_framework IN LISTS _frameworks)
    execute_process(
      COMMAND /usr/bin/codesign --force --sign "$ENV{EXPANDED_CODE_SIGN_IDENTITY}" --timestamp=none "${_framework}"
              COMMAND_ERROR_IS_FATAL ANY
    )
  endforeach()
endif()
