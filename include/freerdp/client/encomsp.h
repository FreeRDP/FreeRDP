/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
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

#ifndef FREERDP_CHANNEL_CLIENT_ENCOMSP_H
#define FREERDP_CHANNEL_CLIENT_ENCOMSP_H

#include <freerdp/channels/encomsp.h>

/**
 * Client Interface
 */

typedef struct _encomsp_client_context EncomspClientContext;

typedef int (*pcEncomspFilterUpdated)(EncomspClientContext* context, ENCOMSP_FILTER_UPDATED_PDU* filterUpdated);
typedef int (*pcEncomspApplicationCreated)(EncomspClientContext* context, ENCOMSP_APPLICATION_CREATED_PDU* applicationCreated);
typedef int (*pcEncomspApplicationRemoved)(EncomspClientContext* context, ENCOMSP_APPLICATION_REMOVED_PDU* applicationRemoved);
typedef int (*pcEncomspWindowCreated)(EncomspClientContext* context, ENCOMSP_WINDOW_CREATED_PDU* windowCreated);
typedef int (*pcEncomspWindowRemoved)(EncomspClientContext* context, ENCOMSP_WINDOW_REMOVED_PDU* windowRemoved);
typedef int (*pcEncomspShowWindow)(EncomspClientContext* context, ENCOMSP_SHOW_WINDOW_PDU* showWindow);
typedef int (*pcEncomspParticipantCreated)(EncomspClientContext* context, ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated);
typedef int (*pcEncomspParticipantRemoved)(EncomspClientContext* context, ENCOMSP_PARTICIPANT_REMOVED_PDU* participantRemoved);
typedef int (*pcEncomspChangeParticipantControlLevel)(EncomspClientContext* context, ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU* changeParticipantControlLevel);
typedef int (*pcEncomspGraphicsStreamPaused)(EncomspClientContext* context, ENCOMSP_GRAPHICS_STREAM_PAUSED_PDU* graphicsStreamPaused);
typedef int (*pcEncomspGraphicsStreamResumed)(EncomspClientContext* context, ENCOMSP_GRAPHICS_STREAM_RESUMED_PDU* graphicsStreamResumed);

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
