/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * AInput Virtual Channel Extension
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_AINPUT_SERVER_H
#define FREERDP_CHANNEL_AINPUT_SERVER_H

#include <freerdp/channels/wtsvc.h>

typedef enum AINPUT_SERVER_OPEN_RESULT
{
	AINPUT_SERVER_OPEN_RESULT_OK = 0,
	AINPUT_SERVER_OPEN_RESULT_CLOSED = 1,
	AINPUT_SERVER_OPEN_RESULT_NOTSUPPORTED = 2,
	AINPUT_SERVER_OPEN_RESULT_ERROR = 3
} AINPUT_SERVER_OPEN_RESULT;

typedef struct _ainput_server_context ainput_server_context;

typedef UINT (*psAInputServerOpen)(ainput_server_context* context);
typedef UINT (*psAInputServerClose)(ainput_server_context* context);

typedef UINT (*psAInputServerOpenResult)(ainput_server_context* context,
                                         AINPUT_SERVER_OPEN_RESULT result);
typedef UINT (*psAInputServerReceive)(ainput_server_context* context, const BYTE* buffer,
                                      UINT32 length);

struct _ainput_server_context
{
	HANDLE vcm;

	/* Server self-defined pointer. */
	void* data;

	/*** APIs called by the server. ***/
	/**
	 * Open the ainput channel.
	 */
	psAInputServerOpen Open;
	/**
	 * Close the ainput channel.
	 */
	psAInputServerClose Close;

	/*** Callbacks registered by the server. ***/
	/**
	 * Indicate whether the channel is opened successfully.
	 */
	psAInputServerOpenResult OpenResult;
	/**
	 * Receive ainput response PDU.
	 */
	psAInputServerReceive Receive;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API ainput_server_context* ainput_server_context_new(HANDLE vcm);
	FREERDP_API void ainput_server_context_free(ainput_server_context* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_AINPUT_SERVER_H */
