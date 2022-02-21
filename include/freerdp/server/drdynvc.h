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

#ifndef FREERDP_CHANNEL_DRDYNVC_SERVER_DRDYNVC_H
#define FREERDP_CHANNEL_DRDYNVC_SERVER_DRDYNVC_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>

/**
 * Server Interface
 */

typedef struct s_drdynvc_server_context DrdynvcServerContext;
typedef struct s_drdynvc_server_private DrdynvcServerPrivate;

typedef UINT (*psDrdynvcStart)(DrdynvcServerContext* context);
typedef UINT (*psDrdynvcStop)(DrdynvcServerContext* context);

struct s_drdynvc_server_context
{
	HANDLE vcm;

	psDrdynvcStart Start;
	psDrdynvcStop Stop;

	DrdynvcServerPrivate* priv;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API DrdynvcServerContext* drdynvc_server_context_new(HANDLE vcm);
	FREERDP_API void drdynvc_server_context_free(DrdynvcServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_DRDYNVC_SERVER_DRDYNVC_H */
