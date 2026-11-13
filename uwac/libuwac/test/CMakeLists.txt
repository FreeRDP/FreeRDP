add_executable(TestUwacClipboard TestUwacClipboard.c)
target_link_libraries(TestUwacClipboard PRIVATE uwac ${WAYLAND_LIBS})
add_test(NAME TestUwacClipboard COMMAND TestUwacClipboard)
set_tests_properties(TestUwacClipboard PROPERTIES TIMEOUT 10)
