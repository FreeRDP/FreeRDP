# FreeRDP: A Remote Desktop Protocol Implementation
# Integration test for the out-of-process Qt-based AAD auth helper
#
# Copyright 2026 David Fort <contact@hardening-consulting.com>
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

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME TestQtAadAuthHelperProtocol
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_qt_aad_auth_helper.py
                   $<TARGET_FILE:freerdp-qt-aad-helper>
  )
  set_tests_properties(TestQtAadAuthHelperProtocol PROPERTIES TIMEOUT 90 SKIP_RETURN_CODE 125)
else()
  message(STATUS "Python3 interpreter not found - skipping TestQtAadAuthHelperProtocol registration")
endif()
