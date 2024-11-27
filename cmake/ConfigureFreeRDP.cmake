if(NOT FREERDP_UNIFIED_BUILD)
  find_package(WinPR 3 REQUIRED)
  include_directories(SYSTEM ${WinPR_INCLUDE_DIR})

  find_package(FreeRDP 3 REQUIRED)
  include_directories(SYSTEM ${FreeRDP_INCLUDE_DIR})

  find_package(FreeRDP-Client 3 REQUIRED)
  include_directories(SYSTEM ${FreeRDP-Client_INCLUDE_DIR})
endif()
