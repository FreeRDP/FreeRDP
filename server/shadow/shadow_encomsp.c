/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "shadow.h"

#include "shadow_encomsp.h"

static int encomsp_change_participant_control_level(EncomspServerContext* context,
		ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU* pdu)
{
	BOOL inLobby;
	BOOL mayView;
	BOOL mayInteract;
	rdpShadowClient* client = (rdpShadowClient*) context->custom;

	printf("ChangeParticipantControlLevel: ParticipantId: %d Flags: 0x%04X\n",
			pdu->ParticipantId, pdu->Flags);

	mayView = (pdu->Flags & ENCOMSP_MAY_VIEW) ? TRUE : FALSE;
	mayInteract = (pdu->Flags & ENCOMSP_MAY_INTERACT) ? TRUE : FALSE;

	if (mayInteract && !mayView)
		mayView = TRUE; /* may interact implies may view */

	if (mayInteract)
	{
		if (!client->mayInteract)
		{
			/* request interact + view */
			client->mayInteract = TRUE;
			client->mayView = TRUE;
		}
	}
	else if (mayView)
	{
		if (client->mayInteract)
		{
			/* release interact */
			client->mayInteract = FALSE;
		}
		else if (!client->mayView)
		{
			/* request view */
			client->mayView = TRUE;
		}
	}
	else
	{
		if (client->mayInteract)
		{
			/* release interact + view */
			client->mayView = FALSE;
			client->mayInteract = FALSE;
		}
		else if (client->mayView)
		{
			/* release view */
			client->mayView = FALSE;
			client->mayInteract = FALSE;
		}
	}

	inLobby = client->mayView ? FALSE : TRUE;

	if (inLobby != client->inLobby)
	{
		shadow_encoder_reset(client->encoder);
		client->inLobby = inLobby;
	}

	return 1;
}

int shadow_client_encomsp_init(rdpShadowClient* client)
{
	EncomspServerContext* encomsp;

	encomsp = client->encomsp = encomsp_server_context_new(client->vcm);

	encomsp->custom = (void*) client;

	encomsp->ChangeParticipantControlLevel = encomsp_change_participant_control_level;

	if (client->encomsp)
		client->encomsp->Start(client->encomsp);

	return 1;
}

