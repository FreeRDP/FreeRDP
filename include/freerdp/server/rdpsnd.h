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

typedef struct s_rdpsnd_server_context RdpsndServerContext;
typedef struct s_rdpsnd_server_context rdpsnd_server_context;
typedef struct s_rdpsnd_server_private RdpsndServerPrivate;

typedef UINT (*psRdpsndStart)(RdpsndServerContext* context);
typedef UINT (*psRdpsndStop)(RdpsndServerContext* context);

typedef BOOL (*psRdpsndChannelIdAssigned)(RdpsndServerContext* context, UINT32 channelId);

typedef UINT (*psRdpsndServerInitialize)(RdpsndServerContext* context, BOOL ownThread);
typedef UINT (*psRdpsndServerSendFormats)(RdpsndServerContext* context);
typedef UINT (*psRdpsndServerSelectFormat)(RdpsndServerContext* context,
                                           UINT16 client_format_index);
typedef UINT (*psRdpsndServerTraining)(RdpsndServerContext* context, UINT16 timestamp,
                                       UINT16 packsize, BYTE* data);
typedef UINT (*psRdpsndServerTrainingConfirm)(RdpsndServerContext* context, UINT16 timestamp,
                                              UINT16 packsize);
typedef UINT (*psRdpsndServerSendSamples)(RdpsndServerContext* context, const void* buf,
                                          size_t nframes, UINT16 wTimestamp);
typedef UINT (*psRdpsndServerSendSamples2)(RdpsndServerContext* context, UINT16 formatNo,
                                           const void* buf, size_t size, UINT16 timestamp,
                                           UINT32 audioTimeStamp);
typedef UINT (*psRdpsndServerConfirmBlock)(RdpsndServerContext* context, BYTE confirmBlockNum,
                                           UINT16 wtimestamp);
typedef UINT (*psRdpsndServerSetVolume)(RdpsndServerContext* context, UINT16 left, UINT16 right);
typedef UINT (*psRdpsndServerClose)(RdpsndServerContext* context);

typedef void (*psRdpsndServerActivated)(RdpsndServerContext* context);

struct s_rdpsnd_server_context
{
	HANDLE vcm;

	psRdpsndStart Start;
	psRdpsndStop Stop;

	RdpsndServerPrivate* priv;

	/* Server self-defined pointer. */
	void* data;

	/* Server to request to use dynamic virtual channel. */
	BOOL use_dynamic_virtual_channel;

	/* Server supported formats. Set by server. */
	AUDIO_FORMAT* server_formats;
	size_t num_server_formats;

	/* Server source PCM audio format. Set by server. */
	AUDIO_FORMAT* src_format;

	/* Server audio latency, or buffer size, in milli-seconds. Set by server. */
	UINT32 latency;

	/* Client supported formats. */
	AUDIO_FORMAT* client_formats;
	UINT16 num_client_formats;
	UINT16 selected_client_format;

	/* Last sent audio block number. */
	UINT8 block_no;

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

	/* dwFlags in CLIENT_AUDIO_VERSION_AND_FORMATS */
	UINT32 capsFlags;
	/* dwVolume in CLIENT_AUDIO_VERSION_AND_FORMATS */
	UINT32 initialVolume;
	/* dwPitch in CLIENT_AUDIO_VERSION_AND_FORMATS */
	UINT32 initialPitch;

	UINT16 qualityMode;

	/**
	 * Send server formats and version to the client. Automatically sent, when
	 * opening the channel.
	 * Also used to restart the protocol after sending the Close PDU.
	 */
	psRdpsndServerSendFormats SendFormats;
	/**
	 * Send Training PDU.
	 */
	psRdpsndServerTraining Training;

	/**
	 * Send encoded audio samples using a Wave2 PDU.
	 * When successful, the block_no member is incremented.
	 */
	psRdpsndServerSendSamples2 SendSamples2;

	/**
	 * Called when a TrainingConfirm PDU is received from the client.
	 */
	psRdpsndServerTrainingConfirm TrainingConfirm;

	/**
	 * Callback, when the channel got its id assigned.
	 * Only called, when use_dynamic_virtual_channel=TRUE.
	 */
	psRdpsndChannelIdAssigned ChannelIdAssigned;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API RdpsndServerContext* rdpsnd_server_context_new(HANDLE vcm);
	FREERDP_API void rdpsnd_server_context_reset(RdpsndServerContext*);
	FREERDP_API void rdpsnd_server_context_free(RdpsndServerContext* context);
	FREERDP_API HANDLE rdpsnd_server_get_event_handle(RdpsndServerContext* context);
	FREERDP_API UINT rdpsnd_server_handle_messages(RdpsndServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPSND_SERVER_H */
