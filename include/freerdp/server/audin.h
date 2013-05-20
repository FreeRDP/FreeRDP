/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Audio Input Virtual Channel
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

#ifndef FREERDP_CHANNEL_AUDIN_SERVER_H
#define FREERDP_CHANNEL_AUDIN_SERVER_H

#include <freerdp/codec/audio.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/rdpsnd.h>

typedef struct _audin_server_context audin_server_context;

typedef void (*psAudinServerSelectFormat)(audin_server_context* context, int client_format_index);
typedef BOOL (*psAudinServerOpen)(audin_server_context* context);
typedef BOOL (*psAudinServerClose)(audin_server_context* context);

typedef void (*psAudinServerOpening)(audin_server_context* context);
typedef void (*psAudinServerOpenResult)(audin_server_context* context, UINT32 result);
typedef void (*psAudinServerReceiveSamples)(audin_server_context* context, const void* buf, int nframes);

struct _audin_server_context
{
	WTSVirtualChannelManager* vcm;

	/* Server self-defined pointer. */
	void* data;

	/* Server supported formats. Set by server. */
	const AUDIO_FORMAT* server_formats;
	int num_server_formats;

	/* Server destination PCM audio format. Set by server. */
	AUDIO_FORMAT dst_format;

	/* Server preferred frames per packet. */
	int frames_per_packet;

	/* Client supported formats. */
	AUDIO_FORMAT* client_formats;
	int num_client_formats;
	int selected_client_format;

	/*** APIs called by the server. ***/
	/**
	 * Choose the audio format to be received. The index argument is an index into
	 * the client_formats array and must be smaller than num_client_formats.
	 */
	psAudinServerSelectFormat SelectFormat;
	/**
	 * Open the audio input stream.
	 */
	psAudinServerOpen Open;
	/**
	 * Close the audio stream.
	 */
	psAudinServerClose Close;

	/*** Callbacks registered by the server. ***/
	/**
	 * It's ready to open the audio input stream. The server should examine client
	 * formats and call SelectFormat to choose the desired one in this callback.
	 */
	psAudinServerOpening Opening;
	/**
	 * Client replied HRESULT of the open operation.
	 */
	psAudinServerOpenResult OpenResult;
	/**
	 * Receive audio samples. Actual bytes in the buffer is:
	 * nframes * dst_format.nBitsPerSample * dst_format.nChannels / 8
	 * Note that this callback is called from a different thread context so the
	 * server must be careful of thread synchronization.
	 */
	psAudinServerReceiveSamples ReceiveSamples;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API audin_server_context* audin_server_context_new(WTSVirtualChannelManager* vcm);
FREERDP_API void audin_server_context_free(audin_server_context* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_AUDIN_SERVER_H */
