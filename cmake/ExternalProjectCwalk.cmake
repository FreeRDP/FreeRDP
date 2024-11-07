# Include Cwalk library as external project.
#
# Download from github and build the library.
include(FetchContent)

# Disable -Werror for cwalk build unless we have set it
if (NOT ENABLE_WARNING_ERROR)
    set(IGNORE_WARNINGS ON CACHE INTERNAL "[Cwalk] cached build variable")
endif()

# If there is already a local copy of cwalk use it
if (EXISTS ${CMAKE_SOURCE_DIR}/external/cwalk)
    set(FETCHCONTENT_SOURCE_DIR_CWALK ${CMAKE_SOURCE_DIR}/external/cwalk)
endif()

FetchContent_Declare(
  Cwalk
  # URL https://github.com/likle/cwalk/archive/refs/tags/v1.2.9.tar.gz
  # URL_HASH SHA256=54f160031687ec90a414e0656cf6266445207cb91b720dacf7a7c415d6bc7108

  GIT_REPOSITORY https://github.com/likle/cwalk.git
  GIT_TAG v1.2.9
  GIT_SHALLOW ON
  OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(Cwalk)

