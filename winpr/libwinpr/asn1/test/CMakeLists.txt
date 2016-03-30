
set(MODULE_NAME "TestAsn1")
set(MODULE_PREFIX "TEST_ASN1")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestAsn1Module.c
	TestAsn1Encoder.c
	TestAsn1Decoder.c
	TestAsn1Encode.c
	TestAsn1Decode.c
	TestAsn1String.c
	TestAsn1Integer.c
	TestAsn1Compare.c
	TestAsn1BerEnc.c
	TestAsn1BerDec.c
	TestAsn1DerEnc.c
	TestAsn1DerDec.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")
