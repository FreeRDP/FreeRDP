
set(MODULE_NAME "TestLibrary")
set(MODULE_PREFIX "TEST_LIBRARY")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestLibraryAddDllDirectory.c
	TestLibraryRemoveDllDirectory.c
	TestLibrarySetDefaultDllDirectories.c
	TestLibraryLoadLibrary.c
	TestLibraryFreeLibrary.c
	TestLibraryGetProcAddress.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

target_link_libraries(${MODULE_NAME} winpr-library winpr-path)

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${EXECUTABLE_OUTPUT_PATH}/${MODULE_NAME} ${TestName} ${EXECUTABLE_OUTPUT_PATH})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")

add_subdirectory(TestLibraryA)
add_subdirectory(TestLibraryB)
