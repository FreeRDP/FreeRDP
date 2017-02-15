# FreeRDP cmake android options
#
# Copyright 2013 Thincast Technologies GmbH
# Copyright 2013 Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

option(WITH_OPENSLES "Enable sound and microphone redirection using OpenSLES" ON)

set(ANDROID_APP_TARGET_SDK 21 CACHE STRING "Application target android SDK")
set(ANDROID_APP_MIN_SDK 14 CACHE STRING "Application minimum android SDK requirement")

