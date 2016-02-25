# UWAC: Using Wayland As Client
# cmake build script
#
# Copyright 2015 David FORT <contact@hardening-consulting.com>
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

# Soname versioning
set(UWAC_VERSION_MAJOR "0")
set(UWAC_VERSION_MINOR "0")
set(UWAC_VERSION_REVISION "1")
set(UWAC_VERSION "${UWAC_VERSION_MAJOR}.${UWAC_VERSION_MINOR}.${UWAC_VERSION_REVISION}")
set(UWAC_VERSION_FULL "${UWAC_VERSION}")
set(UWAC_API_VERSION "${UWAC_VERSION_MAJOR}")

add_subdirectory(include)
add_subdirectory(libuwac)
