include(AddFuzzerTest)

set(FUZZERS TestFuzzDrdynvc.c)

add_fuzzer_test("${FUZZERS}" "winpr")
