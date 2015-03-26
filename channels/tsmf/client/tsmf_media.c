/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Media Container
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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
#include <signal.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/client/tsmf.h>

#include "tsmf_constants.h"
#include "tsmf_types.h"
#include "tsmf_decoder.h"
#include "tsmf_audio.h"
#include "tsmf_main.h"
#include "tsmf_codec.h"
#include "tsmf_media.h"

#define AUDIO_TOLERANCE 10000000LL

struct _TSMF_PRESENTATION
{
	BYTE presentation_id[GUID_SIZE];

	const char *audio_name;
	const char *audio_device;
	int eos;

	IWTSVirtualChannelCallback *channel_callback;

	UINT64 audio_start_time;
	UINT64 audio_end_time;

	UINT32 volume;
	UINT32 muted;

	wArrayList *stream_list;

	int x;
	int y;
	int width;
	int height;

	int nr_rects;
	void *rects;
};

struct _TSMF_STREAM
{
	UINT32 stream_id;

	TSMF_PRESENTATION* presentation;

	ITSMFDecoder *decoder;

	int major_type;
	int eos;
	UINT32 width;
	UINT32 height;

	ITSMFAudioDevice *audio;
	UINT32 sample_rate;
	UINT32 channels;
	UINT32 bits_per_sample;

	/* The end_time of last played sample */
	UINT64 last_end_time;
	/* Next sample should not start before this system time. */
	UINT64 next_start_time;

	HANDLE play_thread;
	HANDLE ack_thread;
	HANDLE stopEvent;
	HANDLE ready;

	wQueue *sample_list;
	wQueue *sample_ack_list;
};

struct _TSMF_SAMPLE
{
	UINT32 sample_id;
	UINT64 start_time;
	UINT64 end_time;
	UINT64 duration;
	UINT32 extensions;
	UINT32 data_size;
	BYTE *data;
	UINT32 decoded_size;
	UINT32 pixfmt;

	TSMF_STREAM* stream;
	IWTSVirtualChannelCallback *channel_callback;
	UINT64 ack_time;
};

static wArrayList *presentation_list = NULL;
static int TERMINATING = 0;

static void _tsmf_presentation_free(TSMF_PRESENTATION* presentation);
static void _tsmf_stream_free(TSMF_STREAM* stream);

static UINT64 get_current_time(void)
{
	struct timeval tp;
	gettimeofday(&tp, 0);
	return ((UINT64)tp.tv_sec) * 10000000LL + ((UINT64)tp.tv_usec) * 10LL;
}

static TSMF_SAMPLE* tsmf_stream_pop_sample(TSMF_STREAM* stream, int sync)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM *s;
	TSMF_SAMPLE* sample;
	BOOL pending = FALSE;
	TSMF_PRESENTATION* presentation = stream->presentation;

	if (!stream)
		return NULL;

	if (Queue_Count(stream->sample_list) < 1)
		return NULL;

	if (sync)
	{
		if (stream->decoder)
		{
			if (stream->decoder->GetDecodedData)
			{
				if (stream->major_type == TSMF_MAJOR_TYPE_AUDIO)
				{
					/* Check if some other stream has earlier sample that needs to be played first */
					if (stream->last_end_time > AUDIO_TOLERANCE)
					{
						ArrayList_Lock(presentation->stream_list);
						count = ArrayList_Count(presentation->stream_list);

						for (index = 0; index < count; index++)
						{
							s = (TSMF_STREAM *) ArrayList_GetItem(presentation->stream_list, index);

							if (s != stream && !s->eos && s->last_end_time &&
									s->last_end_time < stream->last_end_time - AUDIO_TOLERANCE)
							{
								pending = TRUE;
								break;
							}
						}

						ArrayList_Unlock(presentation->stream_list);
					}
				}
				else
				{
					if (stream->last_end_time > presentation->audio_end_time)
					{
						pending = TRUE;
					}
				}
			}
		}
	}

	if (pending)
		return NULL;

	sample = (TSMF_SAMPLE *) Queue_Dequeue(stream->sample_list);

	if (sample && (sample->end_time > stream->last_end_time))
		stream->last_end_time = sample->end_time;

	return sample;
}

static void tsmf_sample_free(void *arg)
{
	TSMF_SAMPLE* sample = arg;

	if (!sample)
		return;

	if (sample->data)
		free(sample->data);

	free(sample);
}

static void tsmf_sample_ack(TSMF_SAMPLE* sample)
{
	if (!sample)
		return;

	tsmf_playback_ack(sample->channel_callback, sample->sample_id, sample->duration, sample->data_size);
}

static void tsmf_sample_queue_ack(TSMF_SAMPLE* sample)
{
	if (!sample)
		return;

	if (!sample->stream)
		return;

	Queue_Enqueue(sample->stream->sample_ack_list, sample);
}

static BOOL tsmf_stream_process_ack(void* arg, BOOL force)
{
	TSMF_STREAM* stream = arg;
	TSMF_SAMPLE* sample;
	UINT64 ack_time;
	BOOL rc = FALSE;

	if (!stream)
		return FALSE;

	Queue_Lock(stream->sample_ack_list);
	sample = (TSMF_SAMPLE*) Queue_Peek(stream->sample_ack_list);

	if (!sample)
		goto finally;

	if (!force)
	{
		ack_time = get_current_time();

		if (sample->ack_time > ack_time)
			goto finally;
	}

	sample = Queue_Dequeue(stream->sample_ack_list);
	tsmf_sample_ack(sample);
	tsmf_sample_free(sample);

finally:
	Queue_Unlock(stream->sample_ack_list);
	return rc;
}

TSMF_PRESENTATION* tsmf_presentation_new(const BYTE* guid, IWTSVirtualChannelCallback* pChannelCallback)
{
	TSMF_PRESENTATION* presentation;

	if (!guid || !pChannelCallback)
		return NULL;

	presentation = (TSMF_PRESENTATION*) calloc(1, sizeof(TSMF_PRESENTATION));

	if (!presentation)
	{
		WLog_ERR(TAG, "calloc failed");
		return NULL;
	}

	CopyMemory(presentation->presentation_id, guid, GUID_SIZE);
	presentation->channel_callback = pChannelCallback;
	presentation->volume = 5000; /* 50% */
	presentation->stream_list = ArrayList_New(TRUE);
	ArrayList_Object(presentation->stream_list)->fnObjectFree = (OBJECT_FREE_FN) _tsmf_stream_free;

	ArrayList_Add(presentation_list, presentation);

	return presentation;
}

static char* guid_to_string(const BYTE* guid, char* str, size_t len)
{
	int i;

	if (!guid || !str)
		return NULL;

	for (i=0; i<GUID_SIZE && len > 2*i; i++)
		snprintf(str + (2*i), len - 2*i, "%02X", guid[i]);

	return str;
}

TSMF_PRESENTATION* tsmf_presentation_find_by_id(const BYTE *guid)
{
	UINT32 index;
	UINT32 count;
	BOOL found = FALSE;
	char guid_str[GUID_SIZE * 2 + 1];
	TSMF_PRESENTATION* presentation;

	ArrayList_Lock(presentation_list);
	count = ArrayList_Count(presentation_list);

	for (index = 0; index < count; index++)
	{
		presentation = (TSMF_PRESENTATION*) ArrayList_GetItem(presentation_list, index);

		if (memcmp(presentation->presentation_id, guid, GUID_SIZE) == 0)
		{
			found = TRUE;
			break;
		}
	}

	ArrayList_Unlock(presentation_list);

	if (!found)
		WLog_WARN(TAG, "presentation id %s not found", guid_to_string(guid, guid_str, sizeof(guid_str)));

	return (found) ? presentation : NULL;
}

static void tsmf_sample_playback_video(TSMF_SAMPLE* sample)
{
	UINT64 t;
	TSMF_VIDEO_FRAME_EVENT event;
	TSMF_STREAM* stream = sample->stream;
	TSMF_PRESENTATION* presentation = stream->presentation;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*) sample->channel_callback;
	TsmfClientContext* tsmf = (TsmfClientContext*) callback->plugin->pInterface;

	DEBUG_TSMF("MessageId %d EndTime %d data_size %d consumed.",
			   sample->sample_id, (int) sample->end_time, sample->data_size);

	if (sample->data)
	{
		t = get_current_time();

		if (stream->next_start_time > t &&
				(sample->end_time >= presentation->audio_start_time ||
				 sample->end_time < stream->last_end_time))
		{
			USleep((stream->next_start_time - t) / 10);
		}

		stream->next_start_time = t + sample->duration - 50000;

		ZeroMemory(&event, sizeof(TSMF_VIDEO_FRAME_EVENT));

		event.frameData = sample->data;
		event.frameSize = sample->decoded_size;
		event.framePixFmt = sample->pixfmt;
		event.frameWidth = sample->stream->width;
		event.frameHeight = sample->stream->height;

#if 0
		/* Dump a .ppm image for every 30 frames. Assuming the frame is in YUV format, we
		   extract the Y values to create a grayscale image. */
		static int frame_id = 0;
		char buf[100];
		FILE *fp;

		if ((frame_id % 30) == 0)
		{
			snprintf(buf, sizeof(buf), "/tmp/FreeRDP_Frame_%d.ppm", frame_id);
			fp = fopen(buf, "wb");
			fwrite("P5\n", 1, 3, fp);
			snprintf(buf, sizeof(buf), "%d %d\n", sample->stream->width, sample->stream->height);
			fwrite(buf, 1, strlen(buf), fp);
			fwrite("255\n", 1, 4, fp);
			fwrite(sample->data, 1, sample->stream->width * sample->stream->height, fp);
			fflush(fp);
			fclose(fp);
		}

		frame_id++;
#endif

		/* The frame data ownership is passed to the event object, and is freed after the event is processed. */
		sample->data = NULL;
		sample->decoded_size = 0;

		if (tsmf->FrameEvent)
			tsmf->FrameEvent(tsmf, &event);

		free(event.frameData);
	}
}

static void tsmf_sample_playback_audio(TSMF_SAMPLE* sample)
{
	UINT64 latency = 0;
	TSMF_STREAM* stream = sample->stream;

	DEBUG_TSMF("MessageId %d EndTime %d consumed.",
			   sample->sample_id, (int)sample->end_time);

	if (sample->stream->audio && sample->data)
	{
		sample->stream->audio->Play(sample->stream->audio, sample->data, sample->decoded_size);
		sample->data = NULL;
		sample->decoded_size = 0;

		if (stream->audio && stream->audio->GetLatency)
			latency = stream->audio->GetLatency(stream->audio);
	}
	else
	{
		latency = 0;
	}

	sample->ack_time = latency + get_current_time();
	stream->last_end_time = sample->end_time + latency;
	stream->presentation->audio_start_time = sample->start_time + latency;
	stream->presentation->audio_end_time = sample->end_time + latency;
}

static void tsmf_sample_playback(TSMF_SAMPLE* sample)
{
	BOOL ret = FALSE;
	UINT32 width;
	UINT32 height;
	UINT32 pixfmt = 0;
	TSMF_STREAM* stream = sample->stream;

	if (stream->decoder)
	{
		if (stream->decoder->DecodeEx)
		{
			ret = stream->decoder->DecodeEx(stream->decoder, sample->data, sample->data_size, sample->extensions,
						sample->start_time, sample->end_time, sample->duration);
		}
		else
		{
			ret = stream->decoder->Decode(stream->decoder, sample->data, sample->data_size, sample->extensions);
		}
	}

	if (!ret)
	{
		tsmf_sample_queue_ack(sample);
		return;
	}

	free(sample->data);
	sample->data = NULL;

	if (stream->major_type == TSMF_MAJOR_TYPE_VIDEO)
	{
		if (stream->decoder->GetDecodedFormat)
		{
			pixfmt = stream->decoder->GetDecodedFormat(stream->decoder);

			if (pixfmt == ((UINT32) -1))
			{
				tsmf_sample_queue_ack(sample);
				return;
			}

			sample->pixfmt = pixfmt;
		}

		ret = FALSE;

		if (stream->decoder->GetDecodedDimension)
		{
			ret = stream->decoder->GetDecodedDimension(stream->decoder, &width, &height);

			if (ret && (width != stream->width || height != stream->height))
			{
				DEBUG_TSMF("video dimension changed to %d x %d", width, height);
				stream->width = width;
				stream->height = height;
			}
		}
	}

	if (stream->decoder->GetDecodedData)
	{
		sample->data = stream->decoder->GetDecodedData(stream->decoder, &sample->decoded_size);

		switch (sample->stream->major_type)
		{
			case TSMF_MAJOR_TYPE_VIDEO:
				tsmf_sample_playback_video(sample);
				tsmf_sample_queue_ack(sample);
				break;

			case TSMF_MAJOR_TYPE_AUDIO:
				tsmf_sample_playback_audio(sample);
				tsmf_sample_queue_ack(sample);
				break;
		}
	}
	else
	{
		TSMF_STREAM* stream = sample->stream;
		UINT64 ack_anticipation_time = get_current_time();
		UINT64 currentRunningTime = sample->start_time;
		BOOL buffer_filled = TRUE;

		if (stream->decoder->GetRunningTime)
		{
			currentRunningTime = stream->decoder->GetRunningTime(stream->decoder);
		}

		if (stream->decoder->BufferFilled)
		{
			buffer_filled = stream->decoder->BufferFilled(stream->decoder);
		}

		if (buffer_filled)
		{
			if (currentRunningTime > sample->start_time)
			{
				ack_anticipation_time += sample->duration;
			}
			else if (currentRunningTime == 0)
			{
				ack_anticipation_time += sample->duration;
			}
			else
			{
				ack_anticipation_time += (sample->start_time - currentRunningTime);
			}
		}
		else
		{
			ack_anticipation_time += sample->duration / 2;
		}

		switch (sample->stream->major_type)
		{
			case TSMF_MAJOR_TYPE_VIDEO:
				{
					break;
				}

			case TSMF_MAJOR_TYPE_AUDIO:
				{
					break;
				}
		}

		sample->ack_time = ack_anticipation_time;
		tsmf_sample_queue_ack(sample);
	}
}

static void* tsmf_stream_ack_func(void *arg)
{
	HANDLE hdl[2];
	TSMF_STREAM* stream = (TSMF_STREAM*) arg;
	DEBUG_TSMF("in %d", stream->stream_id);

	hdl[0] = stream->stopEvent;
	hdl[1] = Queue_Event(stream->sample_ack_list);

	while (1)
	{
		DWORD ev = WaitForMultipleObjects(2, hdl, FALSE, INFINITE);

		if (ev == WAIT_OBJECT_0)
			break;

		if (!stream->decoder)
			continue;

		if (stream->decoder->SetAckFunc)
			continue;

		if (tsmf_stream_process_ack(stream, FALSE))
			break;
	}

	DEBUG_TSMF("out %d", stream->stream_id);
	ExitThread(0);
	return NULL;
}

static void* tsmf_stream_playback_func(void *arg)
{
	HANDLE hdl[2];
	TSMF_SAMPLE* sample;
	TSMF_STREAM* stream = (TSMF_STREAM *) arg;
	TSMF_PRESENTATION* presentation = stream->presentation;

	DEBUG_TSMF("in %d", stream->stream_id);

	if (stream->major_type == TSMF_MAJOR_TYPE_AUDIO &&
			stream->sample_rate && stream->channels && stream->bits_per_sample)
	{
		if (stream->decoder)
		{
			if (stream->decoder->GetDecodedData)
			{
				stream->audio = tsmf_load_audio_device(
					presentation->audio_name && presentation->audio_name[0] ? presentation->audio_name : NULL,
					presentation->audio_device && presentation->audio_device[0] ? presentation->audio_device : NULL);

				if (stream->audio)
				{
					stream->audio->SetFormat(stream->audio, stream->sample_rate, stream->channels, stream->bits_per_sample);
				}
			}
		}
	}

	hdl[0] = stream->stopEvent;
	hdl[1] = Queue_Event(stream->sample_list);

	while (!(WaitForMultipleObjects(2, hdl, FALSE, INFINITE) == WAIT_OBJECT_0))
	{
		sample = tsmf_stream_pop_sample(stream, 0);

		if (sample)
			tsmf_sample_playback(sample);
	}

	if (stream->audio)
	{
		stream->audio->Free(stream->audio);
		stream->audio = NULL;
	}

	DEBUG_TSMF("out %d", stream->stream_id);
	ExitThread(0);
	return NULL;
}

static void tsmf_stream_start(TSMF_STREAM* stream)
{
	if (!stream || !stream->presentation || !stream->decoder)
		return;

	if (stream->decoder->Control)
	{
		stream->decoder->Control(stream->decoder, Control_Resume, NULL);
	}
}

static void tsmf_stream_stop(TSMF_STREAM* stream)
{
	if (!stream || !stream->decoder)
		return;

	if (stream->decoder->Control)
	{
		stream->decoder->Control(stream->decoder, Control_Stop, NULL);
	}
}

static void tsmf_stream_pause(TSMF_STREAM* stream)
{
	if (!stream || !stream->decoder)
		return;

	if (stream->decoder->Control)
	{
		stream->decoder->Control(stream->decoder, Control_Pause, NULL);
	}
}

static void tsmf_stream_restart(TSMF_STREAM* stream)
{
	if (!stream || !stream->decoder)
		return;

	if (stream->decoder->Control)
	{
		stream->decoder->Control(stream->decoder, Control_Resume, NULL);
	}
}

static void tsmf_stream_change_volume(TSMF_STREAM* stream, UINT32 newVolume, UINT32 muted)
{
	if (!stream || !stream->decoder)
		return;

	if (stream->decoder != NULL && stream->decoder->ChangeVolume)
	{
		stream->decoder->ChangeVolume(stream->decoder, newVolume, muted);
	}
	else if (stream->audio != NULL && stream->audio->ChangeVolume)
	{
		stream->audio->ChangeVolume(stream->audio, newVolume, muted);
	}
}

void tsmf_presentation_volume_changed(TSMF_PRESENTATION* presentation, UINT32 newVolume, UINT32 muted)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	presentation->volume = newVolume;
	presentation->muted = muted;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM *) ArrayList_GetItem(presentation->stream_list, index);
		tsmf_stream_change_volume(stream, newVolume, muted);
	}

	ArrayList_Unlock(presentation->stream_list);
}

void tsmf_presentation_paused(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM *) ArrayList_GetItem(presentation->stream_list, index);
		tsmf_stream_pause(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
}

void tsmf_presentation_restarted(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM *) ArrayList_GetItem(presentation->stream_list, index);
		tsmf_stream_restart(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
}

void tsmf_presentation_start(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM *) ArrayList_GetItem(presentation->stream_list, index);
		tsmf_stream_start(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
}

void tsmf_presentation_sync(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		TSMF_STREAM* stream = (TSMF_STREAM *) ArrayList_GetItem(presentation->stream_list, index);
		WaitForSingleObject(stream->ready, 500);
	}

	ArrayList_Unlock(presentation->stream_list);
}

void tsmf_presentation_stop(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	tsmf_presentation_flush(presentation);
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM *) ArrayList_GetItem(presentation->stream_list, index);
		tsmf_stream_stop(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
}

void tsmf_presentation_set_geometry_info(TSMF_PRESENTATION* presentation,
		UINT32 x, UINT32 y, UINT32 width, UINT32 height, int num_rects, RDP_RECT *rects)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	void *tmp_rects;

	/* The server may send messages with invalid width / height.
	 * Ignore those messages. */
	if (!width || !height)
		return;

	if ((width == presentation->width) && (height == presentation->height) &&
			(x == presentation->x) && (y == presentation->y) &&
			(num_rects == presentation->nr_rects) &&
			(0 == memcmp(rects, presentation->rects, num_rects * sizeof(RDP_RECT))))
	{
		return;
	}

	presentation->x = x;
	presentation->y = y;
	presentation->width = width;
	presentation->height = height;

	tmp_rects = realloc(presentation->rects, sizeof(RDP_RECT) * num_rects);
	if (!tmp_rects)
		return;
	presentation->nr_rects = num_rects;
	presentation->rects = tmp_rects;

	CopyMemory(presentation->rects, rects, sizeof(RDP_RECT) * num_rects);

	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM *) ArrayList_GetItem(presentation->stream_list, index);

		if (!stream->decoder)
			continue;

		if (stream->decoder->UpdateRenderingArea)
		{
			stream->decoder->UpdateRenderingArea(stream->decoder, x, y, width, height, num_rects, rects);
		}
	}

	ArrayList_Unlock(presentation->stream_list);
}

void tsmf_presentation_set_audio_device(TSMF_PRESENTATION* presentation, const char *name, const char *device)
{
	presentation->audio_name = name;
	presentation->audio_device = device;
}

static void tsmf_stream_flush(TSMF_STREAM* stream)
{
	//TSMF_SAMPLE* sample;
	/* TODO: free lists */
	if (stream->audio)
		stream->audio->Flush(stream->audio);

	stream->eos = 0;
	stream->last_end_time = 0;
	stream->next_start_time = 0;

	if (stream->major_type == TSMF_MAJOR_TYPE_AUDIO)
	{
		stream->presentation->audio_start_time = 0;
		stream->presentation->audio_end_time = 0;
	}
}

void tsmf_presentation_flush(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);
		tsmf_stream_flush(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
	presentation->eos = 0;
	presentation->audio_start_time = 0;
	presentation->audio_end_time = 0;
}

void _tsmf_presentation_free(TSMF_PRESENTATION* presentation)
{
	tsmf_presentation_stop(presentation);
	ArrayList_Clear(presentation->stream_list);
	ArrayList_Free(presentation->stream_list);

	if (presentation->rects)
		free(presentation->rects);

	ZeroMemory(presentation, sizeof(TSMF_PRESENTATION));
	free(presentation);
}

void tsmf_presentation_free(TSMF_PRESENTATION* presentation)
{
	ArrayList_Remove(presentation_list, presentation);
}

TSMF_STREAM* tsmf_stream_new(TSMF_PRESENTATION* presentation, UINT32 stream_id)
{
	TSMF_STREAM* stream;
	stream = tsmf_stream_find_by_id(presentation, stream_id);

	if (stream)
	{
		WLog_ERR(TAG, "duplicated stream id %d!", stream_id);
		return NULL;
	}

	stream = (TSMF_STREAM*) calloc(1, sizeof(TSMF_STREAM));

	if (!stream)
	{
		WLog_ERR(TAG, "Calloc failed");
		return NULL;
	}

	stream->stream_id = stream_id;
	stream->presentation = presentation;
	stream->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	stream->ready = CreateEvent(NULL, TRUE, TRUE, NULL);
	stream->sample_list = Queue_New(TRUE, -1, -1);
	stream->sample_list->object.fnObjectFree = tsmf_sample_free;
	stream->sample_ack_list = Queue_New(TRUE, -1, -1);
	stream->sample_ack_list->object.fnObjectFree = tsmf_sample_free;
	stream->play_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) tsmf_stream_playback_func, stream, 0, NULL);
	stream->ack_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)tsmf_stream_ack_func, stream, 0, NULL);

	ArrayList_Add(presentation->stream_list, stream);

	return stream;
}

TSMF_STREAM *tsmf_stream_find_by_id(TSMF_PRESENTATION* presentation, UINT32 stream_id)
{
	UINT32 index;
	UINT32 count;
	BOOL found = FALSE;
	TSMF_STREAM* stream;

	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);

		if (stream->stream_id == stream_id)
		{
			found = TRUE;
			break;
		}
	}

	ArrayList_Unlock(presentation->stream_list);
	return (found) ? stream : NULL;
}

static void tsmf_stream_resync(void *arg)
{
	TSMF_STREAM* stream = arg;
	ResetEvent(stream->ready);
}

void tsmf_stream_set_format(TSMF_STREAM* stream, const char *name, wStream *s)
{
	TS_AM_MEDIA_TYPE mediatype;

	if (stream->decoder)
	{
		WLog_ERR(TAG, "duplicated call");
		return;
	}

	tsmf_codec_parse_media_type(&mediatype, s);

	if (mediatype.MajorType == TSMF_MAJOR_TYPE_VIDEO)
	{
		DEBUG_TSMF("video width %d height %d bit_rate %d frame_rate %f codec_data %d",
				   mediatype.Width, mediatype.Height, mediatype.BitRate,
				   (double) mediatype.SamplesPerSecond.Numerator / (double) mediatype.SamplesPerSecond.Denominator,
				   mediatype.ExtraDataSize);
	}
	else if (mediatype.MajorType == TSMF_MAJOR_TYPE_AUDIO)
	{
		DEBUG_TSMF("audio channel %d sample_rate %d bits_per_sample %d codec_data %d",
				   mediatype.Channels, mediatype.SamplesPerSecond.Numerator, mediatype.BitsPerSample,
				   mediatype.ExtraDataSize);
		stream->sample_rate = mediatype.SamplesPerSecond.Numerator;
		stream->channels = mediatype.Channels;
		stream->bits_per_sample = mediatype.BitsPerSample;

		if (stream->bits_per_sample == 0)
			stream->bits_per_sample = 16;
	}

	stream->major_type = mediatype.MajorType;
	stream->width = mediatype.Width;
	stream->height = mediatype.Height;
	stream->decoder = tsmf_load_decoder(name, &mediatype);
	tsmf_stream_change_volume(stream, stream->presentation->volume, stream->presentation->muted);

	if (!stream->decoder)
		return;

	if (stream->decoder->SetAckFunc)
		stream->decoder->SetAckFunc(stream->decoder, tsmf_stream_process_ack, stream);

	if (stream->decoder->SetSyncFunc)
		stream->decoder->SetSyncFunc(stream->decoder, tsmf_stream_resync, stream);
}

void tsmf_stream_end(TSMF_STREAM* stream)
{
	if (!stream)
		return;

	stream->eos = 1;
	stream->presentation->eos = 1;
}

void _tsmf_stream_free(TSMF_STREAM* stream)
{
	if (!stream)
		return;

	tsmf_stream_stop(stream);
	tsmf_stream_flush(stream);
	SetEvent(stream->stopEvent);

	if (stream->play_thread)
	{
		WaitForSingleObject(stream->play_thread, INFINITE);
		CloseHandle(stream->play_thread);
		stream->play_thread = NULL;
	}

	if (stream->ack_thread)
	{
		WaitForSingleObject(stream->ack_thread, INFINITE);
		CloseHandle(stream->ack_thread);
		stream->ack_thread = NULL;
	}

	Queue_Free(stream->sample_list);
	Queue_Free(stream->sample_ack_list);

	if (stream->decoder && stream->decoder->Free)
	{
		stream->decoder->Free(stream->decoder);
		stream->decoder = NULL;
	}

	CloseHandle(stream->stopEvent);
	CloseHandle(stream->ready);
	ZeroMemory(stream, sizeof(TSMF_STREAM));
	free(stream);
}

void tsmf_stream_free(TSMF_STREAM* stream)
{
	TSMF_PRESENTATION* presentation = stream->presentation;
	ArrayList_Remove(presentation->stream_list, stream);
}

void tsmf_stream_push_sample(TSMF_STREAM* stream, IWTSVirtualChannelCallback *pChannelCallback,
			UINT32 sample_id, UINT64 start_time, UINT64 end_time, UINT64 duration, UINT32 extensions,
			UINT32 data_size, BYTE *data)
{
	TSMF_SAMPLE* sample;
	SetEvent(stream->ready);

	if (TERMINATING)
		return;

	sample = (TSMF_SAMPLE*) calloc(1, sizeof(TSMF_SAMPLE));

	if (!sample)
	{
		WLog_ERR(TAG, "calloc failed!");
		return;
	}

	sample->sample_id = sample_id;
	sample->start_time = start_time;
	sample->end_time = end_time;
	sample->duration = duration;
	sample->extensions = extensions;
	sample->stream = stream;
	sample->channel_callback = pChannelCallback;
	sample->data_size = data_size;
	sample->data = calloc(1, data_size + TSMF_BUFFER_PADDING_SIZE);

	if (!sample->data)
	{
		WLog_ERR(TAG, "calloc failed!");
		free(sample);
		return;
	}

	CopyMemory(sample->data, data, data_size);
	Queue_Enqueue(stream->sample_list, sample);
}

#ifndef _WIN32

static void tsmf_signal_handler(int s)
{
	TERMINATING = 1;
	ArrayList_Free(presentation_list);

	if (s == SIGINT)
	{
		signal(s, SIG_DFL);
		kill(getpid(), s);
	}
	else if (s == SIGUSR1)
	{
		signal(s, SIG_DFL);
	}
}

#endif

void tsmf_media_init(void)
{
#ifndef _WIN32
	struct sigaction sigtrap;
	sigtrap.sa_handler = tsmf_signal_handler;
	sigemptyset(&sigtrap.sa_mask);
	sigtrap.sa_flags = 0;
	sigaction(SIGINT, &sigtrap, 0);
	sigaction(SIGUSR1, &sigtrap, 0);
#endif

	if (!presentation_list)
	{
		presentation_list = ArrayList_New(TRUE);
		ArrayList_Object(presentation_list)->fnObjectFree = (OBJECT_FREE_FN) _tsmf_presentation_free;
	}
}
