/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Extension
 *
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

#ifndef FREERDP_CHANNEL_CLIENT_RDPEI_H
#define FREERDP_CHANNEL_CLIENT_RDPEI_H

#include <freerdp/channels/rdpei.h>
#include <winpr/win32error.h>

/**
 * Client Interface
 */

typedef struct _rdpei_client_context RdpeiClientContext;

typedef int (*pcRdpeiGetVersion)(RdpeiClientContext* context);
typedef WIN32ERROR (*pcRdpeiAddContact)(RdpeiClientContext* context, RDPINPUT_CONTACT_DATA* contact);

typedef WIN32ERROR (*pcRdpeiTouchBegin)(RdpeiClientContext* context, int externalId, int x, int y, int* contactId);
typedef WIN32ERROR (*pcRdpeiTouchUpdate)(RdpeiClientContext* context, int externalId, int x, int y, int* contactId);
typedef WIN32ERROR (*pcRdpeiTouchEnd)(RdpeiClientContext* context, int externalId, int x, int y, int* contactId);

typedef WIN32ERROR (*pcRdpeiSuspendTouch)(RdpeiClientContext* context);
typedef WIN32ERROR (*pcRdpeiResumeTouch)(RdpeiClientContext* context);

struct _rdpei_client_context
{
	void* handle;
	void* custom;

	pcRdpeiGetVersion GetVersion;

	pcRdpeiAddContact AddContact;

	pcRdpeiTouchBegin TouchBegin;
	pcRdpeiTouchUpdate TouchUpdate;
	pcRdpeiTouchEnd TouchEnd;

	pcRdpeiSuspendTouch SuspendTouch;
	pcRdpeiResumeTouch ResumeTouch;
};

#endif /* FREERDP_CHANNEL_CLIENT_RDPEI_H */
