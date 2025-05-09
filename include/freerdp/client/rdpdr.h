/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Extension
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_RDPDR_CLIENT_H
#define FREERDP_CHANNEL_RDPDR_CLIENT_H

#include <freerdp/channels/rdpdr.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_rdpdr_client_context RdpdrClientContext;

	typedef UINT (*pcRdpdrRegisterDevice)(RdpdrClientContext* context, const RDPDR_DEVICE* device,
	                                      uintptr_t* pid);
	typedef UINT (*pcRdpdrUnregisterDevice)(RdpdrClientContext* context, uintptr_t id);

	struct s_rdpdr_client_context
	{
		void* handle;
		void* custom;

		pcRdpdrRegisterDevice RdpdrRegisterDevice;
		pcRdpdrUnregisterDevice RdpdrUnregisterDevice;
	};

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPDR_CLIENT_H */
