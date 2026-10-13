add_executable(TestSspiHeaderC TestSspiHeader.c)
target_link_libraries(TestSspiHeaderC PRIVATE winpr)
set_target_properties(TestSspiHeaderC PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(TestSspiHeaderC PRIVATE -Werror=deprecated-declarations)
endif()
add_test(NAME TestSspiHeaderC COMMAND TestSspiHeaderC)

if(CMAKE_CXX_COMPILER_LOADED)
  add_executable(TestSspiHeaderCXX TestSspiHeader.cpp)
  target_link_libraries(TestSspiHeaderCXX PRIVATE winpr)
  set_target_properties(TestSspiHeaderCXX PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(TestSspiHeaderCXX PRIVATE -Werror=deprecated-declarations)
  endif()
  add_test(NAME TestSspiHeaderCXX COMMAND TestSspiHeaderCXX)
endif()

set_property(TARGET TestSspiHeaderC PROPERTY FOLDER "WinPR/Test")
if(TARGET TestSspiHeaderCXX)
  set_property(TARGET TestSspiHeaderCXX PROPERTY FOLDER "WinPR/Test")
endif()
