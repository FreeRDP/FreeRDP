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
#include <freerdp/channels/ainput.h>

typedef enum AINPUT_SERVER_OPEN_RESULT
{
	AINPUT_SERVER_OPEN_RESULT_OK = 0,
	AINPUT_SERVER_OPEN_RESULT_CLOSED = 1,
	AINPUT_SERVER_OPEN_RESULT_NOTSUPPORTED = 2,
	AINPUT_SERVER_OPEN_RESULT_ERROR = 3
} AINPUT_SERVER_OPEN_RESULT;

typedef struct _ainput_server_context ainput_server_context;

typedef BOOL (*psAInputChannelIdAssigned)(ainput_server_context* context, UINT32 channelId);

typedef UINT (*psAInputServerInitialize)(ainput_server_context* context, BOOL externalThread);
typedef UINT (*psAInputServerPoll)(ainput_server_context* context);
typedef BOOL (*psAInputServerChannelHandle)(ainput_server_context* context, HANDLE* handle);

typedef UINT (*psAInputServerOpen)(ainput_server_context* context);
typedef UINT (*psAInputServerClose)(ainput_server_context* context);
typedef BOOL (*psAInputServerIsOpen)(ainput_server_context* context);

typedef UINT (*psAInputServerOpenResult)(ainput_server_context* context,
                                         AINPUT_SERVER_OPEN_RESULT result);
typedef UINT (*psAInputServerMouseEvent)(ainput_server_context* context, UINT64 timestamp,
                                         UINT64 flags, INT32 x, INT32 y);

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
	 * Optional: Set thread handling.
	 * When externalThread=TRUE the application is responsible to call
	 * ainput_server_context_poll periodically to process input events.
	 *
	 * Defaults to externalThread=FALSE
	 */
	psAInputServerInitialize Initialize;

	/**
	 * @brief Poll When externalThread=TRUE call periodically from your main loop.
	 * if externalThread=FALSE do not call.
	 */
	psAInputServerPoll Poll;

	/**
	 * @brief Poll When externalThread=TRUE call to get a handle to wait for events.
	 * Will return FALSE until the handle is available.
	 */
	psAInputServerChannelHandle ChannelHandle;

	/**
	 * Close the ainput channel.
	 */
	psAInputServerClose Close;
	/**
	 * Status of the ainput channel.
	 */
	psAInputServerIsOpen IsOpen;

	/*** Callbacks registered by the server. ***/

	/**
	 * Receive ainput mouse event PDU.
	 */
	psAInputServerMouseEvent MouseEvent;

	rdpContext* rdpcontext;

	/**
	 * Callback, when the channel got its id assigned.
	 */
	psAInputChannelIdAssigned ChannelIdAssigned;
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
