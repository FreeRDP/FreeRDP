
set(MODULE_NAME "TestPath")
set(MODULE_PREFIX "TEST_PATH")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestPathCchAddBackslash.c
	TestPathCchRemoveBackslash.c
	TestPathCchAddBackslashEx.c
	TestPathCchRemoveBackslashEx.c
	TestPathCchAddExtension.c
	TestPathCchAppend.c
	TestPathCchAppendEx.c
	TestPathCchCanonicalize.c
	TestPathCchCanonicalizeEx.c
	TestPathAllocCanonicalize.c
	TestPathCchCombine.c
	TestPathCchCombineEx.c
	TestPathAllocCombine.c
	TestPathCchFindExtension.c
	TestPathCchRenameExtension.c
	TestPathCchRemoveExtension.c
	TestPathCchIsRoot.c
	TestPathIsUNCEx.c
	TestPathCchSkipRoot.c
	TestPathCchStripToRoot.c
	TestPathCchStripPrefix.c
	TestPathCchRemoveFileSpec.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-path)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${EXECUTABLE_OUTPUT_PATH}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")
