set(MODULE_NAME "TestSDL")
set(MODULE_PREFIX "TEST_SDL")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.cpp)

set(${MODULE_PREFIX}_TESTS TestSDLPrefs.cpp)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS freerdp winpr sdl_prefs)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

set(TEST_AREA "${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME}Area")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
	set_property(TEST ${TestName} PROPERTY ENVIRONMENT "XDG_CONFIG_HOME=${TEST_AREA}")
endforeach()

file(MAKE_DIRECTORY "${TEST_AREA}")
file(MAKE_DIRECTORY "${TEST_AREA}/${VENDOR}")
file(MAKE_DIRECTORY "${TEST_AREA}/${VENDOR}/${PRODUCT}")
file(COPY "sdl-freerdp.json" DESTINATION ${TEST_AREA}/${VENDOR}/${PRODUCT})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Client/Test")