# FreeRDP cmake ios options
#
# Copyright 2013 Thincast Technologies GmbH
# Copyright 2013 Martin Fleisz <martin.fleisz@thincast.com>
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

if (NOT FREERDP_IOS_EXTERNAL_SSL_PATH)
	set(FREERDP_IOS_EXTERNAL_SSL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/openssl")
endif()
mark_as_advanced(FREERDP_IOS_EXTERNAL_SSL_PATH)

if(NOT DEFINED IOS_TARGET_SDK)
	set(IOS_TARGET_SDK 9.3 CACHE STRING "Application target iOS SDK")
endif()
