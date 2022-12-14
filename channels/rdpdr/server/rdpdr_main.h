/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Extension
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_RDPDR_SERVER_MAIN_H
#define FREERDP_CHANNEL_RDPDR_SERVER_MAIN_H

#include <winpr/collections.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/settings.h>
#include <freerdp/server/rdpdr.h>

typedef struct S_RDPDR_IRP
{
	UINT32 CompletionId;
	UINT32 DeviceId;
	UINT32 FileId;
	char PathName[256];
	char ExtraBuffer[256];
	void* CallbackData;
	UINT(*Callback)
	(RdpdrServerContext* context, wStream* s, struct S_RDPDR_IRP* irp, UINT32 deviceId,
	 UINT32 completionId, UINT32 ioStatus);
} RDPDR_IRP;

#endif /* FREERDP_CHANNEL_RDPDR_SERVER_MAIN_H */
