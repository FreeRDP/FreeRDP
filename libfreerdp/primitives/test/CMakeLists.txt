
set(MODULE_NAME "TestPrimitives")
set(MODULE_PREFIX "TEST_FREERDP_PRIMITIVES")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestPrimitives16to32bpp.c
	TestPrimitivesAdd.c
	TestPrimitivesAlphaComp.c
	TestPrimitivesAndOr.c
	TestPrimitivesColors.c
	TestPrimitivesCopy.c
	TestPrimitivesSet.c
	TestPrimitivesShift.c
	TestPrimitivesSign.c
	TestPrimitivesYUV.c
	TestPrimitivesYCbCr.c
	TestPrimitivesYCoCg.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

set(${MODULE_PREFIX}_EXTRA_SRCS
	prim_test.c
	prim_test.h
	measure.h)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_EXTRA_SRCS})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

add_definitions(-DPRIM_STATIC=auto -DALL_PRIMITIVES_VERSIONS)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Test")

