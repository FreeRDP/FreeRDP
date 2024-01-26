/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Location Virtual Channel Extension
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

#ifndef FREERDP_CHANNEL_LOCATION_SERVER_LOCATION_H
#define FREERDP_CHANNEL_LOCATION_SERVER_LOCATION_H

#include <freerdp/channels/location.h>
#include <freerdp/channels/wtsvc.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_location_server_context LocationServerContext;

	typedef UINT (*psLocationServerOpen)(LocationServerContext* context);
	typedef UINT (*psLocationServerClose)(LocationServerContext* context);

	typedef BOOL (*psLocationServerChannelIdAssigned)(LocationServerContext* context,
	                                                  UINT32 channelId);

	typedef UINT (*psLocationServerInitialize)(LocationServerContext* context, BOOL externalThread);
	typedef UINT (*psLocationServerPoll)(LocationServerContext* context);
	typedef BOOL (*psLocationServerChannelHandle)(LocationServerContext* context, HANDLE* handle);

	typedef UINT (*psLocationServerServerReady)(LocationServerContext* context,
	                                            const RDPLOCATION_SERVER_READY_PDU* serverReady);
	typedef UINT (*psLocationServerClientReady)(LocationServerContext* context,
	                                            const RDPLOCATION_CLIENT_READY_PDU* clientReady);

	typedef UINT (*psLocationServerBaseLocation3D)(
	    LocationServerContext* context, const RDPLOCATION_BASE_LOCATION3D_PDU* baseLocation3D);
	typedef UINT (*psLocationServerLocation2DDelta)(
	    LocationServerContext* context, const RDPLOCATION_LOCATION2D_DELTA_PDU* location2DDelta);
	typedef UINT (*psLocationServerLocation3DDelta)(
	    LocationServerContext* context, const RDPLOCATION_LOCATION3D_DELTA_PDU* location3DDelta);

	struct s_location_server_context
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
		psLocationServerInitialize Initialize;

		/**
		 * Open the location channel.
		 */
		psLocationServerOpen Open;

		/**
		 * Close the location channel.
		 */
		psLocationServerClose Close;

		/**
		 * Poll
		 * When externalThread=TRUE, call Poll() periodically from your main loop.
		 * If externalThread=FALSE do not call.
		 */
		psLocationServerPoll Poll;

		/**
		 * Retrieve the channel handle for use in conjunction with Poll().
		 * If externalThread=FALSE do not call.
		 */
		psLocationServerChannelHandle ChannelHandle;

		/* All PDUs sent by the server don't require the header to be set */

		/*
		 * Send a ServerReady PDU.
		 */
		psLocationServerServerReady ServerReady;

		/*** Callbacks registered by the server. ***/

		/**
		 * Callback, when the channel got its id assigned.
		 */
		psLocationServerChannelIdAssigned ChannelIdAssigned;

		/**
		 * Callback for the ClientReady PDU.
		 */
		psLocationServerClientReady ClientReady;

		/**
		 * Callback for the BaseLocation3D PDU.
		 */
		psLocationServerBaseLocation3D BaseLocation3D;

		/**
		 * Callback for the Location2DDelta PDU.
		 */
		psLocationServerLocation2DDelta Location2DDelta;

		/**
		 * Callback for the Location3DDelta PDU.
		 */
		psLocationServerLocation3DDelta Location3DDelta;

		rdpContext* rdpcontext;
	};

	FREERDP_API void location_server_context_free(LocationServerContext* context);

	WINPR_ATTR_MALLOC(location_server_context_free, 1)
	FREERDP_API LocationServerContext* location_server_context_new(HANDLE vcm);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_LOCATION_SERVER_LOCATION_H */
