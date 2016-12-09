/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Audio Virtual Channel
 *
 * Copyright 2012 Vic Lee
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

#ifndef FREERDP_CHANNEL_RDPSND_SERVER_H
#define FREERDP_CHANNEL_RDPSND_SERVER_H

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/rdpsnd.h>

typedef struct _rdpsnd_server_context RdpsndServerContext;
typedef struct _rdpsnd_server_context rdpsnd_server_context;
typedef struct _rdpsnd_server_private RdpsndServerPrivate;

typedef UINT (*psRdpsndStart)(RdpsndServerContext* context);
typedef UINT (*psRdpsndStop)(RdpsndServerContext* context);

typedef UINT (*psRdpsndServerInitialize)(RdpsndServerContext* context, BOOL ownThread);
typedef UINT (*psRdpsndServerSelectFormat)(RdpsndServerContext* context, int client_format_index);
typedef UINT (*psRdpsndServerSendSamples)(RdpsndServerContext* context, const void* buf, int nframes, UINT16 wTimestamp);
typedef UINT (*psRdpsndServerConfirmBlock)(RdpsndServerContext* context, BYTE confirmBlockNum, UINT16 wtimestamp);
typedef UINT (*psRdpsndServerSetVolume)(RdpsndServerContext* context, int left, int right);
typedef UINT (*psRdpsndServerClose)(RdpsndServerContext* context);


typedef void (*psRdpsndServerActivated)(RdpsndServerContext* context);

struct _rdpsnd_server_context
{
	HANDLE vcm;

	psRdpsndStart Start;
	psRdpsndStop Stop;

	RdpsndServerPrivate* priv;

	/* Server self-defined pointer. */
	void* data;

	/* Server supported formats. Set by server. */
	const AUDIO_FORMAT* server_formats;
	int num_server_formats;

	/* Server source PCM audio format. Set by server. */
	AUDIO_FORMAT src_format;

	/* Server audio latency, or buffer size, in milli-seconds. Set by server. */
	int latency;

	/* Client supported formats. */
	AUDIO_FORMAT* client_formats;
	int num_client_formats;
	int selected_client_format;

	/* Last sent audio block number. */
	int block_no;

	/*** APIs called by the server. ***/
	/**
	 * Initialize the channel. The caller should check the return value to see
	 * whether the initialization succeed. If not, the "Activated" callback
	 * will not be called and the server must not call any API on this context.
	 */
	psRdpsndServerInitialize Initialize;
	/**
	 * Choose the audio format to be sent. The index argument is an index into
	 * the client_formats array and must be smaller than num_client_formats.
	 */
	psRdpsndServerSelectFormat SelectFormat;
	/**
	 * Send audio samples. Actually bytes in the buffer must be:
	 * nframes * src_format.nBitsPerSample * src_format.nChannels / 8
	 */
	psRdpsndServerSendSamples SendSamples;
	/**
	 * Called when block confirm is received from the client
	 */
	psRdpsndServerConfirmBlock ConfirmBlock;
	/**
	 * Set the volume level of the client. Valid range is between 0 and 0xFFFF.
	 */
	psRdpsndServerSetVolume SetVolume;
	/**
	 * Close the audio stream.
	 */
	psRdpsndServerClose Close;

	/*** Callbacks registered by the server. ***/
	/**
	 * The channel has been activated. The server maybe choose audio format and
	 * start audio stream from this point. Note that this callback is called
	 * from a different thread context so the server must be careful of thread
	 * synchronization.
	 */
	psRdpsndServerActivated Activated;

	/**
	 *  MS-RDPEA channel version the client announces
	 */
	UINT16 clientVersion;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API RdpsndServerContext* rdpsnd_server_context_new(HANDLE vcm);
FREERDP_API void rdpsnd_server_context_reset(RdpsndServerContext *);
FREERDP_API void rdpsnd_server_context_free(RdpsndServerContext* context);
FREERDP_API HANDLE rdpsnd_server_get_event_handle(RdpsndServerContext *context);
FREERDP_API UINT rdpsnd_server_handle_messages(RdpsndServerContext *context);
FREERDP_API UINT rdpsnd_server_send_formats(RdpsndServerContext* context, wStream* s);


#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPSND_SERVER_H */
