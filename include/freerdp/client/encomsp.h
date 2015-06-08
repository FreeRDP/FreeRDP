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

#ifndef FREERDP_CHANNEL_CLIENT_ENCOMSP_H
#define FREERDP_CHANNEL_CLIENT_ENCOMSP_H

#include <freerdp/channels/encomsp.h>
#include <winpr/win32error.h>

/**
 * Client Interface
 */

typedef struct _encomsp_client_context EncomspClientContext;

typedef WIN32ERROR (*pcEncomspFilterUpdated)(EncomspClientContext* context, ENCOMSP_FILTER_UPDATED_PDU* filterUpdated);
typedef WIN32ERROR (*pcEncomspApplicationCreated)(EncomspClientContext* context, ENCOMSP_APPLICATION_CREATED_PDU* applicationCreated);
typedef WIN32ERROR (*pcEncomspApplicationRemoved)(EncomspClientContext* context, ENCOMSP_APPLICATION_REMOVED_PDU* applicationRemoved);
typedef WIN32ERROR (*pcEncomspWindowCreated)(EncomspClientContext* context, ENCOMSP_WINDOW_CREATED_PDU* windowCreated);
typedef WIN32ERROR (*pcEncomspWindowRemoved)(EncomspClientContext* context, ENCOMSP_WINDOW_REMOVED_PDU* windowRemoved);
typedef WIN32ERROR (*pcEncomspShowWindow)(EncomspClientContext* context, ENCOMSP_SHOW_WINDOW_PDU* showWindow);
typedef WIN32ERROR (*pcEncomspParticipantCreated)(EncomspClientContext* context, ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated);
typedef WIN32ERROR (*pcEncomspParticipantRemoved)(EncomspClientContext* context, ENCOMSP_PARTICIPANT_REMOVED_PDU* participantRemoved);
typedef WIN32ERROR (*pcEncomspChangeParticipantControlLevel)(EncomspClientContext* context, ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU* changeParticipantControlLevel);
typedef WIN32ERROR (*pcEncomspGraphicsStreamPaused)(EncomspClientContext* context, ENCOMSP_GRAPHICS_STREAM_PAUSED_PDU* graphicsStreamPaused);
typedef WIN32ERROR (*pcEncomspGraphicsStreamResumed)(EncomspClientContext* context, ENCOMSP_GRAPHICS_STREAM_RESUMED_PDU* graphicsStreamResumed);

struct _encomsp_client_context
{
	void* handle;
	void* custom;

	pcEncomspFilterUpdated FilterUpdated;
	pcEncomspApplicationCreated ApplicationCreated;
	pcEncomspApplicationRemoved ApplicationRemoved;
	pcEncomspWindowCreated WindowCreated;
	pcEncomspWindowRemoved WindowRemoved;
	pcEncomspShowWindow ShowWindow;
	pcEncomspParticipantCreated ParticipantCreated;
	pcEncomspParticipantRemoved ParticipantRemoved;
	pcEncomspChangeParticipantControlLevel ChangeParticipantControlLevel;
	pcEncomspGraphicsStreamPaused GraphicsStreamPaused;
	pcEncomspGraphicsStreamResumed GraphicsStreamResumed;
};

#endif /* FREERDP_CHANNEL_CLIENT_ENCOMSP_H */
