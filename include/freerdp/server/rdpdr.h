/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Server Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_SERVER_RDPDR_H
#define FREERDP_CHANNEL_SERVER_RDPDR_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/rdpdr.h>

/**
 * Server Interface
 */

typedef struct _rdpdr_server_context RdpdrServerContext;
typedef struct _rdpdr_server_private RdpdrServerPrivate;

typedef int (*psRdpdrStart)(RdpdrServerContext* context);
typedef int (*psRdpdrStop)(RdpdrServerContext* context);

struct _rdpdr_server_context
{
	WTSVirtualChannelManager* vcm;

	psRdpdrStart Start;
	psRdpdrStop Stop;

	RdpdrServerPrivate* priv;
};

FREERDP_API RdpdrServerContext* rdpdr_server_context_new(WTSVirtualChannelManager* vcm);
FREERDP_API void rdpdr_server_context_free(RdpdrServerContext* context);

#endif /* FREERDP_CHANNEL_SERVER_RDPDR_H */
