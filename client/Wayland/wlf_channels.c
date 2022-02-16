/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Channels
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

#include <freerdp/config.h>

#include <freerdp/gdi/gfx.h>

#include <freerdp/gdi/video.h>

#include "wlf_channels.h"
#include "wlf_cliprdr.h"
#include "wlf_disp.h"
#include "wlfreerdp.h"

static BOOL encomsp_toggle_control(EncomspClientContext* encomsp, BOOL control)
{
	ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU pdu;

	if (!encomsp)
		return FALSE;

	pdu.ParticipantId = 0;
	pdu.Flags = ENCOMSP_REQUEST_VIEW;

	if (control)
		pdu.Flags |= ENCOMSP_REQUEST_INTERACT;

	encomsp->ChangeParticipantControlLevel(encomsp, &pdu);
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
wlf_encomsp_participant_created(EncomspClientContext* context,
                                const ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated)
{
	wlfContext* wlf;
	rdpSettings* settings;
	BOOL request;

	if (!context || !context->custom || !participantCreated)
		return ERROR_INVALID_PARAMETER;

	wlf = (wlfContext*)context->custom;
	WINPR_ASSERT(wlf);

	settings = wlf->common.context.settings;

	if (!settings)
		return ERROR_INVALID_PARAMETER;

	request = freerdp_settings_get_bool(settings, FreeRDP_RemoteAssistanceRequestControl);
	if (request && (participantCreated->Flags & ENCOMSP_MAY_VIEW) &&
	    !(participantCreated->Flags & ENCOMSP_MAY_INTERACT))
	{
		if (!encomsp_toggle_control(context, TRUE))
			return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

static void wlf_encomsp_init(wlfContext* wlf, EncomspClientContext* encomsp)
{
	wlf->encomsp = encomsp;
	encomsp->custom = (void*)wlf;
	encomsp->ParticipantCreated = wlf_encomsp_participant_created;
}

static void wlf_encomsp_uninit(wlfContext* wlf, EncomspClientContext* encomsp)
{
	if (encomsp)
	{
		encomsp->custom = NULL;
		encomsp->ParticipantCreated = NULL;
	}

	if (wlf)
		wlf->encomsp = NULL;
}

void wlf_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e)
{
	wlfContext* wlf = (wlfContext*)context;

	WINPR_ASSERT(wlf);
	WINPR_ASSERT(e);

	if (FALSE)
	{
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		wlf_cliprdr_init(wlf->clipboard, (CliprdrClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		wlf_encomsp_init(wlf, (EncomspClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		wlf_disp_init(wlf->disp, (DispClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelConnectedEventHandler(context, e);
}

void wlf_OnChannelDisconnectedEventHandler(void* context, const ChannelDisconnectedEventArgs* e)
{
	wlfContext* wlf = (wlfContext*)context;

	WINPR_ASSERT(wlf);
	WINPR_ASSERT(e);

	if (FALSE)
	{
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		wlf_cliprdr_uninit(wlf->clipboard, (CliprdrClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		wlf_encomsp_uninit(wlf, (EncomspClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		wlf_disp_uninit(wlf->disp, (DispClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}
