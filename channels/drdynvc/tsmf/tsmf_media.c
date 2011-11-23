/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Video Redirection Virtual Channel - Media Container
 *
 * Copyright 2010-2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/mutex.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/plugins/tsmf.h>

#include "drdynvc_types.h"
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
	uint8 presentation_id[GUID_SIZE];

	const char* audio_name;
	const char* audio_device;
	int eos;

	uint32 last_x;
	uint32 last_y;
	uint32 last_width;
	uint32 last_height;
	uint16 last_num_rects;
	RDP_RECT* last_rects;

	uint32 output_x;
	uint32 output_y;
	uint32 output_width;
	uint32 output_height;
	uint16 output_num_rects;
	RDP_RECT* output_rects;

	IWTSVirtualChannelCallback* channel_callback;

	uint64 audio_start_time;
	uint64 audio_end_time;

	/* The stream list could be accessed by differnt threads and need to be protected. */
	freerdp_mutex mutex;

	LIST* stream_list;
};

struct _TSMF_STREAM
{
	uint32 stream_id;

	TSMF_PRESENTATION* presentation;

	ITSMFDecoder* decoder;

	int major_type;
	int eos;
	uint32 width;
	uint32 height;

	ITSMFAudioDevice* audio;
	uint32 sample_rate;
	uint32 channels;
	uint32 bits_per_sample;

	/* The end_time of last played sample */
	uint64 last_end_time;
	/* Next sample should not start before this system time. */
	uint64 next_start_time;

	freerdp_thread* thread;

	LIST* sample_list;

	/* The sample ack response queue will be accessed only by the stream thread. */
	LIST* sample_ack_list;
};

struct _TSMF_SAMPLE
{
	uint32 sample_id;
	uint64 start_time;
	uint64 end_time;
	uint64 duration;
	uint32 extensions;
	uint32 data_size;
	uint8* data;
	uint32 decoded_size;
	uint32 pixfmt;

	TSMF_STREAM* stream;
	IWTSVirtualChannelCallback* channel_callback;
	uint64 ack_time;
};

static LIST* presentation_list = NULL;

static uint64 get_current_time(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);
	return ((uint64)tp.tv_sec) * 10000000LL + ((uint64)tp.tv_usec) * 10LL;
}

static TSMF_SAMPLE* tsmf_stream_pop_sample(TSMF_STREAM* stream, int sync)
{
	TSMF_STREAM* s;
	LIST_ITEM* item;
	TSMF_SAMPLE* sample;
	boolean pending = false;
	TSMF_PRESENTATION* presentation = stream->presentation;

	if (!stream->sample_list->head)
		return NULL;

	if (sync)
	{
		if (stream->major_type == TSMF_MAJOR_TYPE_AUDIO)
		{
			/* Check if some other stream has earlier sample that needs to be played first */
			if (stream->last_end_time > AUDIO_TOLERANCE)
			{
				freerdp_mutex_lock(presentation->mutex);
				for (item = presentation->stream_list->head; item; item = item->next)
				{
					s = (TSMF_STREAM*) item->data;
					if (s != stream && !s->eos && s->last_end_time &&
						s->last_end_time < stream->last_end_time - AUDIO_TOLERANCE)
					{
							pending = true;
							break;
					}
				}
				freerdp_mutex_unlock(presentation->mutex);
			}
		}
		else
		{
			if (stream->last_end_time > presentation->audio_end_time)
			{
				pending = true;
			}
		}
	}
	if (pending)
		return NULL;

	freerdp_thread_lock(stream->thread);
	sample = (TSMF_SAMPLE*) list_dequeue(stream->sample_list);
	freerdp_thread_unlock(stream->thread);

	if (sample && sample->end_time > stream->last_end_time)
		stream->last_end_time = sample->end_time;

	return sample;
}

static void tsmf_sample_free(TSMF_SAMPLE* sample)
{
	if (sample->data)
		xfree(sample->data);
	xfree(sample);
}

static void tsmf_sample_ack(TSMF_SAMPLE* sample)
{
	tsmf_playback_ack(sample->channel_callback, sample->sample_id, sample->duration, sample->data_size);
}

static void tsmf_sample_queue_ack(TSMF_SAMPLE* sample)
{
	TSMF_STREAM* stream = sample->stream;

	list_enqueue(stream->sample_ack_list, sample);
}

static void tsmf_stream_process_ack(TSMF_STREAM* stream)
{
	TSMF_SAMPLE* sample;
	uint64 ack_time;

	ack_time = get_current_time();
	while (stream->sample_ack_list->head && !freerdp_thread_is_stopped(stream->thread))
	{
		sample = (TSMF_SAMPLE*) list_peek(stream->sample_ack_list);
		if (sample->ack_time > ack_time)
			break;

		sample = list_dequeue(stream->sample_ack_list);
		tsmf_sample_ack(sample);
		tsmf_sample_free(sample);
	}
}

TSMF_PRESENTATION* tsmf_presentation_new(const uint8* guid, IWTSVirtualChannelCallback* pChannelCallback)
{
	TSMF_PRESENTATION* presentation;

	presentation = tsmf_presentation_find_by_id(guid);
	if (presentation)
	{
		DEBUG_WARN("duplicated presentation id!");
		return NULL;
	}

	presentation = xnew(TSMF_PRESENTATION);

	memcpy(presentation->presentation_id, guid, GUID_SIZE);
	presentation->channel_callback = pChannelCallback;

	presentation->mutex = freerdp_mutex_new();
	presentation->stream_list = list_new();

	list_enqueue(presentation_list, presentation);

	return presentation;
}

TSMF_PRESENTATION* tsmf_presentation_find_by_id(const uint8* guid)
{
	LIST_ITEM* item;
	TSMF_PRESENTATION* presentation;

	for (item = presentation_list->head; item; item = item->next)
	{
		presentation = (TSMF_PRESENTATION*) item->data;
		if (memcmp(presentation->presentation_id, guid, GUID_SIZE) == 0)
			return presentation;
	}
	return NULL;
}

static void tsmf_presentation_restore_last_video_frame(TSMF_PRESENTATION* presentation)
{
	RDP_REDRAW_EVENT* revent;

	if (presentation->last_width && presentation->last_height)
	{
		revent = (RDP_REDRAW_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_TSMF, RDP_EVENT_TYPE_TSMF_REDRAW,
			NULL, NULL);
		revent->x = presentation->last_x;
		revent->y = presentation->last_y;
		revent->width = presentation->last_width;
		revent->height = presentation->last_height;
		if (!tsmf_push_event(presentation->channel_callback, (RDP_EVENT*) revent))
		{
			freerdp_event_free((RDP_EVENT*) revent);
		}
		presentation->last_x = 0;
		presentation->last_y = 0;
		presentation->last_width = 0;
		presentation->last_height = 0;
	}
}

static void tsmf_sample_playback_video(TSMF_SAMPLE* sample)
{
	uint64 t;
	RDP_VIDEO_FRAME_EVENT* vevent;
	TSMF_STREAM* stream = sample->stream;
	TSMF_PRESENTATION* presentation = stream->presentation;

	DEBUG_DVC("MessageId %d EndTime %d data_size %d consumed.",
		sample->sample_id, (int)sample->end_time, sample->data_size);

	if (sample->data)
	{
		t = get_current_time();
		if (stream->next_start_time > t &&
			(sample->end_time >= presentation->audio_start_time ||
			sample->end_time < stream->last_end_time))
		{
			freerdp_usleep((stream->next_start_time - t) / 10);
		}
		stream->next_start_time = t + sample->duration - 50000;

		if (presentation->last_x != presentation->output_x ||
			presentation->last_y != presentation->output_y ||
			presentation->last_width != presentation->output_width ||
			presentation->last_height != presentation->output_height ||
			presentation->last_num_rects != presentation->output_num_rects ||
			(presentation->last_rects && presentation->output_rects &&
			memcmp(presentation->last_rects, presentation->output_rects,
			presentation->last_num_rects * sizeof(RDP_RECT)) != 0))
		{
			tsmf_presentation_restore_last_video_frame(presentation);

			presentation->last_x = presentation->output_x;
			presentation->last_y = presentation->output_y;
			presentation->last_width = presentation->output_width;
			presentation->last_height = presentation->output_height;

			if (presentation->last_rects)
			{
				xfree(presentation->last_rects);
				presentation->last_rects = NULL;
			}
			presentation->last_num_rects = presentation->output_num_rects;
			if (presentation->last_num_rects > 0)
			{
				presentation->last_rects = xzalloc(presentation->last_num_rects * sizeof(RDP_RECT));
				memcpy(presentation->last_rects, presentation->output_rects,
					presentation->last_num_rects * sizeof(RDP_RECT));
			}
		}

		vevent = (RDP_VIDEO_FRAME_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_TSMF, RDP_EVENT_TYPE_TSMF_VIDEO_FRAME,
			NULL, NULL);
		vevent->frame_data = sample->data;
		vevent->frame_size = sample->decoded_size;
		vevent->frame_pixfmt = sample->pixfmt;
		vevent->frame_width = sample->stream->width;
		vevent->frame_height = sample->stream->height;
		vevent->x = presentation->output_x;
		vevent->y = presentation->output_y;
		vevent->width = presentation->output_width;
		vevent->height = presentation->output_height;
		if (presentation->output_num_rects > 0)
		{
			vevent->num_visible_rects = presentation->output_num_rects;
			vevent->visible_rects = (RDP_RECT*) xzalloc(presentation->output_num_rects * sizeof(RDP_RECT));
			memcpy(vevent->visible_rects, presentation->output_rects,
				presentation->output_num_rects * sizeof(RDP_RECT));
		}

		/* The frame data ownership is passed to the event object, and is freed after the event is processed. */
		sample->data = NULL;
		sample->decoded_size = 0;

		if (!tsmf_push_event(sample->channel_callback, (RDP_EVENT*) vevent))
		{
			freerdp_event_free((RDP_EVENT*) vevent);
		}

#if 0
		/* Dump a .ppm image for every 30 frames. Assuming the frame is in YUV format, we
		   extract the Y values to create a grayscale image. */
		static int frame_id = 0;
		char buf[100];
		FILE * fp;
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
	}
}

static void tsmf_sample_playback_audio(TSMF_SAMPLE* sample)
{
	uint64 latency = 0;
	TSMF_STREAM* stream = sample->stream;

	DEBUG_DVC("MessageId %d EndTime %d consumed.",
		sample->sample_id, (int)sample->end_time);

	if (sample->stream->audio && sample->data)
	{
		sample->stream->audio->Play(sample->stream->audio,
			sample->data, sample->decoded_size);
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
	boolean ret = false;
	uint32 width;
	uint32 height;
	uint32 pixfmt = 0;
	TSMF_STREAM* stream = sample->stream;

	if (stream->decoder)
		ret = stream->decoder->Decode(stream->decoder, sample->data, sample->data_size, sample->extensions);
	if (!ret)
	{
		tsmf_sample_ack(sample);
		tsmf_sample_free(sample);
		return;
	}

	xfree(sample->data);
	sample->data = NULL;

	if (stream->major_type == TSMF_MAJOR_TYPE_VIDEO)
	{
		if (stream->decoder->GetDecodedFormat)
		{
			pixfmt = stream->decoder->GetDecodedFormat(stream->decoder);
			if (pixfmt == ((uint32) -1))
			{
				tsmf_sample_ack(sample);
				tsmf_sample_free(sample);
				return;
			}
			sample->pixfmt = pixfmt;
		}

		if (stream->decoder->GetDecodedDimension)
			ret = stream->decoder->GetDecodedDimension(stream->decoder, &width, &height);
		if (ret && (width != stream->width || height != stream->height))
		{
			DEBUG_DVC("video dimension changed to %d x %d", width, height);
			stream->width = width;
			stream->height = height;
		}
	}

	if (stream->decoder->GetDecodedData)
	{
		sample->data = stream->decoder->GetDecodedData(stream->decoder, &sample->decoded_size);
	}

	switch (sample->stream->major_type)
	{
		case TSMF_MAJOR_TYPE_VIDEO:
			tsmf_sample_playback_video(sample);
			tsmf_sample_ack(sample);
			tsmf_sample_free(sample);
			break;
		case TSMF_MAJOR_TYPE_AUDIO:
			tsmf_sample_playback_audio(sample);
			tsmf_sample_queue_ack(sample);
			break;
	}
}

static void* tsmf_stream_playback_func(void* arg)
{
	TSMF_SAMPLE* sample;
	TSMF_STREAM* stream = (TSMF_STREAM*) arg;
	TSMF_PRESENTATION* presentation = stream->presentation;

	DEBUG_DVC("in %d", stream->stream_id);

	if (stream->major_type == TSMF_MAJOR_TYPE_AUDIO &&
		stream->sample_rate && stream->channels && stream->bits_per_sample)
	{
		stream->audio = tsmf_load_audio_device(
			presentation->audio_name && presentation->audio_name[0] ? presentation->audio_name : NULL,
			presentation->audio_device && presentation->audio_device[0] ? presentation->audio_device : NULL);
		if (stream->audio)
		{
			stream->audio->SetFormat(stream->audio,
				stream->sample_rate, stream->channels, stream->bits_per_sample);
		}
	}
	while (!freerdp_thread_is_stopped(stream->thread))
	{
		tsmf_stream_process_ack(stream);
		sample = tsmf_stream_pop_sample(stream, 1);
		if (sample)
			tsmf_sample_playback(sample);
		else
			freerdp_usleep(5000);
	}
	if (stream->eos || presentation->eos)
	{
		while ((sample = tsmf_stream_pop_sample(stream, 1)) != NULL)
			tsmf_sample_playback(sample);
	}
	if (stream->audio)
	{
		stream->audio->Free(stream->audio);
		stream->audio = NULL;
	}

	freerdp_thread_quit(stream->thread);

	DEBUG_DVC("out %d", stream->stream_id);

	return NULL;
}

static void tsmf_stream_start(TSMF_STREAM* stream)
{
	if (!freerdp_thread_is_running(stream->thread))
	{
		freerdp_thread_start(stream->thread, tsmf_stream_playback_func, stream);
	}
}

static void tsmf_stream_stop(TSMF_STREAM* stream)
{
	if (freerdp_thread_is_running(stream->thread))
	{
		freerdp_thread_stop(stream->thread);
	}
}

void tsmf_presentation_start(TSMF_PRESENTATION* presentation)
{
	LIST_ITEM* item;
	TSMF_STREAM* stream;

	for (item = presentation->stream_list->head; item; item = item->next)
	{
		stream = (TSMF_STREAM*) item->data;
		tsmf_stream_start(stream);
	}
}

void tsmf_presentation_stop(TSMF_PRESENTATION* presentation)
{
	LIST_ITEM* item;
	TSMF_STREAM* stream;

	tsmf_presentation_flush(presentation);

	for (item = presentation->stream_list->head; item; item = item->next)
	{
		stream = (TSMF_STREAM*) item->data;
		tsmf_stream_stop(stream);
	}

	tsmf_presentation_restore_last_video_frame(presentation);
	if (presentation->last_rects)
	{
		xfree(presentation->last_rects);
		presentation->last_rects = NULL;
	}
	presentation->last_num_rects = 0;
	if (presentation->output_rects)
	{
		xfree(presentation->output_rects);
		presentation->output_rects = NULL;
	}
	presentation->output_num_rects = 0;
}

void tsmf_presentation_set_geometry_info(TSMF_PRESENTATION* presentation,
	uint32 x, uint32 y, uint32 width, uint32 height,
	int num_rects, RDP_RECT* rects)
{
	presentation->output_x = x;
	presentation->output_y = y;
	presentation->output_width = width;
	presentation->output_height = height;
	if (presentation->output_rects)
		xfree(presentation->output_rects);
	presentation->output_rects = rects;
	presentation->output_num_rects = num_rects;
}

void tsmf_presentation_set_audio_device(TSMF_PRESENTATION* presentation, const char* name, const char* device)
{
	presentation->audio_name = name;
	presentation->audio_device = device;
}

static void tsmf_stream_flush(TSMF_STREAM* stream)
{
	TSMF_SAMPLE* sample;

	while ((sample = tsmf_stream_pop_sample(stream, 0)) != NULL)
		tsmf_sample_free(sample);

	while ((sample = list_dequeue(stream->sample_ack_list)) != NULL)
		tsmf_sample_free(sample);

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
	LIST_ITEM* item;
	TSMF_STREAM * stream;

	for (item = presentation->stream_list->head; item; item = item->next)
	{
		stream = (TSMF_STREAM*) item->data;
		tsmf_stream_flush(stream);
	}

	presentation->eos = 0;
	presentation->audio_start_time = 0;
	presentation->audio_end_time = 0;
}

void tsmf_presentation_free(TSMF_PRESENTATION* presentation)
{
	TSMF_STREAM* stream;

	tsmf_presentation_stop(presentation);
	list_remove(presentation_list, presentation);

	while (presentation->stream_list->head)
	{
		stream = (TSMF_STREAM*) list_peek(presentation->stream_list);
		tsmf_stream_free(stream);
	}
	list_free(presentation->stream_list);

	freerdp_mutex_free(presentation->mutex);

	xfree(presentation);
}

TSMF_STREAM* tsmf_stream_new(TSMF_PRESENTATION* presentation, uint32 stream_id)
{
	TSMF_STREAM* stream;

	stream = tsmf_stream_find_by_id(presentation, stream_id);
	if (stream)
	{
		DEBUG_WARN("duplicated stream id %d!", stream_id);
		return NULL;
	}

	stream = xnew(TSMF_STREAM);

	stream->stream_id = stream_id;
	stream->presentation = presentation;
	stream->thread = freerdp_thread_new();
	stream->sample_list = list_new();
	stream->sample_ack_list = list_new();

	freerdp_mutex_lock(presentation->mutex);
	list_enqueue(presentation->stream_list, stream);
	freerdp_mutex_unlock(presentation->mutex);

	return stream;
}

TSMF_STREAM* tsmf_stream_find_by_id(TSMF_PRESENTATION* presentation, uint32 stream_id)
{
	LIST_ITEM* item;
	TSMF_STREAM* stream;

	for (item = presentation->stream_list->head; item; item = item->next)
	{
		stream = (TSMF_STREAM*) item->data;
		if (stream->stream_id == stream_id)
			return stream;
	}
	return NULL;
}

void tsmf_stream_set_format(TSMF_STREAM* stream, const char* name, STREAM* s)
{
	TS_AM_MEDIA_TYPE mediatype;

	if (stream->decoder)
	{
		DEBUG_WARN("duplicated call");
		return;
	}

	tsmf_codec_parse_media_type(&mediatype, s);

	if (mediatype.MajorType == TSMF_MAJOR_TYPE_VIDEO)
	{
		DEBUG_DVC("video width %d height %d bit_rate %d frame_rate %f codec_data %d",
			mediatype.Width, mediatype.Height, mediatype.BitRate,
			(double)mediatype.SamplesPerSecond.Numerator / (double)mediatype.SamplesPerSecond.Denominator,
			mediatype.ExtraDataSize);
	}
	else if (mediatype.MajorType == TSMF_MAJOR_TYPE_AUDIO)
	{
		DEBUG_DVC("audio channel %d sample_rate %d bits_per_sample %d codec_data %d",
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
}

void tsmf_stream_end(TSMF_STREAM* stream)
{
	stream->eos = 1;
	stream->presentation->eos = 1;
}

void tsmf_stream_free(TSMF_STREAM* stream)
{
	TSMF_PRESENTATION* presentation = stream->presentation;

	tsmf_stream_stop(stream);
	tsmf_stream_flush(stream);

	freerdp_mutex_lock(presentation->mutex);
	list_remove(presentation->stream_list, stream);
	freerdp_mutex_unlock(presentation->mutex);

	list_free(stream->sample_list);
	list_free(stream->sample_ack_list);

	if (stream->decoder)
		stream->decoder->Free(stream->decoder);

	freerdp_thread_free(stream->thread);

	xfree(stream);
}

void tsmf_stream_push_sample(TSMF_STREAM* stream, IWTSVirtualChannelCallback* pChannelCallback,
	uint32 sample_id, uint64 start_time, uint64 end_time, uint64 duration, uint32 extensions,
	uint32 data_size, uint8* data)
{
	TSMF_SAMPLE* sample;

	sample = xnew(TSMF_SAMPLE);

	sample->sample_id = sample_id;
	sample->start_time = start_time;
	sample->end_time = end_time;
	sample->duration = duration;
	sample->extensions = extensions;
	sample->stream = stream;
	sample->channel_callback = pChannelCallback;
	sample->data_size = data_size;
	sample->data = xzalloc(data_size + TSMF_BUFFER_PADDING_SIZE);
	memcpy(sample->data, data, data_size);

	freerdp_thread_lock(stream->thread);
	list_enqueue(stream->sample_list, sample);
	freerdp_thread_unlock(stream->thread);
}

void tsmf_media_init(void)
{
	if (presentation_list == NULL)
		presentation_list = list_new();
}

