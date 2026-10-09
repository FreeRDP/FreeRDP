set(FUZZER_NAME TestFuzzChannelRdpgfx)

add_fuzzer_test(${FUZZER_NAME} "winpr freerdp freerdp-client")
