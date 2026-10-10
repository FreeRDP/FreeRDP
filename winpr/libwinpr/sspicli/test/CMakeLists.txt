disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

if(BUILD_TESTING OR BUILD_TESTING_INTERNAL)
  set(MODULE_NAME "TestLsa")
  set(MODULE_PREFIX "TEST_LSA")

  set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

  set(${MODULE_PREFIX}_TESTS TestLsaClient.c)

  create_test_sourcelist(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_DRIVER} ${${MODULE_PREFIX}_TESTS})

  # lsa_proto.* are module-internal (not exported from the winpr shared library), so compile
  # the wire codec straight into the test alongside the public LSA client API.
  add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS} ../lsa_proto.c)

  target_link_libraries(${MODULE_NAME} winpr ${CMAKE_THREAD_LIBS_INIT})

  set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

  foreach(test ${${MODULE_PREFIX}_TESTS})
    get_filename_component(TestName ${test} NAME_WE)
    add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
  endforeach()

  set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")
endif()

if(BUILD_FUZZERS)
  include(AddFuzzerTest)
  # TestFuzzLsaProto.c includes ../lsa_proto.c directly (module-internal codec).
  add_fuzzer_test("TestFuzzLsaProto.c" "winpr")
endif()
