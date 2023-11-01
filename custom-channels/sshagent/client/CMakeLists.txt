# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2017 Ben Cohen
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

set(MODULE_NAME "sshagent-client")

set(SRCS
	sshagent_main.c
	sshagent_main.h
	sshagent.h
)

set(LIBS
	freerdp
	winpr
)

include_directories(..)

add_library(${MODULE_NAME}
	SHARED
 		${SRCS}
	)
target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})
install(TARGETS ${MODULE_NAME} COMPONENT channels EXPORT FreeRDPTargets
        ARCHIVE DESTINATION ${FREERDP_PLUGIN_PATH}
        LIBRARY DESTINATION ${FREERDP_PLUGIN_PATH}
        RUNTIME DESTINATION ${FREERDP_PLUGIN_PATH}
)
