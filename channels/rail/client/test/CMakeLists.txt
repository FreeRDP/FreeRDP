set(FUZZER_NAME TestFuzzChannelRail)

add_fuzzer_test(${FUZZER_NAME} "winpr freerdp freerdp-client")
