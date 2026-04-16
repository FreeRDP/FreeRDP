set(FUZZER_NAME TestFuzzChannelCliprdr)

add_executable(${FUZZER_NAME} TestFuzzChannelCliprdr.c ../../cliprdr_common.c)
target_link_libraries(${FUZZER_NAME} PUBLIC freerdp winpr fuzzer_config)
set_target_properties(${FUZZER_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")
add_dependencies(fuzzers ${FUZZER_NAME})

set_property(TARGET ${FUZZER_NAME} PROPERTY FOLDER "Channels/cliprdr/Client/Test")
