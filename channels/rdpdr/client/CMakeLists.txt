# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2016 Inuvika Inc.
# Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

define_channel_client("rdpdr")

set(${MODULE_PREFIX}_SRCS
	irp.c
	irp.h
	devman.c
	devman.h
	rdpdr_main.c
	rdpdr_main.h
	rdpdr_capabilities.c
	rdpdr_capabilities.h
)

set(${MODULE_PREFIX}_LIBS
	winpr freerdp
)
if(APPLE AND (NOT IOS))
	find_library(CORESERVICES_LIBRARY CoreServices)
	list(APPEND ${MODULE_PREFIX}_LIBS
		${CORESERVICES_LIBRARY}
	)
endif()
add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} FALSE "VirtualChannelEntryEx")
