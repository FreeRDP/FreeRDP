/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Telemetry Virtual Channel Extension
 *
 * Copyright 2022 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#ifndef FREERDP_CHANNEL_TELEMETRY_SERVER_TELEMETRY_H
#define FREERDP_CHANNEL_TELEMETRY_SERVER_TELEMETRY_H

#include <freerdp/channels/telemetry.h>
#include <freerdp/channels/wtsvc.h>

typedef struct _telemetry_server_context TelemetryServerContext;

typedef UINT (*psTelemetryServerOpen)(TelemetryServerContext* context);
typedef UINT (*psTelemetryServerClose)(TelemetryServerContext* context);

typedef BOOL (*psTelemetryServerChannelIdAssigned)(TelemetryServerContext* context,
                                                   UINT32 channelId);

typedef UINT (*psTelemetryServerInitialize)(TelemetryServerContext* context, BOOL externalThread);
typedef UINT (*psTelemetryServerPoll)(TelemetryServerContext* context);
typedef BOOL (*psTelemetryServerChannelHandle)(TelemetryServerContext* context, HANDLE* handle);

typedef UINT (*psTelemetryServerRdpTelemetry)(TelemetryServerContext* context,
                                              const TELEMETRY_RDP_TELEMETRY_PDU* rdpTelemetry);

struct _telemetry_server_context
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
	psTelemetryServerInitialize Initialize;

	/**
	 * Open the telemetry channel.
	 */
	psTelemetryServerOpen Open;

	/**
	 * Close the telemetry channel.
	 */
	psTelemetryServerClose Close;

	/**
	 * Poll
	 * When externalThread=TRUE, call Poll() periodically from your main loop.
	 * If externalThread=FALSE do not call.
	 */
	psTelemetryServerPoll Poll;

	/**
	 * Retrieve the channel handle for use in conjunction with Poll().
	 * If externalThread=FALSE do not call.
	 */
	psTelemetryServerChannelHandle ChannelHandle;

	/*** Callbacks registered by the server. ***/

	/**
	 * Callback, when the channel got its id assigned
	 */
	psTelemetryServerChannelIdAssigned ChannelIdAssigned;
	/**
	 * Callback for the RDP Telemetry PDU.
	 */
	psTelemetryServerRdpTelemetry RdpTelemetry;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API TelemetryServerContext* telemetry_server_context_new(HANDLE vcm);
	FREERDP_API void telemetry_server_context_free(TelemetryServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_TELEMETRY_SERVER_TELEMETRY_H */
