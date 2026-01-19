option(USE_VERSION_FROM_GIT_TAG "Extract FreeRDP version from git tag." ON)
option(USE_GIT_FOR_REVISION "Extract git tag/commit" OFF)

function(get_project_version VERSION_MAJOR VERSION_MINOR VERSION_REVISION VERSION_SUFFIX GIT_REVISION)

  # Default version, hard codec per release
  set(RAW_VERSION_STRING "3.21.1-dev0")

  set(VERSION_REGEX "^(.*)([0-9]+)\\.([0-9]+)\\.([0-9]+)-?(.*)")

  # Prefer version from .source_tag file
  if(EXISTS "${PROJECT_SOURCE_DIR}/.source_tag")
    file(READ ${PROJECT_SOURCE_DIR}/.source_tag RAW_VERSION_STRING)
    # otherwise try to extract the version from a git tag
  elseif(USE_VERSION_FROM_GIT_TAG)
    git_get_exact_tag(_GIT_TAG --tags --always)
    if(NOT ${_GIT_TAG} STREQUAL "n/a")
      string(REGEX MATCH ${VERSION_REGEX} FOUND_TAG "${_GIT_TAG}")
      if(FOUND_TAG)
        set(RAW_VERSION_STRING ${_GIT_TAG})
      endif()
    endif()
  endif()

  # Default git revision
  set(FKT_GIT_REVISION "n/a")

  # Prefer git revision from .source_version file
  if(EXISTS "${PROJECT_SOURCE_DIR}/.source_version")
    file(READ ${PROJECT_SOURCE_DIR}/.source_version FKT_GIT_REVISION)
    string(STRIP ${FKT_GIT_REVISION} FKT_GIT_REVISION)
    # otherwise try to call git and extract tag/commit
  elseif(USE_VERSION_FROM_GIT_TAG OR USE_GIT_FOR_REVISION)
    git_rev_parse(FKT_GIT_REVISION --short)
  endif()

  string(STRIP ${RAW_VERSION_STRING} RAW_VERSION_STRING)
  string(REGEX REPLACE "${VERSION_REGEX}" "\\2" FKT_VERSION_MAJOR "${RAW_VERSION_STRING}")
  string(REGEX REPLACE "${VERSION_REGEX}" "\\3" FKT_VERSION_MINOR "${RAW_VERSION_STRING}")
  string(REGEX REPLACE "${VERSION_REGEX}" "\\4" FKT_VERSION_REVISION "${RAW_VERSION_STRING}")
  string(REGEX REPLACE "${VERSION_REGEX}" "\\5" FKT_VERSION_SUFFIX "${RAW_VERSION_STRING}")

  set(${VERSION_MAJOR} ${FKT_VERSION_MAJOR} PARENT_SCOPE)
  set(${VERSION_MINOR} ${FKT_VERSION_MINOR} PARENT_SCOPE)
  set(${VERSION_REVISION} ${FKT_VERSION_REVISION} PARENT_SCOPE)
  set(${VERSION_SUFFIX} ${FKT_VERSION_SUFFIX} PARENT_SCOPE)
  set(${GIT_REVISION} ${FKT_GIT_REVISION} PARENT_SCOPE)
endfunction()
