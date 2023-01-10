/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPDR utility functions
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef FREERDP_UTILS_RDPDR_H
#define FREERDP_UTILS_RDPDR_H

#include <winpr/stream.h>
#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct
	{
		UINT32 DeviceType;
		UINT32 DeviceId;
		char PreferredDosName[8];
		UINT32 DeviceDataLength;
		BYTE* DeviceData;
	} RdpdrDevice;

	typedef struct
	{
		UINT16 CapabilityType;
		UINT16 CapabilityLength;
		UINT32 Version;
	} RDPDR_CAPABILITY_HEADER;

	FREERDP_API const char* rdpdr_component_string(UINT16 component);
	FREERDP_API const char* rdpdr_packetid_string(UINT16 packetid);
	FREERDP_API const char* rdpdr_irp_string(UINT32 major);
	FREERDP_API const char* rdpdr_cap_type_string(UINT16 capability);

	FREERDP_API LONG scard_log_status_error(const char* tag, const char* what, LONG status);
	FREERDP_API const char* scard_get_ioctl_string(UINT32 ioControlCode, BOOL funcName);

	FREERDP_API BOOL rdpdr_write_iocompletion_header(wStream* out, UINT32 DeviceId,
	                                                 UINT32 CompletionId, UINT32 ioStatus);

	FREERDP_API void rdpdr_dump_received_packet(wLog* log, DWORD lvl, wStream* out,
	                                            const char* custom);
	FREERDP_API void rdpdr_dump_send_packet(wLog* log, DWORD lvl, wStream* out, const char* custom);

	FREERDP_API UINT rdpdr_read_capset_header(wLog* log, wStream* s,
	                                          RDPDR_CAPABILITY_HEADER* header);
	FREERDP_API UINT rdpdr_write_capset_header(wLog* log, wStream* s,
	                                           const RDPDR_CAPABILITY_HEADER* header);

#ifdef __cplusplus
}
#endif

#endif
