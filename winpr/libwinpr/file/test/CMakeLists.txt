
set(MODULE_NAME "TestFile")
set(MODULE_PREFIX "TEST_FILE")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestFileCreateFile.c
	TestFileDeleteFile.c
	TestFileReadFile.c
	TestFileWriteFile.c
	TestFilePatternMatch.c
	TestFileFindFirstFile.c
	TestFileFindFirstFileEx.c
	TestFileFindNextFile.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-file winpr-path)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

if(NOT MSVC)
	set(TEST_AREA "${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME}Area")
else()
	set(TEST_AREA "${TESTING_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/${MODULE_NAME}Area")
endif()

file(MAKE_DIRECTORY "${TEST_AREA}")
file(WRITE "${TEST_AREA}/TestFile1" "TestFile1")
file(WRITE "${TEST_AREA}/TestFile2" "TestFile2")
file(WRITE "${TEST_AREA}/TestFile3" "TestFile3")
file(MAKE_DIRECTORY "${TEST_AREA}/TestDirectory1")
file(WRITE "${TEST_AREA}/TestDirectory1/TestDirectory1File1" "TestDirectory1File1")
file(MAKE_DIRECTORY "${TEST_AREA}/TestDirectory2")
file(WRITE "${TEST_AREA}/TestDirectory2/TestDirectory2File1" "TestDirectory2File1")
file(WRITE "${TEST_AREA}/TestDirectory2/TestDirectory2File2" "TestDirectory2File2")
file(MAKE_DIRECTORY "${TEST_AREA}/TestDirectory3")
file(WRITE "${TEST_AREA}/TestDirectory3/TestDirectory3File1" "TestDirectory3File1")
file(WRITE "${TEST_AREA}/TestDirectory3/TestDirectory3File2" "TestDirectory3File2")
file(WRITE "${TEST_AREA}/TestDirectory3/TestDirectory3File3" "TestDirectory3File3")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName} ${TEST_AREA})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")
