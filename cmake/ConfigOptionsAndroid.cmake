# FreeRDP cmake android options
#
# Copyright 2013 Thinstuff Technologies GmbH
# Copyright 2013 Bernhard Miklautz <bmiklautz@thinstuff.at>
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

set(JAVA_DEBUG_DEFAULT "off")
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(JAVA_DEBUG_DEFAULT "on")
endif()

option(WITH_DEBUG_ANDROID_JNI "Enable debug output for android jni bindings" ${DEFAULT_DEBUG_OPTION})
option(ANDROID_BUILD_JAVA "Automatically android java code - build type depends on CMAKE_BUILD_TYPE" ON)
option(ANDROID_BUILD_JAVA_DEBUG "Create a android debug package" ${JAVA_DEBUG_DEFAULT})
	
set(ANDROID_APP_TARGET_SDK 11 CACHE STRING "Application target android SDK")
set(ANDROID_APP_MIN_SDK 8 CACHE STRING "Application minimum android SDK requirement")
set(ANDROID_APP_GOOGLE_TARGET_SDK "16" CACHE STRING "Application target google SDK")

