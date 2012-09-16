# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP Windows Server cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

add_executable(wfreerdp-server
	wf_update.c
	wf_update.h
	wf_dxgi.c
	wf_dxgi.h
	wf_input.c
	wf_input.h
	wf_interface.c
	wf_interface.h
	wf_mirage.c
	wf_mirage.h
	wf_peer.c
	wf_peer.h
	wf_settings.c
	wf_settings.h
	wf_info.c
	wf_info.h
	wfreerdp.c
	wfreerdp.h)

if(WITH_MONOLITHIC_BUILD)
	target_link_libraries(wfreerdp-server freerdp)

else()

	target_link_libraries(wfreerdp-server

		freerdp-core

		freerdp-utils

		freerdp-codec

		freerdp-channels)

endif()
