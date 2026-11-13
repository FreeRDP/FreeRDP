set(FUZZER_NAME TestFuzzChannelRdpgfx)

include(AddFuzzerTest)
add_fuzzer_test(${FUZZER_NAME} "winpr freerdp freerdp-client")
