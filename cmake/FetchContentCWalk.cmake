option(WITH_CWALK "Compile with CWalk path sanitation" OFF)

if(WITH_CWALK)
  include(FetchContent)
  FetchContent_Declare(
    cwalk GIT_REPOSITORY https://github.com/likle/cwalk.git GIT_TAG e98d23f68807208952c179b49e4fd1813f31298d
    SYSTEM OVERRIDE_FIND_PACKAGE
  )

  FetchContent_MakeAvailable(cwalk)
endif()
