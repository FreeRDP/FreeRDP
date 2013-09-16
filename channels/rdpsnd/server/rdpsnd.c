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
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include <freerdp/codec/dsp.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/server/rdpsnd.h>

typedef struct _rdpsnd_server
{
	rdpsnd_server_context context;

	HANDLE thread;
	HANDLE StopEvent;
	void* rdpsnd_channel;
	wStream* rdpsnd_pdu;

	FREERDP_DSP_CONTEXT* dsp_context;
	BYTE* out_buffer;
	int out_buffer_size;
	int out_frames;
	int out_pending_frames;

	UINT32 src_bytes_per_sample;
	UINT32 src_bytes_per_frame;
} rdpsnd_server;


static BOOL rdpsnd_server_send_formats(rdpsnd_server* rdpsnd, wStream* s)
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
	Stream_Write_UINT16(s, rdpsnd->context.num_server_formats); /* wNumberOfFormats */
	Stream_Write_UINT8(s, rdpsnd->context.block_no); /* cLastBlockConfirmed */
	Stream_Write_UINT16(s, 0x06); /* wVersion */
	Stream_Write_UINT8(s, 0); /* bPad */
	
	for (i = 0; i < rdpsnd->context.num_server_formats; i++)
	{
		Stream_Write_UINT16(s, rdpsnd->context.server_formats[i].wFormatTag); /* wFormatTag (WAVE_FORMAT_PCM) */
		Stream_Write_UINT16(s, rdpsnd->context.server_formats[i].nChannels); /* nChannels */
		Stream_Write_UINT32(s, rdpsnd->context.server_formats[i].nSamplesPerSec); /* nSamplesPerSec */

		Stream_Write_UINT32(s, rdpsnd->context.server_formats[i].nSamplesPerSec *
			rdpsnd->context.server_formats[i].nChannels *
			rdpsnd->context.server_formats[i].wBitsPerSample / 8); /* nAvgBytesPerSec */

		Stream_Write_UINT16(s, rdpsnd->context.server_formats[i].nBlockAlign); /* nBlockAlign */
		Stream_Write_UINT16(s, rdpsnd->context.server_formats[i].wBitsPerSample); /* wBitsPerSample */
		Stream_Write_UINT16(s, rdpsnd->context.server_formats[i].cbSize); /* cbSize */

		if (rdpsnd->context.server_formats[i].cbSize > 0)
		{
			Stream_Write(s, rdpsnd->context.server_formats[i].data, rdpsnd->context.server_formats[i].cbSize);
		}
	}

	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, pos - 4);
	Stream_SetPosition(s, pos);
	status = WTSVirtualChannelWrite(rdpsnd->rdpsnd_channel, Stream_Buffer(s), Stream_GetPosition(s), NULL);
	Stream_SetPosition(s, 0);

	return status;
}

static void rdpsnd_server_recv_waveconfirm(rdpsnd_server* rdpsnd, wStream* s)
{
	//unhandled for now
	
	UINT16 timestamp = 0;
	BYTE confirmBlockNum = 0;
	Stream_Read_UINT16(s, timestamp);
	Stream_Read_UINT8(s, confirmBlockNum);
	Stream_Seek_UINT8(s); // padding
}

static void rdpsnd_server_recv_quality_mode(rdpsnd_server* rdpsnd, wStream* s)
{
	//unhandled for now
	UINT16 quality;
	
	Stream_Read_UINT16(s, quality);
	Stream_Seek_UINT16(s); // reserved
	
	fprintf(stderr, "Client requested sound quality: %#0X\n", quality);
}

static BOOL rdpsnd_server_recv_formats(rdpsnd_server* rdpsnd, wStream* s)
{
	int i, num_known_format = 0;
	UINT32 flags, vol, pitch;
	UINT16 udpPort, version;
	BYTE lastblock;
		

	Stream_Read_UINT32(s, flags); /* dwFlags */
	Stream_Read_UINT32(s, vol); /* dwVolume */
	Stream_Read_UINT32(s, pitch); /* dwPitch */
	Stream_Read_UINT16(s, udpPort); /* wDGramPort */
	Stream_Read_UINT16(s, rdpsnd->context.num_client_formats); /* wNumberOfFormats */
	Stream_Read_UINT8(s, lastblock); /* cLastBlockConfirmed */
	Stream_Read_UINT16(s, version); /* wVersion */
	Stream_Seek_UINT8(s); /* bPad */

	if (rdpsnd->context.num_client_formats > 0)
	{
		rdpsnd->context.client_formats = (AUDIO_FORMAT*) malloc(rdpsnd->context.num_client_formats * sizeof(AUDIO_FORMAT));
		ZeroMemory(rdpsnd->context.client_formats, sizeof(AUDIO_FORMAT));

		for (i = 0; i < rdpsnd->context.num_client_formats; i++)
		{
			Stream_Read_UINT16(s, rdpsnd->context.client_formats[i].wFormatTag);
			Stream_Read_UINT16(s, rdpsnd->context.client_formats[i].nChannels);
			Stream_Read_UINT32(s, rdpsnd->context.client_formats[i].nSamplesPerSec);
			Stream_Read_UINT32(s, rdpsnd->context.client_formats[i].nAvgBytesPerSec);
			Stream_Read_UINT16(s, rdpsnd->context.client_formats[i].nBlockAlign);
			Stream_Read_UINT16(s, rdpsnd->context.client_formats[i].wBitsPerSample);
			Stream_Read_UINT16(s, rdpsnd->context.client_formats[i].cbSize);

			if (rdpsnd->context.client_formats[i].cbSize > 0)
			{
				Stream_Seek(s, rdpsnd->context.client_formats[i].cbSize);
			}
			
			if (rdpsnd->context.client_formats[i].wFormatTag != 0)
			{
				//lets call this a known format
				//TODO: actually look through our own list of known formats
				num_known_format++;
			}
		}
	}
	
	if (num_known_format == 0)
	{
		fprintf(stderr, "Client doesnt support any known formats!\n");
		return FALSE;
	}

	return TRUE;
}

static void* rdpsnd_server_thread_func(void* arg)
{
	void* fd;
	wStream* s;
	void* buffer;
	DWORD status;
	BYTE msgType;
	UINT16 BodySize;
	HANDLE events[2];
	UINT32 bytes_returned = 0;
	rdpsnd_server* rdpsnd = (rdpsnd_server*) arg;

	events[0] = rdpsnd->StopEvent;

	if (WTSVirtualChannelQuery(rdpsnd->rdpsnd_channel, WTSVirtualFileHandle, &buffer, &bytes_returned) == TRUE)
	{
		fd = *((void**) buffer);
		WTSFreeMemory(buffer);

		events[1] = CreateWaitObjectEvent(NULL, TRUE, FALSE, fd);
	}

	s = Stream_New(NULL, 4096);

	rdpsnd_server_send_formats(rdpsnd, s);

	while (1)
	{
		status = WaitForMultipleObjects(2, events, FALSE, INFINITE);

		if (WaitForSingleObject(rdpsnd->StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		Stream_SetPosition(s, 0);

		if (WTSVirtualChannelRead(rdpsnd->rdpsnd_channel, 0, Stream_Buffer(s),
			Stream_Capacity(s), &bytes_returned) == FALSE)
		{
			if (bytes_returned == 0)
				break;
			
			Stream_EnsureRemainingCapacity(s, (int) bytes_returned);

			if (WTSVirtualChannelRead(rdpsnd->rdpsnd_channel, 0, Stream_Buffer(s),
				Stream_Capacity(s), &bytes_returned) == FALSE)
				break;
		}
		
		Stream_Read_UINT8(s, msgType);
		Stream_Seek_UINT8(s); /* bPad */
		Stream_Read_UINT16(s, BodySize);

		switch (msgType)
		{
			case SNDC_WAVECONFIRM:
				rdpsnd_server_recv_waveconfirm(rdpsnd, s);
				break;
				
			case SNDC_QUALITYMODE:
				rdpsnd_server_recv_quality_mode(rdpsnd, s);
				break;
			case SNDC_FORMATS:
				if (rdpsnd_server_recv_formats(rdpsnd, s))
				{
					IFCALL(rdpsnd->context.Activated, &rdpsnd->context);
				}
				break;
			default:
				fprintf(stderr, "UNKOWN MESSAGE TYPE!! (%#0X)\n\n", msgType);
				break;
		}
	}
	
	Stream_Free(s, TRUE);

	return NULL;
}

static BOOL rdpsnd_server_initialize(rdpsnd_server_context* context)
{
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;

	rdpsnd->rdpsnd_channel = WTSVirtualChannelOpenEx(context->vcm, "rdpsnd", 0);

	if (rdpsnd->rdpsnd_channel != NULL)
	{
		rdpsnd->rdpsnd_pdu = Stream_New(NULL, 4096);

		rdpsnd->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		rdpsnd->thread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) rdpsnd_server_thread_func, (void*) rdpsnd, 0, NULL);

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static void rdpsnd_server_select_format(rdpsnd_server_context* context, int client_format_index)
{
	int bs;
	int out_buffer_size;
	AUDIO_FORMAT *format;
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;

	if (client_format_index < 0 || client_format_index >= context->num_client_formats)
	{
		fprintf(stderr, "rdpsnd_server_select_format: index %d is not correct.\n", client_format_index);
		return;
	}
	
	rdpsnd->src_bytes_per_sample = context->src_format.wBitsPerSample / 8;
	rdpsnd->src_bytes_per_frame = rdpsnd->src_bytes_per_sample * context->src_format.nChannels;

	context->selected_client_format = client_format_index;
	format = &context->client_formats[client_format_index];
	
	if (format->nSamplesPerSec == 0)
	{
		fprintf(stderr, "Invalid Client Sound Format!!\n\n");
		return;
	}

	if (format->wFormatTag == WAVE_FORMAT_DVI_ADPCM)
	{
		bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
		rdpsnd->out_frames = (format->nBlockAlign * 4 * format->nChannels * 2 / bs + 1) * bs / (format->nChannels * 2);
	}
	else if (format->wFormatTag == WAVE_FORMAT_ADPCM)
	{
		bs = (format->nBlockAlign - 7 * format->nChannels) * 2 / format->nChannels + 2;
		rdpsnd->out_frames = bs * 4;
	}
	else
	{
		rdpsnd->out_frames = 0x4000 / rdpsnd->src_bytes_per_frame;
	}

	if (format->nSamplesPerSec != context->src_format.nSamplesPerSec)
	{
		rdpsnd->out_frames = (rdpsnd->out_frames * context->src_format.nSamplesPerSec + format->nSamplesPerSec - 100) / format->nSamplesPerSec;
	}
	rdpsnd->out_pending_frames = 0;

	out_buffer_size = rdpsnd->out_frames * rdpsnd->src_bytes_per_frame;
	
	if (rdpsnd->out_buffer_size < out_buffer_size)
	{
		rdpsnd->out_buffer = (BYTE*) realloc(rdpsnd->out_buffer, out_buffer_size);
		rdpsnd->out_buffer_size = out_buffer_size;
	}

	freerdp_dsp_context_reset_adpcm(rdpsnd->dsp_context);
}

static BOOL rdpsnd_server_send_audio_pdu(rdpsnd_server* rdpsnd)
{
	int size;
	BYTE* src;
	int frames;
	int fill_size;
	BOOL status;
	AUDIO_FORMAT* format;
	int tbytes_per_frame;
	wStream* s = rdpsnd->rdpsnd_pdu;

	format = &rdpsnd->context.client_formats[rdpsnd->context.selected_client_format];
	tbytes_per_frame = format->nChannels * rdpsnd->src_bytes_per_sample;

	if ((format->nSamplesPerSec == rdpsnd->context.src_format.nSamplesPerSec) &&
			(format->nChannels == rdpsnd->context.src_format.nChannels))
	{
		src = rdpsnd->out_buffer;
		frames = rdpsnd->out_pending_frames;
	}
	else
	{
		rdpsnd->dsp_context->resample(rdpsnd->dsp_context, rdpsnd->out_buffer, rdpsnd->src_bytes_per_sample,
			rdpsnd->context.src_format.nChannels, rdpsnd->context.src_format.nSamplesPerSec, rdpsnd->out_pending_frames,
			format->nChannels, format->nSamplesPerSec);
		frames = rdpsnd->dsp_context->resampled_frames;
		src = rdpsnd->dsp_context->resampled_buffer;
	}
	size = frames * tbytes_per_frame;

	if (format->wFormatTag == WAVE_FORMAT_DVI_ADPCM)
	{
		rdpsnd->dsp_context->encode_ima_adpcm(rdpsnd->dsp_context,
			src, size, format->nChannels, format->nBlockAlign);
		src = rdpsnd->dsp_context->adpcm_buffer;
		size = rdpsnd->dsp_context->adpcm_size;
	}
	else if (format->wFormatTag == WAVE_FORMAT_ADPCM)
	{
		rdpsnd->dsp_context->encode_ms_adpcm(rdpsnd->dsp_context,
			src, size, format->nChannels, format->nBlockAlign);
		src = rdpsnd->dsp_context->adpcm_buffer;
		size = rdpsnd->dsp_context->adpcm_size;
	}

	rdpsnd->context.block_no = (rdpsnd->context.block_no + 1) % 256;

	/* Fill to nBlockAlign for the last audio packet */

	fill_size = 0;

	if ((format->wFormatTag == WAVE_FORMAT_DVI_ADPCM || format->wFormatTag == WAVE_FORMAT_ADPCM) &&
		(rdpsnd->out_pending_frames < rdpsnd->out_frames) && ((size % format->nBlockAlign) != 0))
	{
		fill_size = format->nBlockAlign - (size % format->nBlockAlign);
	}

	/* WaveInfo PDU */
	Stream_SetPosition(s, 0);
	Stream_Write_UINT8(s, SNDC_WAVE); /* msgType */
	Stream_Write_UINT8(s, 0); /* bPad */
	Stream_Write_UINT16(s, size + fill_size + 8); /* BodySize */

	Stream_Write_UINT16(s, 0); /* wTimeStamp */
	Stream_Write_UINT16(s, rdpsnd->context.selected_client_format); /* wFormatNo */
	Stream_Write_UINT8(s, rdpsnd->context.block_no); /* cBlockNo */
	Stream_Seek(s, 3); /* bPad */
	Stream_Write(s, src, 4);

	WTSVirtualChannelWrite(rdpsnd->rdpsnd_channel, Stream_Buffer(s), Stream_GetPosition(s), NULL);
	Stream_SetPosition(s, 0);

	/* Wave PDU */
	Stream_EnsureRemainingCapacity(s, size + fill_size);
	Stream_Write_UINT32(s, 0); /* bPad */
	Stream_Write(s, src + 4, size - 4);

	if (fill_size > 0)
		Stream_Zero(s, fill_size);

	status = WTSVirtualChannelWrite(rdpsnd->rdpsnd_channel, Stream_Buffer(s), Stream_GetPosition(s), NULL);
	Stream_SetPosition(s, 0);

	rdpsnd->out_pending_frames = 0;

	return status;
}

static BOOL rdpsnd_server_send_samples(rdpsnd_server_context* context, const void* buf, int nframes)
{
	int cframes;
	int cframesize;
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;

	if (rdpsnd->context.selected_client_format < 0)
		return FALSE;

	while (nframes > 0)
	{
		cframes = MIN(nframes, rdpsnd->out_frames - rdpsnd->out_pending_frames);
		cframesize = cframes * rdpsnd->src_bytes_per_frame;

		CopyMemory(rdpsnd->out_buffer + (rdpsnd->out_pending_frames * rdpsnd->src_bytes_per_frame), buf, cframesize);
		buf = (BYTE*) buf + cframesize;
		nframes -= cframes;
		rdpsnd->out_pending_frames += cframes;

		if (rdpsnd->out_pending_frames >= rdpsnd->out_frames)
		{
			if (!rdpsnd_server_send_audio_pdu(rdpsnd))
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL rdpsnd_server_set_volume(rdpsnd_server_context* context, int left, int right)
{
	int pos;
	BOOL status;
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;
	wStream* s = rdpsnd->rdpsnd_pdu;

	Stream_Write_UINT8(s, SNDC_SETVOLUME);
	Stream_Write_UINT8(s, 0);
	Stream_Seek_UINT16(s);

	Stream_Write_UINT16(s, left);
	Stream_Write_UINT16(s, right);

	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, pos - 4);
	Stream_SetPosition(s, pos);
	status = WTSVirtualChannelWrite(rdpsnd->rdpsnd_channel, Stream_Buffer(s), Stream_GetPosition(s), NULL);
	Stream_SetPosition(s, 0);

	return status;
}

static BOOL rdpsnd_server_close(rdpsnd_server_context* context)
{
	int pos;
	BOOL status;
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;
	wStream* s = rdpsnd->rdpsnd_pdu;

	if (rdpsnd->context.selected_client_format < 0)
		return FALSE;

	if (rdpsnd->out_pending_frames > 0)
	{
		if (!rdpsnd_server_send_audio_pdu(rdpsnd))
			return FALSE;
	}

	rdpsnd->context.selected_client_format = -1;

	Stream_Write_UINT8(s, SNDC_CLOSE);
	Stream_Write_UINT8(s, 0);
	Stream_Seek_UINT16(s);

	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, 2);
	Stream_Write_UINT16(s, pos - 4);
	Stream_SetPosition(s, pos);
	status = WTSVirtualChannelWrite(rdpsnd->rdpsnd_channel, Stream_Buffer(s), Stream_GetPosition(s), NULL);
	Stream_SetPosition(s, 0);

	return status;
}

rdpsnd_server_context* rdpsnd_server_context_new(WTSVirtualChannelManager* vcm)
{
	rdpsnd_server* rdpsnd;

	rdpsnd = (rdpsnd_server*) malloc(sizeof(rdpsnd_server));
	ZeroMemory(rdpsnd, sizeof(rdpsnd_server));

	rdpsnd->context.vcm = vcm;
	rdpsnd->context.selected_client_format = -1;
	rdpsnd->context.Initialize = rdpsnd_server_initialize;
	rdpsnd->context.SelectFormat = rdpsnd_server_select_format;
	rdpsnd->context.SendSamples = rdpsnd_server_send_samples;
	rdpsnd->context.SetVolume = rdpsnd_server_set_volume;
	rdpsnd->context.Close = rdpsnd_server_close;

	rdpsnd->dsp_context = freerdp_dsp_context_new();

	return (rdpsnd_server_context*) rdpsnd;
}

void rdpsnd_server_context_free(rdpsnd_server_context* context)
{
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;

	SetEvent(rdpsnd->StopEvent);
	WaitForSingleObject(rdpsnd->thread, INFINITE);

	CloseHandle(rdpsnd->StopEvent);
	CloseHandle(rdpsnd->thread);

	if (rdpsnd->rdpsnd_channel)
		WTSVirtualChannelClose(rdpsnd->rdpsnd_channel);

	if (rdpsnd->rdpsnd_pdu)
		Stream_Free(rdpsnd->rdpsnd_pdu, TRUE);

	if (rdpsnd->out_buffer)
		free(rdpsnd->out_buffer);

	if (rdpsnd->dsp_context)
		freerdp_dsp_context_free(rdpsnd->dsp_context);

	if (rdpsnd->context.client_formats)
		free(rdpsnd->context.client_formats);

	free(rdpsnd);
}
