/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Echo Virtual Channel Extension
 *
 * Copyright 2014 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_ECHO_SERVER_H
#define FREERDP_CHANNEL_ECHO_SERVER_H

#include <freerdp/channels/wtsvc.h>

typedef enum ECHO_SERVER_OPEN_RESULT
{
	ECHO_SERVER_OPEN_RESULT_OK = 0,
	ECHO_SERVER_OPEN_RESULT_CLOSED = 1,
	ECHO_SERVER_OPEN_RESULT_NOTSUPPORTED = 2,
	ECHO_SERVER_OPEN_RESULT_ERROR = 3
} ECHO_SERVER_OPEN_RESULT;

typedef struct _echo_server_context echo_server_context;

typedef UINT (*psEchoServerOpen)(echo_server_context* context);
typedef UINT (*psEchoServerClose)(echo_server_context* context);
typedef BOOL (*psEchoServerRequest)(echo_server_context* context, const BYTE* buffer,
                                    UINT32 length);

typedef UINT (*psEchoServerOpenResult)(echo_server_context* context,
                                       ECHO_SERVER_OPEN_RESULT result);
typedef UINT (*psEchoServerResponse)(echo_server_context* context, const BYTE* buffer,
                                     UINT32 length);

struct _echo_server_context
{
	HANDLE vcm;

	/* Server self-defined pointer. */
	void* data;

	/*** APIs called by the server. ***/
	/**
	 * Open the echo channel.
	 */
	psEchoServerOpen Open;
	/**
	 * Close the echo channel.
	 */
	psEchoServerClose Close;
	/**
	 * Send echo request PDU.
	 */
	psEchoServerRequest Request;

	/*** Callbacks registered by the server. ***/
	/**
	 * Indicate whether the channel is opened successfully.
	 */
	psEchoServerOpenResult OpenResult;
	/**
	 * Receive echo response PDU.
	 */
	psEchoServerResponse Response;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API echo_server_context* echo_server_context_new(HANDLE vcm);
	FREERDP_API void echo_server_context_free(echo_server_context* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_ECHO_SERVER_H */
