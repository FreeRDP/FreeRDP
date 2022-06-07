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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/channels/log.h>

#include "rdpsnd_common.h"
#include "rdpsnd_main.h"

static wStream* rdpsnd_server_get_buffer(RdpsndServerContext* context)
{
	wStream* s;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	s = context->priv->rdpsnd_pdu;
	Stream_SetPosition(s, 0);
	return s;
}

/**
 * Send Server Audio Formats and Version PDU (2.2.2.1)
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_send_formats(RdpsndServerContext* context)
{
	wStream* s = rdpsnd_server_get_buffer(context);
	size_t pos;
	UINT16 i;
	BOOL status = FALSE;
	ULONG written;

	if (!Stream_EnsureRemainingCapacity(s, 24))
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT8(s, SNDC_FORMATS);
	Stream_Write_UINT8(s, 0);
	Stream_Seek_UINT16(s);
	Stream_Write_UINT32(s, 0);                           /* dwFlags */
	Stream_Write_UINT32(s, 0);                           /* dwVolume */
	Stream_Write_UINT32(s, 0);                           /* dwPitch */
	Stream_Write_UINT16(s, 0);                           /* wDGramPort */
	Stream_Write_UINT16(s, context->num_server_formats); /* wNumberOfFormats */
	Stream_Write_UINT8(s, context->block_no);            /* cLastBlockConfirmed */
	Stream_Write_UINT16(s, CHANNEL_VERSION_WIN_MAX);     /* wVersion */
	Stream_Write_UINT8(s, 0);                            /* bPad */

	for (i = 0; i < context->num_server_formats; i++)
	{
		const AUDIO_FORMAT* format = &context->server_formats[i];

		if (!audio_format_write(s, format))
			goto fail;
	}

	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, pos - 4);
	Stream_SetPosition(s, pos);

	WINPR_ASSERT(context->priv);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR)Stream_Buffer(s),
	                                Stream_GetPosition(s), &written);
	Stream_SetPosition(s, 0);
fail:
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Read Wave Confirm PDU (2.2.3.8) and handle callback
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_recv_waveconfirm(RdpsndServerContext* context, wStream* s)
{
	UINT16 timestamp;
	BYTE confirmBlockNum;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, timestamp);
	Stream_Read_UINT8(s, confirmBlockNum);
	Stream_Seek_UINT8(s);
	IFCALLRET(context->ConfirmBlock, error, context, confirmBlockNum, timestamp);

	if (error)
		WLog_ERR(TAG, "context->ConfirmBlock failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Read Training Confirm PDU (2.2.3.2) and handle callback
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_recv_trainingconfirm(RdpsndServerContext* context, wStream* s)
{
	UINT16 timestamp;
	UINT16 packsize;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, timestamp);
	Stream_Read_UINT16(s, packsize);

	IFCALLRET(context->TrainingConfirm, error, context, timestamp, packsize);
	if (error)
		WLog_ERR(TAG, "context->TrainingConfirm failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Read Quality Mode PDU (2.2.2.3)
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_recv_quality_mode(RdpsndServerContext* context, wStream* s)
{
	WINPR_ASSERT(context);

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, context->qualityMode); /* wQualityMode */
	Stream_Seek_UINT16(s);                       /* Reserved */

	WLog_DBG(TAG, "Client requested sound quality: 0x%04" PRIX16 "", context->qualityMode);

	return CHANNEL_RC_OK;
}

/**
 * Read Client Audio Formats and Version PDU (2.2.2.2)
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_recv_formats(RdpsndServerContext* context, wStream* s)
{
	UINT16 i, num_known_format = 0;
	UINT16 udpPort;
	BYTE lastblock;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, context->capsFlags);          /* dwFlags */
	Stream_Read_UINT32(s, context->initialVolume);      /* dwVolume */
	Stream_Read_UINT32(s, context->initialPitch);       /* dwPitch */
	Stream_Read_UINT16(s, udpPort);                     /* wDGramPort */
	Stream_Read_UINT16(s, context->num_client_formats); /* wNumberOfFormats */
	Stream_Read_UINT8(s, lastblock);                    /* cLastBlockConfirmed */
	Stream_Read_UINT16(s, context->clientVersion);      /* wVersion */
	Stream_Seek_UINT8(s);                               /* bPad */

	/* this check is only a guess as cbSize can influence the size of a format record */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 18ull * context->num_client_formats))
		return ERROR_INVALID_DATA;

	if (!context->num_client_formats)
	{
		WLog_ERR(TAG, "client doesn't support any format!");
		return ERROR_INTERNAL_ERROR;
	}

	context->client_formats = audio_formats_new(context->num_client_formats);

	if (!context->client_formats)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (i = 0; i < context->num_client_formats; i++)
	{
		AUDIO_FORMAT* format = &context->client_formats[i];

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 18))
		{
			WLog_ERR(TAG, "not enough data in stream!");
			error = ERROR_INVALID_DATA;
			goto out_free;
		}

		Stream_Read_UINT16(s, format->wFormatTag);
		Stream_Read_UINT16(s, format->nChannels);
		Stream_Read_UINT32(s, format->nSamplesPerSec);
		Stream_Read_UINT32(s, format->nAvgBytesPerSec);
		Stream_Read_UINT16(s, format->nBlockAlign);
		Stream_Read_UINT16(s, format->wBitsPerSample);
		Stream_Read_UINT16(s, format->cbSize);

		if (format->cbSize > 0)
		{
			if (!Stream_SafeSeek(s, format->cbSize))
			{
				WLog_ERR(TAG, "Stream_SafeSeek failed!");
				error = ERROR_INTERNAL_ERROR;
				goto out_free;
			}
		}

		if (format->wFormatTag != 0)
		{
			// lets call this a known format
			// TODO: actually look through our own list of known formats
			num_known_format++;
		}
	}

	if (!context->num_client_formats)
	{
		WLog_ERR(TAG, "client doesn't support any known format!");
		goto out_free;
	}

	return CHANNEL_RC_OK;
out_free:
	free(context->client_formats);
	return error;
}

static DWORD WINAPI rdpsnd_server_thread(LPVOID arg)
{
	DWORD nCount = 0, status;
	HANDLE events[2] = { 0 };
	RdpsndServerContext* context = (RdpsndServerContext*)arg;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	events[nCount++] = context->priv->channelEvent;
	events[nCount++] = context->priv->StopEvent;

	WINPR_ASSERT(nCount <= ARRAYSIZE(events));

	while (TRUE)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "!", error);
			break;
		}

		status = WaitForSingleObject(context->priv->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

		if ((error = rdpsnd_server_handle_messages(context)))
		{
			WLog_ERR(TAG, "rdpsnd_server_handle_messages failed with error %" PRIu32 "", error);
			break;
		}
	}

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error, "rdpsnd_server_thread reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_initialize(RdpsndServerContext* context, BOOL ownThread)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	context->priv->ownThread = ownThread;
	return context->Start(context);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_select_format(RdpsndServerContext* context, UINT16 client_format_index)
{
	int bs;
	int out_buffer_size;
	AUDIO_FORMAT* format;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if ((client_format_index >= context->num_client_formats) || (!context->src_format))
	{
		WLog_ERR(TAG, "index %d is not correct.", client_format_index);
		return ERROR_INVALID_DATA;
	}

	EnterCriticalSection(&context->priv->lock);
	context->priv->src_bytes_per_sample = context->src_format->wBitsPerSample / 8;
	context->priv->src_bytes_per_frame =
	    context->priv->src_bytes_per_sample * context->src_format->nChannels;
	context->selected_client_format = client_format_index;
	format = &context->client_formats[client_format_index];

	if (format->nSamplesPerSec == 0)
	{
		WLog_ERR(TAG, "invalid Client Sound Format!!");
		error = ERROR_INVALID_DATA;
		goto out;
	}

	if (context->latency <= 0)
		context->latency = 50;

	context->priv->out_frames = context->src_format->nSamplesPerSec * context->latency / 1000;

	if (context->priv->out_frames < 1)
		context->priv->out_frames = 1;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_DVI_ADPCM:
			bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
			context->priv->out_frames -= context->priv->out_frames % bs;

			if (context->priv->out_frames < bs)
				context->priv->out_frames = bs;

			break;

		case WAVE_FORMAT_ADPCM:
			bs = (format->nBlockAlign - 7 * format->nChannels) * 2 / format->nChannels + 2;
			context->priv->out_frames -= context->priv->out_frames % bs;

			if (context->priv->out_frames < bs)
				context->priv->out_frames = bs;

			break;
	}

	context->priv->out_pending_frames = 0;
	out_buffer_size = context->priv->out_frames * context->priv->src_bytes_per_frame;

	if (context->priv->out_buffer_size < out_buffer_size)
	{
		BYTE* newBuffer;
		newBuffer = (BYTE*)realloc(context->priv->out_buffer, out_buffer_size);

		if (!newBuffer)
		{
			WLog_ERR(TAG, "realloc failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto out;
		}

		context->priv->out_buffer = newBuffer;
		context->priv->out_buffer_size = out_buffer_size;
	}

	freerdp_dsp_context_reset(context->priv->dsp_context, format, 0u);
out:
	LeaveCriticalSection(&context->priv->lock);
	return error;
}

/**
 * Send Training PDU (2.2.3.1)
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_training(RdpsndServerContext* context, UINT16 timestamp, UINT16 packsize,
                                   BYTE* data)
{
	size_t end = 0;
	ULONG written;
	BOOL status;
	wStream* s = rdpsnd_server_get_buffer(context);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return ERROR_INTERNAL_ERROR;

	Stream_Write_UINT8(s, SNDC_TRAINING);
	Stream_Write_UINT8(s, 0);
	Stream_Seek_UINT16(s);
	Stream_Write_UINT16(s, timestamp);
	Stream_Write_UINT16(s, packsize);

	if (packsize > 0)
	{
		if (!Stream_EnsureRemainingCapacity(s, packsize))
		{
			Stream_SetPosition(s, 0);
			return ERROR_INTERNAL_ERROR;
		}

		Stream_Write(s, data, packsize);
	}

	end = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, end - 4);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR)Stream_Buffer(s), end,
	                                &written);

	Stream_SetPosition(s, 0);

	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static BOOL rdpsnd_server_align_wave_pdu(wStream* s, UINT32 alignment)
{
	size_t size;
	Stream_SealLength(s);
	size = Stream_Length(s);

	if ((size % alignment) != 0)
	{
		size_t offset = alignment - size % alignment;

		if (!Stream_EnsureRemainingCapacity(s, offset))
			return FALSE;

		Stream_Zero(s, offset);
	}

	Stream_SealLength(s);
	return TRUE;
}

/**
 * Function description
 * context->priv->lock should be obtained before calling this function
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_send_wave_pdu(RdpsndServerContext* context, UINT16 wTimestamp)
{
	size_t length;
	size_t start, end = 0;
	const BYTE* src;
	AUDIO_FORMAT* format;
	ULONG written;
	UINT error = CHANNEL_RC_OK;
	wStream* s = rdpsnd_server_get_buffer(context);

	if (context->selected_client_format > context->num_client_formats)
		return ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(context->client_formats);

	format = &context->client_formats[context->selected_client_format];
	/* WaveInfo PDU */
	Stream_SetPosition(s, 0);

	if (!Stream_EnsureRemainingCapacity(s, 16))
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT8(s, SNDC_WAVE);                        /* msgType */
	Stream_Write_UINT8(s, 0);                                /* bPad */
	Stream_Write_UINT16(s, 0);                               /* BodySize */
	Stream_Write_UINT16(s, wTimestamp);                      /* wTimeStamp */
	Stream_Write_UINT16(s, context->selected_client_format); /* wFormatNo */
	Stream_Write_UINT8(s, context->block_no);                /* cBlockNo */
	Stream_Seek(s, 3);                                       /* bPad */
	start = Stream_GetPosition(s);
	src = context->priv->out_buffer;
	length = context->priv->out_pending_frames * context->priv->src_bytes_per_frame * 1ULL;

	if (!freerdp_dsp_encode(context->priv->dsp_context, context->src_format, src, length, s))
		return ERROR_INTERNAL_ERROR;
	else
	{
		/* Set stream size */
		if (!rdpsnd_server_align_wave_pdu(s, format->nBlockAlign))
			return ERROR_INTERNAL_ERROR;

		end = Stream_GetPosition(s);
		Stream_SetPosition(s, 2);
		Stream_Write_UINT16(s, end - start + 8);
		Stream_SetPosition(s, end);

		if (!WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR)Stream_Buffer(s),
		                            start + 4, &written))
		{
			WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
			error = ERROR_INTERNAL_ERROR;
		}
	}

	if (error != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	Stream_SetPosition(s, start);
	Stream_Write_UINT32(s, 0); /* bPad */
	Stream_SetPosition(s, start);

	if (!WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR)Stream_Pointer(s), end - start,
	                            &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		error = ERROR_INTERNAL_ERROR;
	}

	context->block_no = (context->block_no + 1) % 256;

out:
	Stream_SetPosition(s, 0);
	context->priv->out_pending_frames = 0;
	return error;
}

/**
 * Function description
 * context->priv->lock should be obtained before calling this function
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_send_wave2_pdu(RdpsndServerContext* context, UINT16 formatNo,
                                         const BYTE* data, size_t size, BOOL encoded,
                                         UINT16 timestamp, UINT32 audioTimeStamp)
{
	size_t end = 0;
	ULONG written;
	UINT error = CHANNEL_RC_OK;
	BOOL status;
	wStream* s = rdpsnd_server_get_buffer(context);

	if (!Stream_EnsureRemainingCapacity(s, 16))
	{
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	/* Wave2 PDU */
	Stream_Write_UINT8(s, SNDC_WAVE2);        /* msgType */
	Stream_Write_UINT8(s, 0);                 /* bPad */
	Stream_Write_UINT16(s, 0);                /* BodySize */
	Stream_Write_UINT16(s, timestamp);        /* wTimeStamp */
	Stream_Write_UINT16(s, formatNo);         /* wFormatNo */
	Stream_Write_UINT8(s, context->block_no); /* cBlockNo */
	Stream_Write_UINT8(s, 0);                 /* bPad */
	Stream_Write_UINT8(s, 0);                 /* bPad */
	Stream_Write_UINT8(s, 0);                 /* bPad */
	Stream_Write_UINT32(s, audioTimeStamp);   /* dwAudioTimeStamp */

	if (encoded)
	{
		if (!Stream_EnsureRemainingCapacity(s, size))
		{
			error = ERROR_INTERNAL_ERROR;
			goto out;
		}

		Stream_Write(s, data, size);
	}
	else
	{
		AUDIO_FORMAT* format;

		if (!freerdp_dsp_encode(context->priv->dsp_context, context->src_format, data, size, s))
		{
			error = ERROR_INTERNAL_ERROR;
			goto out;
		}

		format = &context->client_formats[formatNo];
		if (!rdpsnd_server_align_wave_pdu(s, format->nBlockAlign))
		{
			error = ERROR_INTERNAL_ERROR;
			goto out;
		}
	}

	end = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, end - 4);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR)Stream_Buffer(s), end,
	                                &written);

	if (!status || (end != written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed! [stream length=%" PRIdz " - written=%" PRIu32,
		         end, written);
		error = ERROR_INTERNAL_ERROR;
	}

	context->block_no = (context->block_no + 1) % 256;

out:
	Stream_SetPosition(s, 0);
	context->priv->out_pending_frames = 0;
	return error;
}

/* Wrapper function to send WAVE or WAVE2 PDU depending on client connected */
static UINT rdpsnd_server_send_audio_pdu(RdpsndServerContext* context, UINT16 wTimestamp)
{
	const BYTE* src;
	size_t length;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (context->selected_client_format >= context->num_client_formats)
		return ERROR_INTERNAL_ERROR;

	src = context->priv->out_buffer;
	length = context->priv->out_pending_frames * context->priv->src_bytes_per_frame;

	if (context->clientVersion >= CHANNEL_VERSION_WIN_8)
		return rdpsnd_server_send_wave2_pdu(context, context->selected_client_format, src, length,
		                                    FALSE, wTimestamp, wTimestamp);
	else
		return rdpsnd_server_send_wave_pdu(context, wTimestamp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_send_samples(RdpsndServerContext* context, const void* buf,
                                       size_t nframes, UINT16 wTimestamp)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	EnterCriticalSection(&context->priv->lock);

	if (context->selected_client_format >= context->num_client_formats)
	{
		/* It's possible while format negotiation has not been done */
		WLog_WARN(TAG, "Drop samples because client format has not been negotiated.");
		error = ERROR_NOT_READY;
		goto out;
	}

	while (nframes > 0)
	{
		const size_t cframes =
		    MIN(nframes, context->priv->out_frames - context->priv->out_pending_frames);
		size_t cframesize = cframes * context->priv->src_bytes_per_frame;
		CopyMemory(context->priv->out_buffer +
		               (context->priv->out_pending_frames * context->priv->src_bytes_per_frame),
		           buf, cframesize);
		buf = (const BYTE*)buf + cframesize;
		nframes -= cframes;
		context->priv->out_pending_frames += cframes;

		if (context->priv->out_pending_frames >= context->priv->out_frames)
		{
			if ((error = rdpsnd_server_send_audio_pdu(context, wTimestamp)))
			{
				WLog_ERR(TAG, "rdpsnd_server_send_audio_pdu failed with error %" PRIu32 "", error);
				break;
			}
		}
	}

out:
	LeaveCriticalSection(&context->priv->lock);
	return error;
}

/**
 * Send encoded audio samples using a Wave2 PDU.
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_send_samples2(RdpsndServerContext* context, UINT16 formatNo,
                                        const void* buf, size_t size, UINT16 timestamp,
                                        UINT32 audioTimeStamp)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (context->clientVersion < CHANNEL_VERSION_WIN_8)
		return ERROR_INTERNAL_ERROR;

	EnterCriticalSection(&context->priv->lock);

	error =
	    rdpsnd_server_send_wave2_pdu(context, formatNo, buf, size, TRUE, timestamp, audioTimeStamp);

	LeaveCriticalSection(&context->priv->lock);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_set_volume(RdpsndServerContext* context, UINT16 left, UINT16 right)
{
	size_t len;
	BOOL status;
	ULONG written;
	wStream* s = rdpsnd_server_get_buffer(context);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT8(s, SNDC_SETVOLUME);
	Stream_Write_UINT8(s, 0);
	Stream_Write_UINT16(s, 4); /* Payload length */
	Stream_Write_UINT16(s, left);
	Stream_Write_UINT16(s, right);
	len = Stream_GetPosition(s);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR)Stream_Buffer(s),
	                                (ULONG)len, &written);
	Stream_SetPosition(s, 0);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_close(RdpsndServerContext* context)
{
	size_t pos;
	BOOL status;
	ULONG written;
	UINT error = CHANNEL_RC_OK;
	wStream* s = rdpsnd_server_get_buffer(context);

	EnterCriticalSection(&context->priv->lock);

	if (context->priv->out_pending_frames > 0)
	{
		if (context->selected_client_format >= context->num_client_formats)
		{
			WLog_ERR(TAG, "Pending audio frame exists while no format selected.");
			error = ERROR_INVALID_DATA;
		}
		else if ((error = rdpsnd_server_send_audio_pdu(context, 0)))
		{
			WLog_ERR(TAG, "rdpsnd_server_send_audio_pdu failed with error %" PRIu32 "", error);
		}
	}

	LeaveCriticalSection(&context->priv->lock);

	if (error)
		return error;

	context->selected_client_format = 0xFFFF;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT8(s, SNDC_CLOSE);
	Stream_Write_UINT8(s, 0);
	Stream_Seek_UINT16(s);
	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, pos - 4);
	Stream_SetPosition(s, pos);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR)Stream_Buffer(s),
	                                Stream_GetPosition(s), &written);
	Stream_SetPosition(s, 0);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_start(RdpsndServerContext* context)
{
	void* buffer = NULL;
	DWORD bytesReturned;
	RdpsndServerPrivate* priv;
	UINT error = ERROR_INTERNAL_ERROR;
	PULONG pSessionId = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	priv = context->priv;
	priv->SessionId = WTS_CURRENT_SESSION;

	if (context->use_dynamic_virtual_channel)
	{
		UINT32 channelId;
		BOOL status = TRUE;

		if (WTSQuerySessionInformationA(context->vcm, WTS_CURRENT_SESSION, WTSSessionId,
		                                (LPSTR*)&pSessionId, &bytesReturned))
		{
			priv->SessionId = (DWORD)*pSessionId;
			WTSFreeMemory(pSessionId);
			priv->ChannelHandle = (HANDLE)WTSVirtualChannelOpenEx(
			    priv->SessionId, RDPSND_DVC_CHANNEL_NAME, WTS_CHANNEL_OPTION_DYNAMIC);
			if (!priv->ChannelHandle)
			{
				WLog_ERR(TAG, "Open audio dynamic virtual channel (%s) failed!",
				         RDPSND_DVC_CHANNEL_NAME);
				return ERROR_INTERNAL_ERROR;
			}

			channelId = WTSChannelGetIdByHandle(priv->ChannelHandle);

			IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
			if (!status)
			{
				WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
				goto out_close;
			}
		}
		else
		{
			WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}
	else
	{
		priv->ChannelHandle =
		    WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, RDPSND_CHANNEL_NAME);
		if (!priv->ChannelHandle)
		{
			WLog_ERR(TAG, "Open audio static virtual channel (rdpsnd) failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	if (!WTSVirtualChannelQuery(priv->ChannelHandle, WTSVirtualEventHandle, &buffer,
	                            &bytesReturned) ||
	    (bytesReturned != sizeof(HANDLE)))
	{
		WLog_ERR(TAG,
		         "error during WTSVirtualChannelQuery(WTSVirtualEventHandle) or invalid returned "
		         "size(%" PRIu32 ")",
		         bytesReturned);

		if (buffer)
			WTSFreeMemory(buffer);

		goto out_close;
	}

	CopyMemory(&priv->channelEvent, buffer, sizeof(HANDLE));
	WTSFreeMemory(buffer);
	priv->rdpsnd_pdu = Stream_New(NULL, 4096);

	if (!priv->rdpsnd_pdu)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out_close;
	}

	if (!InitializeCriticalSectionEx(&context->priv->lock, 0, 0))
	{
		WLog_ERR(TAG, "InitializeCriticalSectionEx failed!");
		goto out_pdu;
	}

	if ((error = rdpsnd_server_send_formats(context)))
	{
		WLog_ERR(TAG, "rdpsnd_server_send_formats failed with error %" PRIu32 "", error);
		goto out_lock;
	}

	if (priv->ownThread)
	{
		context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		if (!context->priv->StopEvent)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			goto out_lock;
		}

		context->priv->Thread =
		    CreateThread(NULL, 0, rdpsnd_server_thread, (void*)context, 0, NULL);

		if (!context->priv->Thread)
		{
			WLog_ERR(TAG, "CreateThread failed!");
			goto out_stopEvent;
		}
	}

	return CHANNEL_RC_OK;
out_stopEvent:
	CloseHandle(context->priv->StopEvent);
	context->priv->StopEvent = NULL;
out_lock:
	DeleteCriticalSection(&context->priv->lock);
out_pdu:
	Stream_Free(context->priv->rdpsnd_pdu, TRUE);
	context->priv->rdpsnd_pdu = NULL;
out_close:
	WTSVirtualChannelClose(context->priv->ChannelHandle);
	context->priv->ChannelHandle = NULL;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_server_stop(RdpsndServerContext* context)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (!context->priv->StopEvent)
		return error;

	if (context->priv->ownThread)
	{
		if (context->priv->StopEvent)
		{
			SetEvent(context->priv->StopEvent);

			if (WaitForSingleObject(context->priv->Thread, INFINITE) == WAIT_FAILED)
			{
				error = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", error);
				return error;
			}

			CloseHandle(context->priv->Thread);
			CloseHandle(context->priv->StopEvent);
			context->priv->Thread = NULL;
			context->priv->StopEvent = NULL;
		}
	}

	DeleteCriticalSection(&context->priv->lock);

	if (context->priv->rdpsnd_pdu)
	{
		Stream_Free(context->priv->rdpsnd_pdu, TRUE);
		context->priv->rdpsnd_pdu = NULL;
	}

	if (context->priv->ChannelHandle)
	{
		WTSVirtualChannelClose(context->priv->ChannelHandle);
		context->priv->ChannelHandle = NULL;
	}

	return error;
}

RdpsndServerContext* rdpsnd_server_context_new(HANDLE vcm)
{
	RdpsndServerPrivate* priv;
	RdpsndServerContext* context = (RdpsndServerContext*)calloc(1, sizeof(RdpsndServerContext));

	if (!context)
		goto fail;

	context->vcm = vcm;
	context->Start = rdpsnd_server_start;
	context->Stop = rdpsnd_server_stop;
	context->selected_client_format = 0xFFFF;
	context->Initialize = rdpsnd_server_initialize;
	context->SendFormats = rdpsnd_server_send_formats;
	context->SelectFormat = rdpsnd_server_select_format;
	context->Training = rdpsnd_server_training;
	context->SendSamples = rdpsnd_server_send_samples;
	context->SendSamples2 = rdpsnd_server_send_samples2;
	context->SetVolume = rdpsnd_server_set_volume;
	context->Close = rdpsnd_server_close;
	context->priv = priv = (RdpsndServerPrivate*)calloc(1, sizeof(RdpsndServerPrivate));

	if (!priv)
	{
		WLog_ERR(TAG, "calloc failed!");
		goto fail;
	}

	priv->dsp_context = freerdp_dsp_context_new(TRUE);

	if (!priv->dsp_context)
	{
		WLog_ERR(TAG, "freerdp_dsp_context_new failed!");
		goto fail;
	}

	priv->input_stream = Stream_New(NULL, 4);

	if (!priv->input_stream)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto fail;
	}

	priv->expectedBytes = 4;
	priv->waitingHeader = TRUE;
	priv->ownThread = TRUE;
	return context;
fail:
	rdpsnd_server_context_free(context);
	return NULL;
}

void rdpsnd_server_context_reset(RdpsndServerContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	context->priv->expectedBytes = 4;
	context->priv->waitingHeader = TRUE;
	Stream_SetPosition(context->priv->input_stream, 0);
}

void rdpsnd_server_context_free(RdpsndServerContext* context)
{
	if (!context)
		return;

	if (context->priv)
	{
		rdpsnd_server_stop(context);

		free(context->priv->out_buffer);

		if (context->priv->dsp_context)
			freerdp_dsp_context_free(context->priv->dsp_context);

		if (context->priv->input_stream)
			Stream_Free(context->priv->input_stream, TRUE);
	}

	free(context->server_formats);
	free(context->client_formats);
	free(context->priv);
	free(context);
}

HANDLE rdpsnd_server_get_event_handle(RdpsndServerContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	return context->priv->channelEvent;
}

/*
 * Handle rpdsnd messages - server side
 *
 * @param Server side context
 *
 * @return 0 on success
 * 		   ERROR_NO_DATA if no data could be read this time
 *         otherwise error
 */
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpsnd_server_handle_messages(RdpsndServerContext* context)
{
	DWORD bytesReturned;
	UINT ret = CHANNEL_RC_OK;
	RdpsndServerPrivate* priv;
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	priv = context->priv;
	s = priv->input_stream;

	if (!WTSVirtualChannelRead(priv->ChannelHandle, 0, (PCHAR)Stream_Pointer(s),
	                           priv->expectedBytes, &bytesReturned))
	{
		if (GetLastError() == ERROR_NO_DATA)
			return ERROR_NO_DATA;

		WLog_ERR(TAG, "channel connection closed");
		return ERROR_INTERNAL_ERROR;
	}

	priv->expectedBytes -= bytesReturned;
	Stream_Seek(s, bytesReturned);

	if (priv->expectedBytes)
		return CHANNEL_RC_OK;

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);

	if (priv->waitingHeader)
	{
		/* header case */
		Stream_Read_UINT8(s, priv->msgType);
		Stream_Seek_UINT8(s); /* bPad */
		Stream_Read_UINT16(s, priv->expectedBytes);
		priv->waitingHeader = FALSE;
		Stream_SetPosition(s, 0);

		if (priv->expectedBytes)
		{
			if (!Stream_EnsureCapacity(s, priv->expectedBytes))
			{
				WLog_ERR(TAG, "Stream_EnsureCapacity failed!");
				return CHANNEL_RC_NO_MEMORY;
			}

			return CHANNEL_RC_OK;
		}
	}

	/* when here we have the header + the body */
#ifdef WITH_DEBUG_SND
	WLog_DBG(TAG, "message type %" PRIu8 "", priv->msgType);
#endif
	priv->expectedBytes = 4;
	priv->waitingHeader = TRUE;

	switch (priv->msgType)
	{
		case SNDC_WAVECONFIRM:
			ret = rdpsnd_server_recv_waveconfirm(context, s);
			break;

		case SNDC_TRAINING:
			ret = rdpsnd_server_recv_trainingconfirm(context, s);
			break;

		case SNDC_FORMATS:
			ret = rdpsnd_server_recv_formats(context, s);

			if ((ret == CHANNEL_RC_OK) && (context->clientVersion < CHANNEL_VERSION_WIN_7))
				IFCALL(context->Activated, context);

			break;

		case SNDC_QUALITYMODE:
			ret = rdpsnd_server_recv_quality_mode(context, s);

			if ((ret == CHANNEL_RC_OK) && (context->clientVersion >= CHANNEL_VERSION_WIN_7))
				IFCALL(context->Activated, context);

			break;

		default:
			WLog_ERR(TAG, "UNKNOWN MESSAGE TYPE!! (0x%02" PRIX8 ")", priv->msgType);
			ret = ERROR_INVALID_DATA;
			break;
	}

	Stream_SetPosition(s, 0);
	return ret;
}
