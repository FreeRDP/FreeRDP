/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
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

#ifndef FREERDP_CHANNEL_ENCOMSP_SERVER_ENCOMSP_H
#define FREERDP_CHANNEL_ENCOMSP_SERVER_ENCOMSP_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>

#include <freerdp/channels/encomsp.h>

/**
 * Server Interface
 */

typedef struct s_encomsp_server_context EncomspServerContext;
typedef struct s_encomsp_server_private EncomspServerPrivate;

typedef UINT (*psEncomspStart)(EncomspServerContext* context);
typedef UINT (*psEncomspStop)(EncomspServerContext* context);

typedef UINT (*psEncomspFilterUpdated)(EncomspServerContext* context,
                                       ENCOMSP_FILTER_UPDATED_PDU* filterUpdated);
typedef UINT (*psEncomspApplicationCreated)(EncomspServerContext* context,
                                            ENCOMSP_APPLICATION_CREATED_PDU* applicationCreated);
typedef UINT (*psEncomspApplicationRemoved)(EncomspServerContext* context,
                                            ENCOMSP_APPLICATION_REMOVED_PDU* applicationRemoved);
typedef UINT (*psEncomspWindowCreated)(EncomspServerContext* context,
                                       ENCOMSP_WINDOW_CREATED_PDU* windowCreated);
typedef UINT (*psEncomspWindowRemoved)(EncomspServerContext* context,
                                       ENCOMSP_WINDOW_REMOVED_PDU* windowRemoved);
typedef UINT (*psEncomspShowWindow)(EncomspServerContext* context,
                                    ENCOMSP_SHOW_WINDOW_PDU* showWindow);
typedef UINT (*psEncomspParticipantCreated)(EncomspServerContext* context,
                                            ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated);
typedef UINT (*psEncomspParticipantRemoved)(EncomspServerContext* context,
                                            ENCOMSP_PARTICIPANT_REMOVED_PDU* participantRemoved);
typedef UINT (*psEncomspChangeParticipantControlLevel)(
    EncomspServerContext* context,
    ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU* changeParticipantControlLevel);
typedef UINT (*psEncomspGraphicsStreamPaused)(
    EncomspServerContext* context, ENCOMSP_GRAPHICS_STREAM_PAUSED_PDU* graphicsStreamPaused);
typedef UINT (*psEncomspGraphicsStreamResumed)(
    EncomspServerContext* context, ENCOMSP_GRAPHICS_STREAM_RESUMED_PDU* graphicsStreamResumed);

struct s_encomsp_server_context
{
	HANDLE vcm;
	void* custom;

	psEncomspStart Start;
	psEncomspStop Stop;

	psEncomspFilterUpdated FilterUpdated;
	psEncomspApplicationCreated ApplicationCreated;
	psEncomspApplicationRemoved ApplicationRemoved;
	psEncomspWindowCreated WindowCreated;
	psEncomspWindowRemoved WindowRemoved;
	psEncomspShowWindow ShowWindow;
	psEncomspParticipantCreated ParticipantCreated;
	psEncomspParticipantRemoved ParticipantRemoved;
	psEncomspChangeParticipantControlLevel ChangeParticipantControlLevel;
	psEncomspGraphicsStreamPaused GraphicsStreamPaused;
	psEncomspGraphicsStreamResumed GraphicsStreamResumed;

	EncomspServerPrivate* priv;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API EncomspServerContext* encomsp_server_context_new(HANDLE vcm);
	FREERDP_API void encomsp_server_context_free(EncomspServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_ENCOMSP_SERVER_ENCOMSP_H */
