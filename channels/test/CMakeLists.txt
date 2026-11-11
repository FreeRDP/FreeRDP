add_library(freerdp-channels-test STATIC dvc_test_host.c dvc_test_host.h)
target_include_directories(freerdp-channels-test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(freerdp-channels-test PUBLIC winpr freerdp)
set_property(TARGET freerdp-channels-test PROPERTY FOLDER "FreeRDP/Channels/Test")
