set(add_resource_macro_internal_dir ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")

macro(AddTargetWithResourceFile nameAndTarget is_exe version sources)
    list(LENGTH ${nameAndTarget} target_length)
    if (target_length GREATER 1)
        list(GET ${nameAndTarget} 1 name)
        list(GET ${nameAndTarget} 0 target)
    else()
        set(name ${nameAndTarget})
        set(target ${nameAndTarget})
    endif()

    set(VERSIONING OFF)
    set(IS_EXE OFF)
    if ("${is_exe}" MATCHES "TRUE")
        set(IS_EXE ON)
    elseif ("${is_exe}" MATCHES "WIN32")
        set(IS_EXE ON)
        set(exe_options "WIN32")
    elseif ("${is_exe}" MATCHES "SHARED")
        set(lib_options "SHARED")
    elseif ("${is_exe}" MATCHES "STATIC")
        set(lib_options "STATIC")
    endif()

    if (IS_EXE AND WITH_BINARY_VERSIONING)
        set(VERSIONING ON)
    elseif (NOT IS_EXE AND WITH_LIBRARY_VERSIONING)
        set(VERSIONING ON)
    endif()
    if (${ARGC} GREATER 4)
        set(VERSIONING ${ARGV5})
    endif()

    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*"
           RC_PROGRAM_VERSION_MATCH ${version})
    set (RC_VERSION_MAJOR ${CMAKE_MATCH_1})
    set (RC_VERSION_MINOR ${CMAKE_MATCH_2})
    set (RC_VERSION_BUILD ${CMAKE_MATCH_3})

    if (WIN32)
      if (IS_EXE)
          if (VERSIONING)
              set (RC_VERSION_FILE "${name}${RC_VERSION_MAJOR}${CMAKE_EXECUTABLE_SUFFIX}" )
          else()
              set (RC_VERSION_FILE "${name}${CMAKE_EXECUTABLE_SUFFIX}" )
          endif()
      else()
          if (VERSIONING)
              set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${name}${RC_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}" )
          else()
              set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${name}${CMAKE_SHARED_LIBRARY_SUFFIX}" )
          endif()
      endif()

      configure_file(
          ${add_resource_macro_internal_dir}/WindowsDLLVersion.rc.in
          ${CMAKE_CURRENT_BINARY_DIR}/version.rc
          @ONLY
      )

      list(APPEND ${sources} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
    endif()

    set(OUTPUT_FILENAME "${name}")
    if (VERSIONING)
        string(APPEND OUTPUT_FILENAME "${RC_VERSION_MAJOR}")
    endif()

    if (IS_EXE)
        message("add_executable(${target}) [${exe_options}]")
        add_executable(
            ${target}
            ${exe_options}
            ${${sources}}
        )

        set_target_properties(
            ${target}
            PROPERTIES
            OUTPUT_NAME ${OUTPUT_FILENAME}
        )
        string(APPEND OUTPUT_FILENAME "${CMAKE_EXECUTABLE_SUFFIX}")
    else()
        message("add_library(${target}) [${lib_options}]")
        add_library(${target}
            ${lib_options}
            ${${sources}})

        if (VERSIONING)
            set_target_properties(
                ${target}
                PROPERTIES
                    VERSION ${version}
                    SOVERSION ${RC_VERSION_MAJOR}
                )
        else()
            set_target_properties(${target} PROPERTIES PREFIX "")
        endif()

        set_target_properties(
                                ${target}
                                PROPERTIES
                                OUTPUT_NAME ${OUTPUT_FILENAME}
                            )
        set (OUTPUT_FILENAME "${CMAKE_SHARED_LIBRARY_PREFIX}${OUTPUT_FILENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" )
    endif()


    if (WITH_DEBUG_SYMBOLS AND MSVC AND (is_exe OR BUILD_SHARED_LIBS))
        message("add PDB for ${OUTPUT_FILENAME}")
        set_target_properties(
            ${target}
            PROPERTIES
                PDB_NAME ${OUTPUT_FILENAME}
        )
        install(FILES $<TARGET_PDB_FILE:${target}> DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT symbols)
    endif()
endmacro()
