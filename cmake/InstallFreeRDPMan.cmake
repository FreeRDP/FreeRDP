include(today)
include(GNUInstallDirs)
include(CleaningConfigureFile)

get_filename_component(INSTALL_FREERDP_MAN_SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

function(install_freerdp_man manpage section)
  if(WITH_MANPAGES)
    install(FILES ${manpage} DESTINATION ${CMAKE_INSTALL_MANDIR}/man${section})
  endif()
endfunction()

function(generate_and_install_freerdp_man_from_template name_base section api)
  if(WITH_MANPAGES)
    if(WITH_BINARY_VERSIONING)
      set(manpage "${CMAKE_CURRENT_BINARY_DIR}/${name_base}${api}.${section}")
    else()
      set(manpage "${CMAKE_CURRENT_BINARY_DIR}/${name_base}.${section}")
    endif()
    cleaning_configure_file(${name_base}.${section}.in ${manpage})
    install_freerdp_man(${manpage} ${section})
  endif()
endfunction()

# Generate an install target for a manpage.
#
# This is not as simple as it looks like:
#
# 1. extract the raw file names (files that require configure_file end with .in, ready to use files
#    with .1 or some other manpage related number)
# 2. do the same for every dependency
# 3. create a command to run during build. Add a few defined symbols by default
# 4. add variable names passed to the function to the command
# 5. run CMake -P as custom_target during build.
#   * run configure_file for all .in files
#   * concatenate all manpage sections to the target manpage
# 6. create the actual install target
function(generate_and_install_freerdp_man_from_xml target section dependencies variable_names)
  if(WITH_MANPAGES)
    get_target_property(name_base "${target}" OUTPUT_NAME)
    set(template "${target}.${section}")
    set(MANPAGE_NAME "${name_base}")
    set(manpage "${name_base}.${section}")

    # We need the variable ${MAN_TODAY} to contain the current date in ISO
    # format to replace it in the cleaning_configure_file step.
    include(today)

    today(MAN_TODAY)

    set(GENERATE_COMMAND
        -Dtemplate=\"${template}\" -DMANPAGE_NAME=\"${MANPAGE_NAME}\" -Dmanpage=\"${manpage}\"
        -DMAN_TODAY=\"${MAN_TODAY}\" -DCURRENT_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"
        -DCURRENT_BINARY_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\" -Dtarget="${target}" -Dsection="${section}"
        -Ddependencies="${dependencies}"
    )

    foreach(var IN ITEMS ${variable_names})
      list(APPEND GENERATE_COMMAND -D${var}=${${var}})
    endforeach()

    list(APPEND GENERATE_COMMAND -P \"${INSTALL_FREERDP_MAN_SCRIPT_DIR}/GenerateManpages.cmake\")

    add_custom_target(
      ${manpage}.target ALL COMMAND ${CMAKE_COMMAND} ${GENERATE_COMMAND} DEPENDS generate_argument_manpage.target
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    install_freerdp_man(${CMAKE_CURRENT_BINARY_DIR}/${manpage} ${section})
  endif()
endfunction()
