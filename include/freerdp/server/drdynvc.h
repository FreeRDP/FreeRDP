/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Extension
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

#ifndef FREERDP_CHANNEL_SERVER_DRDYNVC_H
#define FREERDP_CHANNEL_SERVER_DRDYNVC_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>

/**
 * Server Interface
 */

typedef struct _drdynvc_client_context DrdynvcServerContext;
typedef struct _drdynvc_server_private DrdynvcServerPrivate;

typedef int (*psDrdynvcStart)(DrdynvcServerContext* context);
typedef int (*psDrdynvcStop)(DrdynvcServerContext* context);

struct _drdynvc_client_context
{
	WTSVirtualChannelManager* vcm;

	psDrdynvcStart Start;
	psDrdynvcStop Stop;

	DrdynvcServerPrivate* priv;
};

FREERDP_API DrdynvcServerContext* drdynvc_server_context_new(WTSVirtualChannelManager* vcm);
FREERDP_API void drdynvc_server_context_free(DrdynvcServerContext* context);

#endif /* FREERDP_CHANNEL_SERVER_DRDYNVC_H */
