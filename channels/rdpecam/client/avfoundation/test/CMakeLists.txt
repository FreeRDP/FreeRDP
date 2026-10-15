# FreeRDP: A Remote Desktop Protocol Implementation
# MS-RDPECAM AVFoundation camera HAL for macOS and iOS
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(MODULE_NAME "TestRdpecamAVFoundation")
set(TEST_RDPECAM_AVFOUNDATION_TESTS TestCameraAVFoundationNV12.c)
create_test_sourcelist(TEST_RDPECAM_AVFOUNDATION_SRCS TestRdpecamAVFoundation.c ${TEST_RDPECAM_AVFOUNDATION_TESTS})
add_executable(${MODULE_NAME} ${TEST_RDPECAM_AVFOUNDATION_SRCS} ../nv12.c)
target_link_libraries(${MODULE_NAME} PRIVATE freerdp-client winpr ${COREVIDEO_FRAMEWORK})
set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")
foreach(test ${TEST_RDPECAM_AVFOUNDATION_TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()
set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Channels/rdpecam/Test")
