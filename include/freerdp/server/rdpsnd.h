/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Audio Virtual Channel
 *
 * Copyright 2012 Vic Lee
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

typedef struct _rdpsnd_server_context rdpsnd_server_context;

typedef BOOL (*psRdpsndServerInitialize)(rdpsnd_server_context* context);
typedef void (*psRdpsndServerSelectFormat)(rdpsnd_server_context* context, int client_format_index);
typedef BOOL (*psRdpsndServerSendSamples)(rdpsnd_server_context* context, const void* buf, int nframes);
typedef BOOL (*psRdpsndServerSetVolume)(rdpsnd_server_context* context, int left, int right);
typedef BOOL (*psRdpsndServerClose)(rdpsnd_server_context* context);

typedef void (*psRdpsndServerActivated)(rdpsnd_server_context* context);

struct _rdpsnd_server_context
{
	WTSVirtualChannelManager* vcm;

	/* Server self-defined pointer. */
	void* data;

	/* Server supported formats. Set by server. */
	const AUDIO_FORMAT* server_formats;
	int num_server_formats;

	/* Server source PCM audio format. Set by server. */
	AUDIO_FORMAT src_format;

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
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API rdpsnd_server_context* rdpsnd_server_context_new(WTSVirtualChannelManager* vcm);
FREERDP_API void rdpsnd_server_context_free(rdpsnd_server_context* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPSND_SERVER_H */
