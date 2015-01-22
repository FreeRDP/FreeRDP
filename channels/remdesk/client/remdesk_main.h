/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Assistance Virtual Channel
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_REMDESK_CLIENT_MAIN_H
#define FREERDP_CHANNEL_REMDESK_CLIENT_MAIN_H

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/api.h>
#include <freerdp/svc.h>
#include <freerdp/addin.h>
#include <freerdp/settings.h>

#include <freerdp/client/remdesk.h>

#include <freerdp/channels/log.h>
#define TAG CHANNELS_TAG("remdesk.client")

struct remdesk_plugin
{
	CHANNEL_DEF channelDef;
	CHANNEL_ENTRY_POINTS_FREERDP channelEntryPoints;

	RemdeskClientContext* context;

	HANDLE thread;
	wStream* data_in;
	void* InitHandle;
	DWORD OpenHandle;
	rdpSettings* settings;
	wMessageQueue* queue;

	UINT32 Version;
	char* ExpertBlob;
	BYTE* EncryptedPassStub;
	int EncryptedPassStubSize;
};
typedef struct remdesk_plugin remdeskPlugin;

#endif /* FREERDP_CHANNEL_REMDESK_CLIENT_MAIN_H */
