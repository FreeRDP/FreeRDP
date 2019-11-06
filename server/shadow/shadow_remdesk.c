/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "shadow.h"

#include "shadow_remdesk.h"

int shadow_client_remdesk_init(rdpShadowClient* client)
{
	RemdeskServerContext* remdesk;

	remdesk = client->remdesk = remdesk_server_context_new(client->vcm);
	remdesk->rdpcontext = &client->context;

	remdesk->custom = (void*)client;

	if (client->remdesk)
		client->remdesk->Start(client->remdesk);

	return 1;
}

void shadow_client_remdesk_uninit(rdpShadowClient* client)
{
	if (client->remdesk)
	{
		client->remdesk->Stop(client->remdesk);
		remdesk_server_context_free(client->remdesk);
		client->remdesk = NULL;
	}
}
