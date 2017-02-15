/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2016 Jiang Zihao <zihao.jiang@yahoo.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/log.h>
#include "shadow.h"

#include "shadow_rdpgfx.h"

#define TAG SERVER_TAG("shadow")

int shadow_client_rdpgfx_init(rdpShadowClient* client)
{
	RdpgfxServerContext* rdpgfx;
	rdpgfx = client->rdpgfx = rdpgfx_server_context_new(client->vcm);
	if (!rdpgfx)
	{
		return 0;
	}

	rdpgfx->rdpcontext = &client->context;

	rdpgfx->custom = client;

	return 1;
}

void shadow_client_rdpgfx_uninit(rdpShadowClient* client)
{
	if (client->rdpgfx)
	{
		rdpgfx_server_context_free(client->rdpgfx);
		client->rdpgfx = NULL;
	}
}
