/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Mouse Cursor Virtual Channel Extension
 *
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#ifndef FREERDP_CHANNEL_RDPEMSC_SERVER_RDPEMSC_H
#define FREERDP_CHANNEL_RDPEMSC_SERVER_RDPEMSC_H

#include <freerdp/channels/rdpemsc.h>
#include <freerdp/channels/wtsvc.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_mouse_cursor_server_context MouseCursorServerContext;

	typedef UINT (*psMouseCursorServerOpen)(MouseCursorServerContext* context);
	typedef UINT (*psMouseCursorServerClose)(MouseCursorServerContext* context);

	typedef BOOL (*psMouseCursorServerChannelIdAssigned)(MouseCursorServerContext* context,
	                                                     UINT32 channelId);

	typedef UINT (*psMouseCursorServerInitialize)(MouseCursorServerContext* context,
	                                              BOOL externalThread);
	typedef UINT (*psMouseCursorServerPoll)(MouseCursorServerContext* context);
	typedef BOOL (*psMouseCursorServerChannelHandle)(MouseCursorServerContext* context,
	                                                 HANDLE* handle);

	typedef UINT (*psMouseCursorServerCapsAdvertise)(
	    MouseCursorServerContext* context,
	    const RDP_MOUSE_CURSOR_CAPS_ADVERTISE_PDU* capsAdvertise);
	typedef UINT (*psMouseCursorServerCapsConfirm)(
	    MouseCursorServerContext* context, const RDP_MOUSE_CURSOR_CAPS_CONFIRM_PDU* capsConfirm);

	typedef UINT (*psMouseCursorServerMouseptrUpdate)(
	    MouseCursorServerContext* context,
	    const RDP_MOUSE_CURSOR_MOUSEPTR_UPDATE_PDU* mouseptrUpdate);

	struct s_mouse_cursor_server_context
	{
		HANDLE vcm;

		/* Server self-defined pointer. */
		void* userdata;

		/*** APIs called by the server. ***/

		/**
		 * Optional: Set thread handling.
		 * When externalThread=TRUE, the application is responsible to call
		 * Poll() periodically to process channel events.
		 *
		 * Defaults to externalThread=FALSE
		 */
		psMouseCursorServerInitialize Initialize;

		/**
		 * Open the mouse cursor channel.
		 */
		psMouseCursorServerOpen Open;

		/**
		 * Close the mouse cursor channel.
		 */
		psMouseCursorServerClose Close;

		/**
		 * Poll
		 * When externalThread=TRUE, call Poll() periodically from your main loop.
		 * If externalThread=FALSE do not call.
		 */
		psMouseCursorServerPoll Poll;

		/**
		 * Retrieve the channel handle for use in conjunction with Poll().
		 * If externalThread=FALSE do not call.
		 */
		psMouseCursorServerChannelHandle ChannelHandle;

		/* All PDUs sent by the server don't require the pduType to be set */

		/*
		 * Send a CapsConfirm PDU.
		 */
		psMouseCursorServerCapsConfirm CapsConfirm;

		/*
		 * Send a MouseptrUpdate PDU.
		 */
		psMouseCursorServerMouseptrUpdate MouseptrUpdate;

		/*** Callbacks registered by the server. ***/

		/**
		 * Callback, when the channel got its id assigned.
		 */
		psMouseCursorServerChannelIdAssigned ChannelIdAssigned;

		/**
		 * Callback for the CapsAdvertise PDU.
		 */
		psMouseCursorServerCapsAdvertise CapsAdvertise;

		rdpContext* rdpcontext;
	};

	FREERDP_API void mouse_cursor_server_context_free(MouseCursorServerContext* context);

	WINPR_ATTR_MALLOC(mouse_cursor_server_context_free, 1)
	FREERDP_API MouseCursorServerContext* mouse_cursor_server_context_new(HANDLE vcm);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPEMSC_SERVER_RDPEMSC_H */
