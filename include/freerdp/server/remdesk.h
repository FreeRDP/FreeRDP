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

#ifndef FREERDP_CHANNEL_SERVER_REMDESK_H
#define FREERDP_CHANNEL_SERVER_REMDESK_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>

#include <freerdp/client/remdesk.h>

/**
 * Server Interface
 */

typedef struct _remdesk_server_context RemdeskServerContext;
typedef struct _remdesk_server_private RemdeskServerPrivate;

typedef int (*psRemdeskStart)(RemdeskServerContext* context);
typedef int (*psRemdeskStop)(RemdeskServerContext* context);

struct _remdesk_server_context
{
	HANDLE vcm;

	psRemdeskStart Start;
	psRemdeskStop Stop;

	RemdeskServerPrivate* priv;
};

FREERDP_API RemdeskServerContext* remdesk_server_context_new(HANDLE vcm);
FREERDP_API void remdesk_server_context_free(RemdeskServerContext* context);

#endif /* FREERDP_CHANNEL_SERVER_REMDESK_H */

