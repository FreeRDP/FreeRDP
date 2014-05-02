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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include "rdpsnd_main.h"

static BOOL rdpsnd_server_send_formats(RdpsndServerContext* context, wStream* s)
{
	int pos;
	UINT16 i;
	BOOL status;

	Stream_Write_UINT8(s, SNDC_FORMATS);
	Stream_Write_UINT8(s, 0);
	Stream_Seek_UINT16(s);

	Stream_Write_UINT32(s, 0); /* dwFlags */
	Stream_Write_UINT32(s, 0); /* dwVolume */
	Stream_Write_UINT32(s, 0); /* dwPitch */
	Stream_Write_UINT16(s, 0); /* wDGramPort */
	Stream_Write_UINT16(s, context->num_server_formats); /* wNumberOfFormats */
	Stream_Write_UINT8(s, context->block_no); /* cLastBlockConfirmed */
	Stream_Write_UINT16(s, 0x06); /* wVersion */
	Stream_Write_UINT8(s, 0); /* bPad */
	
	for (i = 0; i < context->num_server_formats; i++)
	{
		Stream_Write_UINT16(s, context->server_formats[i].wFormatTag); /* wFormatTag (WAVE_FORMAT_PCM) */
		Stream_Write_UINT16(s, context->server_formats[i].nChannels); /* nChannels */
		Stream_Write_UINT32(s, context->server_formats[i].nSamplesPerSec); /* nSamplesPerSec */

		Stream_Write_UINT32(s, context->server_formats[i].nSamplesPerSec *
			context->server_formats[i].nChannels *
			context->server_formats[i].wBitsPerSample / 8); /* nAvgBytesPerSec */

		Stream_Write_UINT16(s, context->server_formats[i].nBlockAlign); /* nBlockAlign */
		Stream_Write_UINT16(s, context->server_formats[i].wBitsPerSample); /* wBitsPerSample */
		Stream_Write_UINT16(s, context->server_formats[i].cbSize); /* cbSize */

		if (context->server_formats[i].cbSize > 0)
		{
			Stream_Write(s, context->server_formats[i].data, context->server_formats[i].cbSize);
		}
	}

	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, pos - 4);
	Stream_SetPosition(s, pos);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);
	Stream_SetPosition(s, 0);

	return status;
}

static BOOL rdpsnd_server_recv_waveconfirm(RdpsndServerContext* context, wStream* s)
{
	UINT16 timestamp = 0;
	BYTE confirmBlockNum = 0;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, timestamp);
	Stream_Read_UINT8(s, confirmBlockNum);
	Stream_Seek_UINT8(s);
	return TRUE;
}

static BOOL rdpsnd_server_recv_quality_mode(RdpsndServerContext* context, wStream* s)
{
	UINT16 quality;
	
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, quality);
	Stream_Seek_UINT16(s); // reserved
	
	fprintf(stderr, "Client requested sound quality: %#0X\n", quality);
	return TRUE;
}

static BOOL rdpsnd_server_recv_formats(RdpsndServerContext* context, wStream* s)
{
	int i, num_known_format = 0;
	UINT32 flags, vol, pitch;
	UINT16 udpPort, version;
	BYTE lastblock;

	if (Stream_GetRemainingLength(s) < 20)
		return FALSE;

	Stream_Read_UINT32(s, flags); /* dwFlags */
	Stream_Read_UINT32(s, vol); /* dwVolume */
	Stream_Read_UINT32(s, pitch); /* dwPitch */
	Stream_Read_UINT16(s, udpPort); /* wDGramPort */
	Stream_Read_UINT16(s, context->num_client_formats); /* wNumberOfFormats */
	Stream_Read_UINT8(s, lastblock); /* cLastBlockConfirmed */
	Stream_Read_UINT16(s, version); /* wVersion */
	Stream_Seek_UINT8(s); /* bPad */

	/* this check is only a guess as cbSize can influence the size of a format record */
	if (Stream_GetRemainingLength(s) < context->num_client_formats * 18)
		return FALSE;

	if (!context->num_client_formats)
	{
		fprintf(stderr, "%s: client doesn't support any format!\n", __FUNCTION__);
		return FALSE;
	}

	context->client_formats = (AUDIO_FORMAT *)calloc(context->num_client_formats, sizeof(AUDIO_FORMAT));
	if (!context->client_formats)
		return FALSE;

	for (i = 0; i < context->num_client_formats; i++)
	{
		if (Stream_GetRemainingLength(s) < 18)
			goto out_free;

		Stream_Read_UINT16(s, context->client_formats[i].wFormatTag);
		Stream_Read_UINT16(s, context->client_formats[i].nChannels);
		Stream_Read_UINT32(s, context->client_formats[i].nSamplesPerSec);
		Stream_Read_UINT32(s, context->client_formats[i].nAvgBytesPerSec);
		Stream_Read_UINT16(s, context->client_formats[i].nBlockAlign);
		Stream_Read_UINT16(s, context->client_formats[i].wBitsPerSample);
		Stream_Read_UINT16(s, context->client_formats[i].cbSize);

		if (context->client_formats[i].cbSize > 0)
		{
			if (!Stream_SafeSeek(s, context->client_formats[i].cbSize))
				goto out_free;
		}

		if (context->client_formats[i].wFormatTag != 0)
		{
			//lets call this a known format
			//TODO: actually look through our own list of known formats
			num_known_format++;
		}
	}

	if (!context->num_client_formats)
	{
		fprintf(stderr, "%s: client doesn't support any known format!\n", __FUNCTION__);
		goto out_free;
	}

	return TRUE;

out_free:
	free(context->client_formats);
	return FALSE;
}

static void* rdpsnd_server_thread(void* arg)
{
	wStream* s;
	DWORD status;
	DWORD nCount;
	void* buffer;
	BYTE msgType;
	UINT16 BodySize;
	HANDLE events[8];
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	RdpsndServerContext* context;
	BOOL doRun;

	context = (RdpsndServerContext *)arg;

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = context->priv->ChannelHandle;

	s = Stream_New(NULL, 4096);
	if (!s)
		return NULL;

	if (WTSVirtualChannelQuery(ChannelEvent, WTSVirtualEventHandle, &buffer, &BytesReturned))
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = ChannelEvent;
	events[nCount++] = context->priv->StopEvent;

	if (!rdpsnd_server_send_formats(context, s))
		goto out;

	doRun = TRUE;
	while (doRun)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(context->priv->StopEvent, 0) == WAIT_OBJECT_0)
			break;

		Stream_SetPosition(s, 0);

		if (!WTSVirtualChannelRead(ChannelEvent, 0, (PCHAR)Stream_Buffer(s),
									Stream_Capacity(s), &BytesReturned))
		{
			if (!BytesReturned)
				break;

			Stream_EnsureRemainingCapacity(s, BytesReturned);

			if (!WTSVirtualChannelRead(ChannelEvent, 0, (PCHAR)Stream_Buffer(s),
										Stream_Capacity(s), &BytesReturned))
				break;
		}

		if (Stream_GetRemainingLength(s) < 4)
			break;

		Stream_Read_UINT8(s, msgType);
		Stream_Seek_UINT8(s); /* bPad */
		Stream_Read_UINT16(s, BodySize);

		if (Stream_GetRemainingLength(s) < BodySize)
			break;

		switch (msgType)
		{
			case SNDC_WAVECONFIRM:
				doRun = rdpsnd_server_recv_waveconfirm(context, s);
				break;

			case SNDC_QUALITYMODE:
				doRun = rdpsnd_server_recv_quality_mode(context, s);
				break;

			case SNDC_FORMATS:
				doRun = rdpsnd_server_recv_formats(context, s);
				if (doRun)
				{
					IFCALL(context->Activated, context);
				}
				break;

			default:
				fprintf(stderr, "%s: UNKOWN MESSAGE TYPE!! (%#0X)\n\n", __FUNCTION__, msgType);
				break;
		}
	}

out:
	Stream_Free(s, TRUE);
	return NULL;
}

static BOOL rdpsnd_server_initialize(RdpsndServerContext* context)
{
	return context->Start(context) >= 0;
}

static BOOL rdpsnd_server_select_format(RdpsndServerContext* context, int client_format_index)
{
	int bs;
	int out_buffer_size;
	AUDIO_FORMAT *format;

	if (client_format_index < 0 || client_format_index >= context->num_client_formats)
	{
		fprintf(stderr, "%s: index %d is not correct.\n", __FUNCTION__, client_format_index);
		return FALSE;
	}
	
	context->priv->src_bytes_per_sample = context->src_format.wBitsPerSample / 8;
	context->priv->src_bytes_per_frame = context->priv->src_bytes_per_sample * context->src_format.nChannels;

	context->selected_client_format = client_format_index;
	format = &context->client_formats[client_format_index];
	
	if (format->nSamplesPerSec == 0)
	{
		fprintf(stderr, "%s: invalid Client Sound Format!!\n", __FUNCTION__);
		return FALSE;
	}

	switch(format->wFormatTag)
	{
		case WAVE_FORMAT_DVI_ADPCM:
			bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
			context->priv->out_frames = (format->nBlockAlign * 4 * format->nChannels * 2 / bs + 1) * bs / (format->nChannels * 2);
			break;

		case WAVE_FORMAT_ADPCM:
			bs = (format->nBlockAlign - 7 * format->nChannels) * 2 / format->nChannels + 2;
			context->priv->out_frames = bs * 4;
			break;
		default:
			context->priv->out_frames = 0x4000 / context->priv->src_bytes_per_frame;
			break;
	}

	if (format->nSamplesPerSec != context->src_format.nSamplesPerSec)
	{
		context->priv->out_frames = (context->priv->out_frames * context->src_format.nSamplesPerSec + format->nSamplesPerSec - 100) / format->nSamplesPerSec;
	}
	context->priv->out_pending_frames = 0;

	out_buffer_size = context->priv->out_frames * context->priv->src_bytes_per_frame;
	
	if (context->priv->out_buffer_size < out_buffer_size)
	{
		BYTE *newBuffer;

		newBuffer = (BYTE *)realloc(context->priv->out_buffer, out_buffer_size);
		if (!newBuffer)
			return FALSE;

		context->priv->out_buffer = newBuffer;
		context->priv->out_buffer_size = out_buffer_size;
	}

	freerdp_dsp_context_reset_adpcm(context->priv->dsp_context);
	return TRUE;
}

static BOOL rdpsnd_server_send_audio_pdu(RdpsndServerContext* context)
{
	int size;
	BYTE* src;
	int frames;
	int fill_size;
	BOOL status;
	AUDIO_FORMAT* format;
	int tbytes_per_frame;
	wStream* s = context->priv->rdpsnd_pdu;

	format = &context->client_formats[context->selected_client_format];
	tbytes_per_frame = format->nChannels * context->priv->src_bytes_per_sample;

	if ((format->nSamplesPerSec == context->src_format.nSamplesPerSec) &&
			(format->nChannels == context->src_format.nChannels))
	{
		src = context->priv->out_buffer;
		frames = context->priv->out_pending_frames;
	}
	else
	{
		context->priv->dsp_context->resample(context->priv->dsp_context, context->priv->out_buffer,
				context->priv->src_bytes_per_sample, context->src_format.nChannels,
				context->src_format.nSamplesPerSec, context->priv->out_pending_frames,
				format->nChannels, format->nSamplesPerSec);
		frames = context->priv->dsp_context->resampled_frames;
		src = context->priv->dsp_context->resampled_buffer;
	}
	size = frames * tbytes_per_frame;

	if (format->wFormatTag == WAVE_FORMAT_DVI_ADPCM)
	{
		context->priv->dsp_context->encode_ima_adpcm(context->priv->dsp_context,
			src, size, format->nChannels, format->nBlockAlign);
		src = context->priv->dsp_context->adpcm_buffer;
		size = context->priv->dsp_context->adpcm_size;
	}
	else if (format->wFormatTag == WAVE_FORMAT_ADPCM)
	{
		context->priv->dsp_context->encode_ms_adpcm(context->priv->dsp_context,
			src, size, format->nChannels, format->nBlockAlign);
		src = context->priv->dsp_context->adpcm_buffer;
		size = context->priv->dsp_context->adpcm_size;
	}

	context->block_no = (context->block_no + 1) % 256;

	/* Fill to nBlockAlign for the last audio packet */

	fill_size = 0;

	if ((format->wFormatTag == WAVE_FORMAT_DVI_ADPCM || format->wFormatTag == WAVE_FORMAT_ADPCM) &&
		(context->priv->out_pending_frames < context->priv->out_frames) && ((size % format->nBlockAlign) != 0))
	{
		fill_size = format->nBlockAlign - (size % format->nBlockAlign);
	}

	/* WaveInfo PDU */
	Stream_SetPosition(s, 0);
	Stream_Write_UINT8(s, SNDC_WAVE); /* msgType */
	Stream_Write_UINT8(s, 0); /* bPad */
	Stream_Write_UINT16(s, size + fill_size + 8); /* BodySize */

	Stream_Write_UINT16(s, 0); /* wTimeStamp */
	Stream_Write_UINT16(s, context->selected_client_format); /* wFormatNo */
	Stream_Write_UINT8(s, context->block_no); /* cBlockNo */
	Stream_Seek(s, 3); /* bPad */
	Stream_Write(s, src, 4);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);
	if (!status)
		goto out;
	Stream_SetPosition(s, 0);

	/* Wave PDU */
	Stream_EnsureRemainingCapacity(s, size + fill_size);
	Stream_Write_UINT32(s, 0); /* bPad */
	Stream_Write(s, src + 4, size - 4);

	if (fill_size > 0)
		Stream_Zero(s, fill_size);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);

out:
	Stream_SetPosition(s, 0);
	context->priv->out_pending_frames = 0;
	return status;
}

static BOOL rdpsnd_server_send_samples(RdpsndServerContext* context, const void* buf, int nframes)
{
	int cframes;
	int cframesize;

	if (context->selected_client_format < 0)
		return FALSE;

	while (nframes > 0)
	{
		cframes = MIN(nframes, context->priv->out_frames - context->priv->out_pending_frames);
		cframesize = cframes * context->priv->src_bytes_per_frame;

		CopyMemory(context->priv->out_buffer +
				(context->priv->out_pending_frames * context->priv->src_bytes_per_frame), buf, cframesize);
		buf = (BYTE*) buf + cframesize;
		nframes -= cframes;
		context->priv->out_pending_frames += cframes;

		if (context->priv->out_pending_frames >= context->priv->out_frames)
		{
			if (!rdpsnd_server_send_audio_pdu(context))
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL rdpsnd_server_set_volume(RdpsndServerContext* context, int left, int right)
{
	int pos;
	BOOL status;
	wStream* s = context->priv->rdpsnd_pdu;

	Stream_Write_UINT8(s, SNDC_SETVOLUME);
	Stream_Write_UINT8(s, 0);
	Stream_Seek_UINT16(s);

	Stream_Write_UINT16(s, left);
	Stream_Write_UINT16(s, right);

	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, pos - 4);
	Stream_SetPosition(s, pos);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);
	Stream_SetPosition(s, 0);

	return status;
}

static BOOL rdpsnd_server_close(RdpsndServerContext* context)
{
	int pos;
	BOOL status;
	wStream* s = context->priv->rdpsnd_pdu;

	if (context->selected_client_format < 0)
		return FALSE;

	if (context->priv->out_pending_frames > 0)
	{
		if (!rdpsnd_server_send_audio_pdu(context))
			return FALSE;
	}

	context->selected_client_format = -1;

	Stream_Write_UINT8(s, SNDC_CLOSE);
	Stream_Write_UINT8(s, 0);
	Stream_Seek_UINT16(s);

	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, pos - 4);
	Stream_SetPosition(s, pos);
	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);
	Stream_SetPosition(s, 0);

	return status;
}

static int rdpsnd_server_start(RdpsndServerContext* context)
{
	context->priv->ChannelHandle = WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, "rdpsnd");
	if (!context->priv->ChannelHandle)
		return -1;

	context->priv->rdpsnd_pdu = Stream_New(NULL, 4096);
	if (!context->priv->rdpsnd_pdu)
		goto out_close;

	context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!context->priv->StopEvent)
		goto out_pdu;

	context->priv->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rdpsnd_server_thread, (void*) context, 0, NULL);
	if (!context->priv->Thread)
		goto out_stopEvent;

	return 0;

out_stopEvent:
	CloseHandle(context->priv->StopEvent);
	context->priv->StopEvent = NULL;
out_pdu:
	Stream_Free(context->priv->rdpsnd_pdu, TRUE);
	context->priv->rdpsnd_pdu = NULL;
out_close:
	WTSVirtualChannelClose(context->priv->ChannelHandle);
	context->priv->ChannelHandle = NULL;
	return -1;
}

static int rdpsnd_server_stop(RdpsndServerContext* context)
{
	if (context->priv->StopEvent)
	{
		SetEvent(context->priv->StopEvent);

		WaitForSingleObject(context->priv->Thread, INFINITE);
		CloseHandle(context->priv->Thread);
	}

	return 0;
}

RdpsndServerContext* rdpsnd_server_context_new(HANDLE vcm)
{
	RdpsndServerContext* context;

	context = (RdpsndServerContext*) calloc(1, sizeof(RdpsndServerContext));
	if (!context)
		return NULL;

	context->vcm = vcm;

	context->Start = rdpsnd_server_start;
	context->Stop = rdpsnd_server_stop;

	context->selected_client_format = -1;
	context->Initialize = rdpsnd_server_initialize;
	context->SelectFormat = rdpsnd_server_select_format;
	context->SendSamples = rdpsnd_server_send_samples;
	context->SetVolume = rdpsnd_server_set_volume;
	context->Close = rdpsnd_server_close;

	context->priv = (RdpsndServerPrivate*) calloc(1, sizeof(RdpsndServerPrivate));
	if (!context->priv)
		goto out_free;

	context->priv->dsp_context = freerdp_dsp_context_new();
	if (!context->priv->dsp_context)
		goto out_free_priv;

	return context;

out_free_priv:
	free(context->priv);
out_free:
	free(context);
	return NULL;
}

void rdpsnd_server_context_free(RdpsndServerContext* context)
{
	if (!context->priv->StopEvent)
	{
		SetEvent(context->priv->StopEvent);
		WaitForSingleObject(context->priv->Thread, INFINITE);
	}

	if (context->priv->ChannelHandle)
		WTSVirtualChannelClose(context->priv->ChannelHandle);

	if (context->priv->rdpsnd_pdu)
		Stream_Free(context->priv->rdpsnd_pdu, TRUE);

	if (context->priv->out_buffer)
		free(context->priv->out_buffer);

	if (context->priv->dsp_context)
		freerdp_dsp_context_free(context->priv->dsp_context);

	if (context->client_formats)
		free(context->client_formats);

	free(context);
}
