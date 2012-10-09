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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/dsp.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/server/rdpsnd.h>

typedef struct _rdpsnd_server
{
	rdpsnd_server_context context;

	void* rdpsnd_channel;
	freerdp_thread* rdpsnd_channel_thread;
	STREAM* rdpsnd_pdu;

	FREERDP_DSP_CONTEXT* dsp_context;
	BYTE* out_buffer;
	int out_buffer_size;
	int out_frames;
	int out_pending_frames;

	uint32 src_bytes_per_sample;
	uint32 src_bytes_per_frame;
} rdpsnd_server;

#define RDPSND_PDU_INIT(_s, _msgType) \
{ \
	stream_write_BYTE(_s, _msgType); \
	stream_write_BYTE(_s, 0); \
	stream_seek_UINT16(_s); \
}

#define RDPSND_PDU_FINISH(_s) \
{ \
	BOOL _r; \
	int _pos; \
	_pos = stream_get_pos(_s); \
	stream_set_pos(_s, 2); \
	stream_write_UINT16(_s, _pos - 4); \
	stream_set_pos(_s, _pos); \
	_r = WTSVirtualChannelWrite(rdpsnd->rdpsnd_channel, stream_get_head(_s), stream_get_length(_s), NULL); \
	stream_set_pos(_s, 0); \
	return _r; \
}

static BOOL rdpsnd_server_send_formats(rdpsnd_server* rdpsnd, STREAM* s)
{
	UINT16 i;

	RDPSND_PDU_INIT(s, SNDC_FORMATS);

	stream_write_uint32(s, 0); /* dwFlags */
	stream_write_uint32(s, 0); /* dwVolume */
	stream_write_uint32(s, 0); /* dwPitch */
	stream_write_UINT16(s, 0); /* wDGramPort */
	stream_write_UINT16(s, rdpsnd->context.num_server_formats); /* wNumberOfFormats */
	stream_write_BYTE(s, rdpsnd->context.block_no); /* cLastBlockConfirmed */
	stream_write_UINT16(s, 0x06); /* wVersion */
	stream_write_BYTE(s, 0); /* bPad */

	for (i = 0; i < rdpsnd->context.num_server_formats; i++)
	{
		stream_write_UINT16(s, rdpsnd->context.server_formats[i].wFormatTag); /* wFormatTag (WAVE_FORMAT_PCM) */
		stream_write_UINT16(s, rdpsnd->context.server_formats[i].nChannels); /* nChannels */
		stream_write_uint32(s, rdpsnd->context.server_formats[i].nSamplesPerSec); /* nSamplesPerSec */

		stream_write_uint32(s, rdpsnd->context.server_formats[i].nSamplesPerSec *
			rdpsnd->context.server_formats[i].nChannels *
			rdpsnd->context.server_formats[i].wBitsPerSample / 8); /* nAvgBytesPerSec */

		stream_write_UINT16(s, rdpsnd->context.server_formats[i].nBlockAlign); /* nBlockAlign */
		stream_write_UINT16(s, rdpsnd->context.server_formats[i].wBitsPerSample); /* wBitsPerSample */
		stream_write_UINT16(s, rdpsnd->context.server_formats[i].cbSize); /* cbSize */

		if (rdpsnd->context.server_formats[i].cbSize > 0)
		{
			stream_write(s, rdpsnd->context.server_formats[i].data, rdpsnd->context.server_formats[i].cbSize);
		}
	}

	RDPSND_PDU_FINISH(s);
}

static BOOL rdpsnd_server_recv_formats(rdpsnd_server* rdpsnd, STREAM* s)
{
	int i;

	if (stream_get_left(s) < 20)
		return FALSE;

	stream_seek_uint32(s); /* dwFlags */
	stream_seek_uint32(s); /* dwVolume */
	stream_seek_uint32(s); /* dwPitch */
	stream_seek_UINT16(s); /* wDGramPort */
	stream_read_UINT16(s, rdpsnd->context.num_client_formats); /* wNumberOfFormats */
	stream_seek_BYTE(s); /* cLastBlockConfirmed */
	stream_seek_UINT16(s); /* wVersion */
	stream_seek_BYTE(s); /* bPad */

	if (rdpsnd->context.num_client_formats > 0)
	{
		rdpsnd->context.client_formats = xzalloc(rdpsnd->context.num_client_formats * sizeof(rdpsndFormat));

		for (i = 0; i < rdpsnd->context.num_client_formats; i++)
		{
			if (stream_get_left(s) < 18)
			{
				free(rdpsnd->context.client_formats);
				rdpsnd->context.client_formats = NULL;
				return FALSE;
			}

			stream_read_UINT16(s, rdpsnd->context.client_formats[i].wFormatTag);
			stream_read_UINT16(s, rdpsnd->context.client_formats[i].nChannels);
			stream_read_uint32(s, rdpsnd->context.client_formats[i].nSamplesPerSec);
			stream_seek_uint32(s); /* nAvgBytesPerSec */
			stream_read_UINT16(s, rdpsnd->context.client_formats[i].nBlockAlign);
			stream_read_UINT16(s, rdpsnd->context.client_formats[i].wBitsPerSample);
			stream_read_UINT16(s, rdpsnd->context.client_formats[i].cbSize);

			if (rdpsnd->context.client_formats[i].cbSize > 0)
			{
				stream_seek(s, rdpsnd->context.client_formats[i].cbSize);
			}
		}
	}

	return TRUE;
}

static void* rdpsnd_server_thread_func(void* arg)
{
	void* fd;
	STREAM* s;
	void* buffer;
	BYTE msgType;
	UINT16 BodySize;
	uint32 bytes_returned = 0;
	rdpsnd_server* rdpsnd = (rdpsnd_server*) arg;
	freerdp_thread* thread = rdpsnd->rdpsnd_channel_thread;

	if (WTSVirtualChannelQuery(rdpsnd->rdpsnd_channel, WTSVirtualFileHandle, &buffer, &bytes_returned) == TRUE)
	{
		fd = *((void**)buffer);
		WTSFreeMemory(buffer);
		thread->signals[thread->num_signals++] = wait_obj_new_with_fd(fd);
	}

	s = stream_new(4096);

	rdpsnd_server_send_formats(rdpsnd, s);

	while (1)
	{
		freerdp_thread_wait(thread);
		
		if (freerdp_thread_is_stopped(thread))
			break;

		stream_set_pos(s, 0);

		if (WTSVirtualChannelRead(rdpsnd->rdpsnd_channel, 0, stream_get_head(s),
			stream_get_size(s), &bytes_returned) == FALSE)
		{
			if (bytes_returned == 0)
				break;
			
			stream_check_size(s, (int) bytes_returned);

			if (WTSVirtualChannelRead(rdpsnd->rdpsnd_channel, 0, stream_get_head(s),
				stream_get_size(s), &bytes_returned) == FALSE)
				break;
		}

		stream_read_BYTE(s, msgType);
		stream_seek_BYTE(s); /* bPad */
		stream_read_UINT16(s, BodySize);

		if (BodySize + 4 > (int) bytes_returned)
			continue;

		switch (msgType)
		{
			case SNDC_FORMATS:
				if (rdpsnd_server_recv_formats(rdpsnd, s))
				{
					IFCALL(rdpsnd->context.Activated, &rdpsnd->context);
				}
				break;
			default:
				break;
		}
	}

	stream_free(s);
	freerdp_thread_quit(thread);

	return 0;
}

static BOOL rdpsnd_server_initialize(rdpsnd_server_context* context)
{
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;

	rdpsnd->rdpsnd_channel = WTSVirtualChannelOpenEx(context->vcm, "rdpsnd", 0);

	if (rdpsnd->rdpsnd_channel != NULL)
	{
		rdpsnd->rdpsnd_pdu = stream_new(4096);
		rdpsnd->rdpsnd_channel_thread = freerdp_thread_new();
		freerdp_thread_start(rdpsnd->rdpsnd_channel_thread, rdpsnd_server_thread_func, rdpsnd);

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
	rdpsndFormat *format;
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;

	if (client_format_index < 0 || client_format_index >= context->num_client_formats)
	{
		printf("rdpsnd_server_select_format: index %d is not correct.\n", client_format_index);
		return;
	}

	rdpsnd->src_bytes_per_sample = context->src_format.wBitsPerSample / 8;
	rdpsnd->src_bytes_per_frame = rdpsnd->src_bytes_per_sample * context->src_format.nChannels;

	context->selected_client_format = client_format_index;
	format = &context->client_formats[client_format_index];

	if (format->wFormatTag == 0x11)
	{
		bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
		rdpsnd->out_frames = (format->nBlockAlign * 4 * format->nChannels * 2 / bs + 1) * bs / (format->nChannels * 2);
	}
	else if (format->wFormatTag == 0x02)
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
		rdpsnd->out_buffer = realloc(rdpsnd->out_buffer, out_buffer_size);
		rdpsnd->out_buffer_size = out_buffer_size;
	}

	freerdp_dsp_context_reset_adpcm(rdpsnd->dsp_context);
}

static BOOL rdpsnd_server_send_audio_pdu(rdpsnd_server* rdpsnd)
{
	int size;
	BOOL r;
	BYTE* src;
	int frames;
	int fill_size;
	rdpsndFormat* format;
	int tbytes_per_frame;
	STREAM* s = rdpsnd->rdpsnd_pdu;

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

	if (format->wFormatTag == 0x11)
	{
		rdpsnd->dsp_context->encode_ima_adpcm(rdpsnd->dsp_context,
			src, size, format->nChannels, format->nBlockAlign);
		src = rdpsnd->dsp_context->adpcm_buffer;
		size = rdpsnd->dsp_context->adpcm_size;
	}
	else if (format->wFormatTag == 0x02)
	{
		rdpsnd->dsp_context->encode_ms_adpcm(rdpsnd->dsp_context,
			src, size, format->nChannels, format->nBlockAlign);
		src = rdpsnd->dsp_context->adpcm_buffer;
		size = rdpsnd->dsp_context->adpcm_size;
	}

	rdpsnd->context.block_no = (rdpsnd->context.block_no + 1) % 256;

	/* Fill to nBlockAlign for the last audio packet */
	if ((format->wFormatTag == 0x11 || format->wFormatTag == 0x02) &&
		rdpsnd->out_pending_frames < rdpsnd->out_frames && (size % format->nBlockAlign) != 0)
		fill_size = format->nBlockAlign - (size % format->nBlockAlign);
	else
		fill_size = 0;

	/* WaveInfo PDU */
	stream_set_pos(s, 0);
	stream_write_BYTE(s, SNDC_WAVE); /* msgType */
	stream_write_BYTE(s, 0); /* bPad */
	stream_write_UINT16(s, size + fill_size + 8); /* BodySize */

	stream_write_UINT16(s, 0); /* wTimeStamp */
	stream_write_UINT16(s, rdpsnd->context.selected_client_format); /* wFormatNo */
	stream_write_BYTE(s, rdpsnd->context.block_no); /* cBlockNo */
	stream_seek(s, 3); /* bPad */
	stream_write(s, src, 4);

	WTSVirtualChannelWrite(rdpsnd->rdpsnd_channel, stream_get_head(s), stream_get_length(s), NULL);
	stream_set_pos(s, 0);

	/* Wave PDU */
	stream_check_size(s, size + fill_size);
	stream_write_uint32(s, 0); /* bPad */
	stream_write(s, src + 4, size - 4);

	if (fill_size > 0)
		stream_write_zero(s, fill_size);

	r = WTSVirtualChannelWrite(rdpsnd->rdpsnd_channel, stream_get_head(s), stream_get_length(s), NULL);
	stream_set_pos(s, 0);

	rdpsnd->out_pending_frames = 0;

	return r;
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

		memcpy(rdpsnd->out_buffer + (rdpsnd->out_pending_frames * rdpsnd->src_bytes_per_frame), buf, cframesize);
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
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;
	STREAM* s = rdpsnd->rdpsnd_pdu;

	RDPSND_PDU_INIT(s, SNDC_SETVOLUME);

	stream_write_UINT16(s, left);
	stream_write_UINT16(s, right);
	
	RDPSND_PDU_FINISH(s);
}

static BOOL rdpsnd_server_close(rdpsnd_server_context* context)
{
	rdpsnd_server* rdpsnd = (rdpsnd_server*) context;
	STREAM* s = rdpsnd->rdpsnd_pdu;

	if (rdpsnd->context.selected_client_format < 0)
		return FALSE;

	if (rdpsnd->out_pending_frames > 0)
	{
		if (!rdpsnd_server_send_audio_pdu(rdpsnd))
			return FALSE;
	}

	rdpsnd->context.selected_client_format = -1;

	RDPSND_PDU_INIT(s, SNDC_CLOSE);
	RDPSND_PDU_FINISH(s);
}

rdpsnd_server_context* rdpsnd_server_context_new(WTSVirtualChannelManager* vcm)
{
	rdpsnd_server* rdpsnd;

	rdpsnd = xnew(rdpsnd_server);
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

	if (rdpsnd->rdpsnd_channel_thread)
	{
		freerdp_thread_stop(rdpsnd->rdpsnd_channel_thread);
		freerdp_thread_free(rdpsnd->rdpsnd_channel_thread);
	}

	if (rdpsnd->rdpsnd_channel)
		WTSVirtualChannelClose(rdpsnd->rdpsnd_channel);

	if (rdpsnd->rdpsnd_pdu)
		stream_free(rdpsnd->rdpsnd_pdu);

	if (rdpsnd->out_buffer)
		free(rdpsnd->out_buffer);

	if (rdpsnd->dsp_context)
		freerdp_dsp_context_free(rdpsnd->dsp_context);

	if (rdpsnd->context.client_formats)
		free(rdpsnd->context.client_formats);

	free(rdpsnd);
}
