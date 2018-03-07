/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Media Container
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/string.h>
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

/* 1 second = 10,000,000 100ns units*/
#define VIDEO_ADJUST_MAX 10*1000*1000

#define MAX_ACK_TIME 666667

#define AUDIO_MIN_BUFFER_LEVEL 3
#define AUDIO_MAX_BUFFER_LEVEL 6

#define VIDEO_MIN_BUFFER_LEVEL 10
#define VIDEO_MAX_BUFFER_LEVEL 30

struct _TSMF_PRESENTATION
{
	BYTE presentation_id[GUID_SIZE];

	const char* audio_name;
	const char* audio_device;

	IWTSVirtualChannelCallback* channel_callback;

	UINT64 audio_start_time;
	UINT64 audio_end_time;

	UINT32 volume;
	UINT32 muted;

	wArrayList* stream_list;

	int x;
	int y;
	int width;
	int height;

	int nr_rects;
	void* rects;
};

struct _TSMF_STREAM
{
	UINT32 stream_id;

	TSMF_PRESENTATION* presentation;

	ITSMFDecoder* decoder;

	int major_type;
	int eos;
	UINT32 eos_message_id;
	IWTSVirtualChannelCallback* eos_channel_callback;
	int delayed_stop;
	UINT32 width;
	UINT32 height;

	ITSMFAudioDevice* audio;
	UINT32 sample_rate;
	UINT32 channels;
	UINT32 bits_per_sample;

	/* The start time of last played sample */
	UINT64 last_start_time;
	/* The end_time of last played sample */
	UINT64 last_end_time;
	/* Next sample should not start before this system time. */
	UINT64 next_start_time;

	UINT32 minBufferLevel;
	UINT32 maxBufferLevel;
	UINT32 currentBufferLevel;

	HANDLE play_thread;
	HANDLE ack_thread;
	HANDLE stopEvent;
	HANDLE ready;

	wQueue* sample_list;
	wQueue* sample_ack_list;
	rdpContext* rdpcontext;

	BOOL seeking;
};

struct _TSMF_SAMPLE
{
	UINT32 sample_id;
	UINT64 start_time;
	UINT64 end_time;
	UINT64 duration;
	UINT32 extensions;
	UINT32 data_size;
	BYTE* data;
	UINT32 decoded_size;
	UINT32 pixfmt;

	BOOL invalidTimestamps;

	TSMF_STREAM* stream;
	IWTSVirtualChannelCallback* channel_callback;
	UINT64 ack_time;
};

static wArrayList* presentation_list = NULL;
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
	TSMF_STREAM* s;
	TSMF_SAMPLE* sample;
	BOOL pending = FALSE;
	TSMF_PRESENTATION* presentation = NULL;

	if (!stream)
		return NULL;

	presentation = stream->presentation;

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
					/* Start time is more reliable than end time as some stream types seem to have incorrect
					 * end times from the server
					 */
					if (stream->last_start_time > AUDIO_TOLERANCE)
					{
						ArrayList_Lock(presentation->stream_list);
						count = ArrayList_Count(presentation->stream_list);

						for (index = 0; index < count; index++)
						{
							s = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);

							/* Start time is more reliable than end time as some stream types seem to have incorrect
							 * end times from the server
							 */
							if (s != stream && !s->eos && s->last_start_time &&
							    s->last_start_time < stream->last_start_time - AUDIO_TOLERANCE)
							{
								DEBUG_TSMF("Pending due to audio tolerance");
								pending = TRUE;
								break;
							}
						}

						ArrayList_Unlock(presentation->stream_list);
					}
				}
				else
				{
					/* Start time is more reliable than end time as some stream types seem to have incorrect
					 * end times from the server
					 */
					if (stream->last_start_time > presentation->audio_start_time)
					{
						DEBUG_TSMF("Pending due to stream start time > audio start time");
						pending = TRUE;
					}
				}
			}
		}
	}

	if (pending)
		return NULL;

	sample = (TSMF_SAMPLE*) Queue_Dequeue(stream->sample_list);

	/* Only update stream last end time if the sample end time is valid and greater than the current stream end time */
	if (sample && (sample->end_time > stream->last_end_time)
	    && (!sample->invalidTimestamps))
		stream->last_end_time = sample->end_time;

	/* Only update stream last start time if the sample start time is valid and greater than the current stream start time */
	if (sample && (sample->start_time > stream->last_start_time)
	    && (!sample->invalidTimestamps))
		stream->last_start_time = sample->start_time;

	return sample;
}

static void tsmf_sample_free(void* arg)
{
	TSMF_SAMPLE* sample = arg;

	if (!sample)
		return;

	free(sample->data);
	free(sample);
}

static BOOL tsmf_sample_ack(TSMF_SAMPLE* sample)
{
	if (!sample)
		return FALSE;

	return tsmf_playback_ack(sample->channel_callback, sample->sample_id,
	                         sample->duration, sample->data_size);
}

static BOOL tsmf_sample_queue_ack(TSMF_SAMPLE* sample)
{
	if (!sample)
		return FALSE;

	if (!sample->stream)
		return FALSE;

	return Queue_Enqueue(sample->stream->sample_ack_list, sample);
}

/* Returns TRUE if no more samples are currently available
 * Returns FALSE otherwise
 */
static BOOL tsmf_stream_process_ack(void* arg, BOOL force)
{
	TSMF_STREAM* stream = arg;
	TSMF_SAMPLE* sample;
	UINT64 ack_time;
	BOOL rc = FALSE;

	if (!stream)
		return TRUE;

	Queue_Lock(stream->sample_ack_list);
	sample = (TSMF_SAMPLE*) Queue_Peek(stream->sample_ack_list);

	if (!sample)
	{
		rc = TRUE;
		goto finally;
	}

	if (!force)
	{
		/* Do some min/max ack limiting if we have access to Buffer level information */
		if (stream->decoder && stream->decoder->BufferLevel)
		{
			/* Try to keep buffer level below max by withholding acks */
			if (stream->currentBufferLevel > stream->maxBufferLevel)
				goto finally;
			/* Try to keep buffer level above min by pushing acks through quickly */
			else if (stream->currentBufferLevel < stream->minBufferLevel)
				goto dequeue;
		}

		/* Time based acks only */
		ack_time = get_current_time();

		if (sample->ack_time > ack_time)
			goto finally;
	}

dequeue:
	sample = Queue_Dequeue(stream->sample_ack_list);

	if (sample)
	{
		tsmf_sample_ack(sample);
		tsmf_sample_free(sample);
	}

finally:
	Queue_Unlock(stream->sample_ack_list);
	return rc;
}

TSMF_PRESENTATION* tsmf_presentation_new(const BYTE* guid,
        IWTSVirtualChannelCallback* pChannelCallback)
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
	presentation->muted = 0;

	if (!(presentation->stream_list = ArrayList_New(TRUE)))
		goto error_stream_list;

	ArrayList_Object(presentation->stream_list)->fnObjectFree =
	    (OBJECT_FREE_FN) _tsmf_stream_free;

	if (ArrayList_Add(presentation_list, presentation) < 0)
		goto error_add;

	return presentation;
error_add:
	ArrayList_Free(presentation->stream_list);
error_stream_list:
	free(presentation);
	return NULL;
}

static char* guid_to_string(const BYTE* guid, char* str, size_t len)
{
	int i;

	if (!guid || !str)
		return NULL;

	for (i = 0; i < GUID_SIZE && len > 2 * i; i++)
		sprintf_s(str + (2 * i), len - 2 * i, "%02"PRIX8"", guid[i]);

	return str;
}

TSMF_PRESENTATION* tsmf_presentation_find_by_id(const BYTE* guid)
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
		WLog_WARN(TAG, "presentation id %s not found", guid_to_string(guid, guid_str,
		          sizeof(guid_str)));

	return (found) ? presentation : NULL;
}

static BOOL tsmf_sample_playback_video(TSMF_SAMPLE* sample)
{
	UINT64 t;
	TSMF_VIDEO_FRAME_EVENT event;
	TSMF_STREAM* stream = sample->stream;
	TSMF_PRESENTATION* presentation = stream->presentation;
	TSMF_CHANNEL_CALLBACK* callback = (TSMF_CHANNEL_CALLBACK*)
	                                  sample->channel_callback;
	TsmfClientContext* tsmf = (TsmfClientContext*) callback->plugin->pInterface;
	DEBUG_TSMF("MessageId %"PRIu32" EndTime %"PRIu64" data_size %"PRIu32" consumed.",
	           sample->sample_id, sample->end_time, sample->data_size);

	if (sample->data)
	{
		t = get_current_time();

		/* Start time is more reliable than end time as some stream types seem to have incorrect
		 * end times from the server
		 */
		if (stream->next_start_time > t &&
		    ((sample->start_time >= presentation->audio_start_time) ||
		     ((sample->start_time < stream->last_start_time)
		      && (!sample->invalidTimestamps))))
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
		event.x = presentation->x;
		event.y = presentation->y;
		event.width = presentation->width;
		event.height = presentation->height;

		if (presentation->nr_rects > 0)
		{
			event.numVisibleRects = presentation->nr_rects;
			event.visibleRects = (RECTANGLE_16*) calloc(event.numVisibleRects, sizeof(RECTANGLE_16));

			if (!event.visibleRects)
			{
				WLog_ERR(TAG, "can't allocate memory for copy rectangles");
				return FALSE;
			}

			memcpy(event.visibleRects, presentation->rects, presentation->nr_rects * sizeof(RDP_RECT));
			presentation->nr_rects = 0;
		}

#if 0
		/* Dump a .ppm image for every 30 frames. Assuming the frame is in YUV format, we
		   extract the Y values to create a grayscale image. */
		static int frame_id = 0;
		char buf[100];
		FILE* fp;

		if ((frame_id % 30) == 0)
		{
			sprintf_s(buf, sizeof(buf), "/tmp/FreeRDP_Frame_%d.ppm", frame_id);
			fp = fopen(buf, "wb");
			fwrite("P5\n", 1, 3, fp);
			sprintf_s(buf, sizeof(buf), "%"PRIu32" %"PRIu32"\n", sample->stream->width,
			          sample->stream->height);
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

		if (event.visibleRects != NULL)
			free(event.visibleRects);
	}

	return TRUE;
}

static BOOL tsmf_sample_playback_audio(TSMF_SAMPLE* sample)
{
	UINT64 latency = 0;
	TSMF_STREAM* stream = sample->stream;
	BOOL ret;
	DEBUG_TSMF("MessageId %"PRIu32" EndTime %"PRIu64" consumed.",
	           sample->sample_id, sample->end_time);

	if (stream->audio && sample->data)
	{
		ret = sample->stream->audio->Play(sample->stream->audio, sample->data,
		                                  sample->decoded_size);
		sample->data = NULL;
		sample->decoded_size = 0;

		if (stream->audio->GetLatency)
			latency = stream->audio->GetLatency(stream->audio);
	}
	else
	{
		ret = TRUE;
		latency = 0;
	}

	sample->ack_time = latency + get_current_time();

	/* Only update stream times if the sample timestamps are valid */
	if (!sample->invalidTimestamps)
	{
		stream->last_start_time = sample->start_time + latency;
		stream->last_end_time = sample->end_time + latency;
		stream->presentation->audio_start_time = sample->start_time + latency;
		stream->presentation->audio_end_time = sample->end_time + latency;
	}

	return ret;
}

static BOOL tsmf_sample_playback(TSMF_SAMPLE* sample)
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
			/* Try to "sync" video buffers to audio buffers by looking at the running time for each stream
			 * The difference between the two running times causes an offset between audio and video actual
			 * render times. So, we try to adjust timestamps on the video buffer to match those on the audio buffer.
			 */
			if (stream->major_type == TSMF_MAJOR_TYPE_VIDEO)
			{
				TSMF_STREAM* temp_stream = NULL;
				TSMF_PRESENTATION* presentation = stream->presentation;
				ArrayList_Lock(presentation->stream_list);
				int count = ArrayList_Count(presentation->stream_list);
				int index = 0;

				for (index = 0; index < count; index++)
				{
					UINT64 time_diff;
					temp_stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list,
					              index);

					if (temp_stream->major_type == TSMF_MAJOR_TYPE_AUDIO)
					{
						UINT64 video_time = (UINT64) stream->decoder->GetRunningTime(stream->decoder);
						UINT64 audio_time = (UINT64) temp_stream->decoder->GetRunningTime(
						                        temp_stream->decoder);
						UINT64 max_adjust = VIDEO_ADJUST_MAX;

						if (video_time < audio_time)
							max_adjust = -VIDEO_ADJUST_MAX;

						if (video_time > audio_time)
							time_diff = video_time - audio_time;
						else
							time_diff = audio_time - video_time;

						time_diff = time_diff < VIDEO_ADJUST_MAX ? time_diff : max_adjust;
						sample->start_time += time_diff;
						sample->end_time += time_diff;
						break;
					}
				}

				ArrayList_Unlock(presentation->stream_list);
			}

			ret = stream->decoder->DecodeEx(stream->decoder, sample->data,
			                                sample->data_size, sample->extensions,
			                                sample->start_time, sample->end_time, sample->duration);
		}
		else
		{
			ret = stream->decoder->Decode(stream->decoder, sample->data, sample->data_size,
			                              sample->extensions);
		}
	}

	if (!ret)
	{
		WLog_ERR(TAG, "decode error, queue ack anyways");

		if (!tsmf_sample_queue_ack(sample))
		{
			WLog_ERR(TAG, "error queuing sample for ack");
			return FALSE;
		}

		return TRUE;
	}

	free(sample->data);
	sample->data = NULL;

	if (stream->major_type == TSMF_MAJOR_TYPE_VIDEO)
	{
		if (stream->decoder->GetDecodedFormat)
		{
			pixfmt = stream->decoder->GetDecodedFormat(stream->decoder);

			if (pixfmt == ((UINT32) - 1))
			{
				WLog_ERR(TAG, "unable to decode video format");

				if (!tsmf_sample_queue_ack(sample))
				{
					WLog_ERR(TAG, "error queuing sample for ack");
				}

				return FALSE;
			}

			sample->pixfmt = pixfmt;
		}

		if (stream->decoder->GetDecodedDimension)
		{
			ret = stream->decoder->GetDecodedDimension(stream->decoder, &width, &height);

			if (ret && (width != stream->width || height != stream->height))
			{
				DEBUG_TSMF("video dimension changed to %"PRIu32" x %"PRIu32"", width, height);
				stream->width = width;
				stream->height = height;
			}
		}
	}

	if (stream->decoder->GetDecodedData)
	{
		sample->data = stream->decoder->GetDecodedData(stream->decoder,
		               &sample->decoded_size);

		switch (sample->stream->major_type)
		{
			case TSMF_MAJOR_TYPE_VIDEO:
				ret = tsmf_sample_playback_video(sample) &&
				      tsmf_sample_queue_ack(sample);
				break;

			case TSMF_MAJOR_TYPE_AUDIO:
				ret = tsmf_sample_playback_audio(sample) &&
				      tsmf_sample_queue_ack(sample);
				break;
		}
	}
	else
	{
		TSMF_STREAM* stream = sample->stream;
		UINT64 ack_anticipation_time = get_current_time();
		BOOL buffer_filled = TRUE;

		/* Classify the buffer as filled once it reaches minimum level */
		if (stream->decoder->BufferLevel)
		{
			if (stream->currentBufferLevel < stream->minBufferLevel)
				buffer_filled = FALSE;
		}

		if (buffer_filled)
		{
			ack_anticipation_time += (sample->duration / 2 < MAX_ACK_TIME) ?
			                         sample->duration / 2 : MAX_ACK_TIME;
		}
		else
		{
			ack_anticipation_time += (sample->duration / 2 < MAX_ACK_TIME) ?
			                         sample->duration / 2 : MAX_ACK_TIME;
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

		if (!tsmf_sample_queue_ack(sample))
		{
			WLog_ERR(TAG, "error queuing sample for ack");
			ret = FALSE;
		}
	}

	return ret;
}

static DWORD WINAPI tsmf_stream_ack_func(LPVOID arg)
{
	HANDLE hdl[2];
	TSMF_STREAM* stream = (TSMF_STREAM*) arg;
	UINT error = CHANNEL_RC_OK;
	DEBUG_TSMF("in %"PRIu32"", stream->stream_id);
	hdl[0] = stream->stopEvent;
	hdl[1] = Queue_Event(stream->sample_ack_list);

	while (1)
	{
		DWORD ev = WaitForMultipleObjects(2, hdl, FALSE, 1000);

		if (ev == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", error);
			break;
		}

		if (stream->decoder)
			if (stream->decoder->BufferLevel)
				stream->currentBufferLevel = stream->decoder->BufferLevel(stream->decoder);

		if (stream->eos)
		{
			while ((stream->currentBufferLevel > 0)
			       && !(tsmf_stream_process_ack(stream, TRUE)))
			{
				DEBUG_TSMF("END OF STREAM PROCESSING!");

				if (stream->decoder && stream->decoder->BufferLevel)
					stream->currentBufferLevel = stream->decoder->BufferLevel(stream->decoder);
				else
					stream->currentBufferLevel = 1;

				USleep(1000);
			}

			tsmf_send_eos_response(stream->eos_channel_callback, stream->eos_message_id);
			stream->eos = 0;

			if (stream->delayed_stop)
			{
				DEBUG_TSMF("Finishing delayed stream stop, now that eos has processed.");
				tsmf_stream_flush(stream);

				if (stream->decoder && stream->decoder->Control)
					stream->decoder->Control(stream->decoder, Control_Stop, NULL);
			}
		}

		/* Stream stopped force all of the acks to happen */
		if (ev == WAIT_OBJECT_0)
		{
			DEBUG_TSMF("ack: Stream stopped!");

			while (1)
			{
				if (tsmf_stream_process_ack(stream, TRUE))
					break;

				USleep(1000);
			}

			break;
		}

		if (tsmf_stream_process_ack(stream, FALSE))
			continue;

		if (stream->currentBufferLevel > stream->minBufferLevel)
			USleep(1000);
	}

	if (error && stream->rdpcontext)
		setChannelError(stream->rdpcontext, error,
		                "tsmf_stream_ack_func reported an error");

	DEBUG_TSMF("out %"PRIu32"", stream->stream_id);
	ExitThread(error);
	return error;
}

static DWORD WINAPI tsmf_stream_playback_func(LPVOID arg)
{
	HANDLE hdl[2];
	TSMF_SAMPLE* sample = NULL;
	TSMF_STREAM* stream = (TSMF_STREAM*) arg;
	TSMF_PRESENTATION* presentation = stream->presentation;
	UINT error = CHANNEL_RC_OK;
	DWORD status;
	DEBUG_TSMF("in %"PRIu32"", stream->stream_id);

	if (stream->major_type == TSMF_MAJOR_TYPE_AUDIO &&
	    stream->sample_rate && stream->channels && stream->bits_per_sample)
	{
		if (stream->decoder)
		{
			if (stream->decoder->GetDecodedData)
			{
				stream->audio = tsmf_load_audio_device(
				                    presentation->audio_name
				                    && presentation->audio_name[0] ? presentation->audio_name : NULL,
				                    presentation->audio_device
				                    && presentation->audio_device[0] ? presentation->audio_device : NULL);

				if (stream->audio)
				{
					stream->audio->SetFormat(stream->audio, stream->sample_rate, stream->channels,
					                         stream->bits_per_sample);
				}
			}
		}
	}

	hdl[0] = stream->stopEvent;
	hdl[1] = Queue_Event(stream->sample_list);

	while (1)
	{
		status = WaitForMultipleObjects(2, hdl, FALSE, 1000);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", error);
			break;
		}

		status = WaitForSingleObject(stream->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

		if (stream->decoder)
			if (stream->decoder->BufferLevel)
				stream->currentBufferLevel = stream->decoder->BufferLevel(stream->decoder);

		sample = tsmf_stream_pop_sample(stream, 0);

		if (sample && !tsmf_sample_playback(sample))
		{
			WLog_ERR(TAG, "error playing sample");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (stream->currentBufferLevel > stream->minBufferLevel)
			USleep(1000);
	}

	if (stream->audio)
	{
		stream->audio->Free(stream->audio);
		stream->audio = NULL;
	}

	if (error && stream->rdpcontext)
		setChannelError(stream->rdpcontext, error,
		                "tsmf_stream_playback_func reported an error");

	DEBUG_TSMF("out %"PRIu32"", stream->stream_id);
	ExitThread(error);
	return error;
}

static BOOL tsmf_stream_start(TSMF_STREAM* stream)
{
	if (!stream || !stream->presentation || !stream->decoder
	    || !stream->decoder->Control)
		return TRUE;

	stream->eos = 0;
	return stream->decoder->Control(stream->decoder, Control_Restart, NULL);
}

static BOOL tsmf_stream_stop(TSMF_STREAM* stream)
{
	if (!stream || !stream->decoder || !stream->decoder->Control)
		return TRUE;

	/* If stopping after eos - we delay until the eos has been processed
	 * this allows us to process any buffers that have been acked even though
	 * they have not actually been completely processes by the decoder
	 */
	if (stream->eos)
	{
		DEBUG_TSMF("Setting up a delayed stop for once the eos has been processed.");
		stream->delayed_stop = 1;
		return TRUE;
	}
	/* Otherwise force stop immediately */
	else
	{
		DEBUG_TSMF("Stop with no pending eos response, so do it immediately.");
		tsmf_stream_flush(stream);
		return stream->decoder->Control(stream->decoder, Control_Stop, NULL);
	}
}

static BOOL tsmf_stream_pause(TSMF_STREAM* stream)
{
	if (!stream || !stream->decoder || !stream->decoder->Control)
		return TRUE;

	return stream->decoder->Control(stream->decoder, Control_Pause, NULL);
}

static BOOL tsmf_stream_restart(TSMF_STREAM* stream)
{
	if (!stream || !stream->decoder || !stream->decoder->Control)
		return TRUE;

	stream->eos = 0;
	return stream->decoder->Control(stream->decoder, Control_Restart, NULL);
}

static BOOL tsmf_stream_change_volume(TSMF_STREAM* stream, UINT32 newVolume,
                                      UINT32 muted)
{
	if (!stream || !stream->decoder)
		return TRUE;

	if (stream->decoder != NULL && stream->decoder->ChangeVolume)
	{
		return stream->decoder->ChangeVolume(stream->decoder, newVolume, muted);
	}
	else if (stream->audio != NULL && stream->audio->ChangeVolume)
	{
		return stream->audio->ChangeVolume(stream->audio, newVolume, muted);
	}

	return TRUE;
}

BOOL tsmf_presentation_volume_changed(TSMF_PRESENTATION* presentation,
                                      UINT32 newVolume, UINT32 muted)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	BOOL ret = TRUE;
	presentation->volume = newVolume;
	presentation->muted = muted;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);
		ret &= tsmf_stream_change_volume(stream, newVolume, muted);
	}

	ArrayList_Unlock(presentation->stream_list);
	return ret;
}

BOOL tsmf_presentation_paused(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	BOOL ret = TRUE;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);
		ret &= tsmf_stream_pause(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
	return ret;
}

BOOL tsmf_presentation_restarted(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	BOOL ret = TRUE;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);
		ret &= tsmf_stream_restart(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
	return ret;
}

BOOL tsmf_presentation_start(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	BOOL ret = TRUE;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);
		ret &= tsmf_stream_start(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT tsmf_presentation_sync(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	UINT error;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		TSMF_STREAM* stream = (TSMF_STREAM*) ArrayList_GetItem(
		                          presentation->stream_list, index);

		if (WaitForSingleObject(stream->ready, 500) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			return error;
		}
	}

	ArrayList_Unlock(presentation->stream_list);
	return CHANNEL_RC_OK;
}

BOOL tsmf_presentation_stop(TSMF_PRESENTATION* presentation)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	BOOL ret = TRUE;
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);
		ret &= tsmf_stream_stop(stream);
	}

	ArrayList_Unlock(presentation->stream_list);
	presentation->audio_start_time = 0;
	presentation->audio_end_time = 0;
	return ret;
}

BOOL tsmf_presentation_set_geometry_info(TSMF_PRESENTATION* presentation,
        UINT32 x, UINT32 y, UINT32 width, UINT32 height, int num_rects, RDP_RECT* rects)
{
	UINT32 index;
	UINT32 count;
	TSMF_STREAM* stream;
	void* tmp_rects = NULL;
	BOOL ret = TRUE;

	/* The server may send messages with invalid width / height.
	 * Ignore those messages. */
	if (!width || !height)
		return TRUE;

	/* Streams can be added/removed from the presentation and the server will resend geometry info when a new stream is
	 * added to the presentation. Also, num_rects is used to indicate whether or not the window is visible.
	 * So, always process a valid message with unchanged position/size and/or no visibility rects.
	 */
	presentation->x = x;
	presentation->y = y;
	presentation->width = width;
	presentation->height = height;
	tmp_rects = realloc(presentation->rects, sizeof(RDP_RECT) * num_rects);

	if (!tmp_rects && num_rects)
		return FALSE;

	presentation->nr_rects = num_rects;
	presentation->rects = tmp_rects;
	CopyMemory(presentation->rects, rects, sizeof(RDP_RECT) * num_rects);
	ArrayList_Lock(presentation->stream_list);
	count = ArrayList_Count(presentation->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (TSMF_STREAM*) ArrayList_GetItem(presentation->stream_list, index);

		if (!stream->decoder)
			continue;

		if (stream->decoder->UpdateRenderingArea)
		{
			ret = stream->decoder->UpdateRenderingArea(stream->decoder, x, y, width, height,
			        num_rects, rects);
		}
	}

	ArrayList_Unlock(presentation->stream_list);
	return ret;
}

void tsmf_presentation_set_audio_device(TSMF_PRESENTATION* presentation,
                                        const char* name, const char* device)
{
	presentation->audio_name = name;
	presentation->audio_device = device;
}

BOOL tsmf_stream_flush(TSMF_STREAM* stream)
{
	BOOL ret = TRUE;

	//TSMF_SAMPLE* sample;
	/* TODO: free lists */
	if (stream->audio)
		ret = stream->audio->Flush(stream->audio);

	stream->eos = 0;
	stream->eos_message_id = 0;
	stream->eos_channel_callback = NULL;
	stream->delayed_stop = 0;
	stream->last_end_time = 0;
	stream->next_start_time = 0;

	if (stream->major_type == TSMF_MAJOR_TYPE_AUDIO)
	{
		stream->presentation->audio_start_time = 0;
		stream->presentation->audio_end_time = 0;
	}

	return TRUE;
}

void _tsmf_presentation_free(TSMF_PRESENTATION* presentation)
{
	tsmf_presentation_stop(presentation);
	ArrayList_Clear(presentation->stream_list);
	ArrayList_Free(presentation->stream_list);
	free(presentation->rects);
	ZeroMemory(presentation, sizeof(TSMF_PRESENTATION));
	free(presentation);
}

void tsmf_presentation_free(TSMF_PRESENTATION* presentation)
{
	ArrayList_Remove(presentation_list, presentation);
}

TSMF_STREAM* tsmf_stream_new(TSMF_PRESENTATION* presentation, UINT32 stream_id,
                             rdpContext* rdpcontext)
{
	TSMF_STREAM* stream;
	stream = tsmf_stream_find_by_id(presentation, stream_id);

	if (stream)
	{
		WLog_ERR(TAG, "duplicated stream id %"PRIu32"!", stream_id);
		return NULL;
	}

	stream = (TSMF_STREAM*) calloc(1, sizeof(TSMF_STREAM));

	if (!stream)
	{
		WLog_ERR(TAG, "Calloc failed");
		return NULL;
	}

	stream->minBufferLevel = VIDEO_MIN_BUFFER_LEVEL;
	stream->maxBufferLevel = VIDEO_MAX_BUFFER_LEVEL;
	stream->currentBufferLevel = 1;
	stream->seeking = FALSE;
	stream->eos = 0;
	stream->eos_message_id = 0;
	stream->eos_channel_callback = NULL;
	stream->stream_id = stream_id;
	stream->presentation = presentation;
	stream->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!stream->stopEvent)
		goto error_stopEvent;

	stream->ready = CreateEvent(NULL, TRUE, TRUE, NULL);

	if (!stream->ready)
		goto error_ready;

	stream->sample_list = Queue_New(TRUE, -1, -1);

	if (!stream->sample_list)
		goto error_sample_list;

	stream->sample_list->object.fnObjectFree = tsmf_sample_free;
	stream->sample_ack_list = Queue_New(TRUE, -1, -1);

	if (!stream->sample_ack_list)
		goto error_sample_ack_list;

	stream->sample_ack_list->object.fnObjectFree = tsmf_sample_free;
	stream->play_thread = CreateThread(NULL, 0, tsmf_stream_playback_func,
	                                   stream, CREATE_SUSPENDED, NULL);

	if (!stream->play_thread)
		goto error_play_thread;

	stream->ack_thread = CreateThread(NULL, 0, tsmf_stream_ack_func, stream,
	                                  CREATE_SUSPENDED, NULL);

	if (!stream->ack_thread)
		goto error_ack_thread;

	if (ArrayList_Add(presentation->stream_list, stream) < 0)
		goto error_add;

	stream->rdpcontext = rdpcontext;
	return stream;
error_add:
	SetEvent(stream->stopEvent);

	if (WaitForSingleObject(stream->ack_thread, INFINITE) == WAIT_FAILED)
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", GetLastError());

error_ack_thread:
	SetEvent(stream->stopEvent);

	if (WaitForSingleObject(stream->play_thread, INFINITE) == WAIT_FAILED)
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", GetLastError());

error_play_thread:
	Queue_Free(stream->sample_ack_list);
error_sample_ack_list:
	Queue_Free(stream->sample_list);
error_sample_list:
	CloseHandle(stream->ready);
error_ready:
	CloseHandle(stream->stopEvent);
error_stopEvent:
	free(stream);
	return NULL;
}

void tsmf_stream_start_threads(TSMF_STREAM* stream)
{
	ResumeThread(stream->play_thread);
	ResumeThread(stream->ack_thread);
}

TSMF_STREAM* tsmf_stream_find_by_id(TSMF_PRESENTATION* presentation,
                                    UINT32 stream_id)
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

static void tsmf_stream_resync(void* arg)
{
	TSMF_STREAM* stream = arg;
	ResetEvent(stream->ready);
}

BOOL tsmf_stream_set_format(TSMF_STREAM* stream, const char* name, wStream* s)
{
	TS_AM_MEDIA_TYPE mediatype;
	BOOL ret = TRUE;

	if (stream->decoder)
	{
		WLog_ERR(TAG, "duplicated call");
		return FALSE;
	}

	if (!tsmf_codec_parse_media_type(&mediatype, s))
	{
		WLog_ERR(TAG, "unable to parse media type");
		return FALSE;
	}

	if (mediatype.MajorType == TSMF_MAJOR_TYPE_VIDEO)
	{
		DEBUG_TSMF("video width %"PRIu32" height %"PRIu32" bit_rate %"PRIu32" frame_rate %f codec_data %"PRIu32"",
		           mediatype.Width, mediatype.Height, mediatype.BitRate,
		           (double) mediatype.SamplesPerSecond.Numerator / (double)
		           mediatype.SamplesPerSecond.Denominator,
		           mediatype.ExtraDataSize);
		stream->minBufferLevel = VIDEO_MIN_BUFFER_LEVEL;
		stream->maxBufferLevel = VIDEO_MAX_BUFFER_LEVEL;
	}
	else if (mediatype.MajorType == TSMF_MAJOR_TYPE_AUDIO)
	{
		DEBUG_TSMF("audio channel %"PRIu32" sample_rate %"PRIu32" bits_per_sample %"PRIu32" codec_data %"PRIu32"",
		           mediatype.Channels, mediatype.SamplesPerSecond.Numerator,
		           mediatype.BitsPerSample,
		           mediatype.ExtraDataSize);
		stream->sample_rate = mediatype.SamplesPerSecond.Numerator;
		stream->channels = mediatype.Channels;
		stream->bits_per_sample = mediatype.BitsPerSample;

		if (stream->bits_per_sample == 0)
			stream->bits_per_sample = 16;

		stream->minBufferLevel = AUDIO_MIN_BUFFER_LEVEL;
		stream->maxBufferLevel = AUDIO_MAX_BUFFER_LEVEL;
	}

	stream->major_type = mediatype.MajorType;
	stream->width = mediatype.Width;
	stream->height = mediatype.Height;
	stream->decoder = tsmf_load_decoder(name, &mediatype);
	ret &= tsmf_stream_change_volume(stream, stream->presentation->volume,
	                                 stream->presentation->muted);

	if (!stream->decoder)
		return FALSE;

	if (stream->decoder->SetAckFunc)
		ret &= stream->decoder->SetAckFunc(stream->decoder, tsmf_stream_process_ack,
		                                   stream);

	if (stream->decoder->SetSyncFunc)
		ret &= stream->decoder->SetSyncFunc(stream->decoder, tsmf_stream_resync,
		                                    stream);

	return ret;
}

void tsmf_stream_end(TSMF_STREAM* stream, UINT32 message_id,
                     IWTSVirtualChannelCallback* pChannelCallback)
{
	if (!stream)
		return;

	stream->eos = 1;
	stream->eos_message_id = message_id;
	stream->eos_channel_callback = pChannelCallback;
}

void _tsmf_stream_free(TSMF_STREAM* stream)
{
	if (!stream)
		return;

	tsmf_stream_stop(stream);
	SetEvent(stream->stopEvent);

	if (stream->play_thread)
	{
		if (WaitForSingleObject(stream->play_thread, INFINITE) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", GetLastError());
			return;
		}

		CloseHandle(stream->play_thread);
		stream->play_thread = NULL;
	}

	if (stream->ack_thread)
	{
		if (WaitForSingleObject(stream->ack_thread, INFINITE) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", GetLastError());
			return;
		}

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

BOOL tsmf_stream_push_sample(TSMF_STREAM* stream,
                             IWTSVirtualChannelCallback* pChannelCallback,
                             UINT32 sample_id, UINT64 start_time, UINT64 end_time, UINT64 duration,
                             UINT32 extensions,
                             UINT32 data_size, BYTE* data)
{
	TSMF_SAMPLE* sample;
	SetEvent(stream->ready);

	if (TERMINATING)
		return TRUE;

	sample = (TSMF_SAMPLE*) calloc(1, sizeof(TSMF_SAMPLE));

	if (!sample)
	{
		WLog_ERR(TAG, "calloc sample failed!");
		return FALSE;
	}

	sample->sample_id = sample_id;
	sample->start_time = start_time;
	sample->end_time = end_time;
	sample->duration = duration;
	sample->extensions = extensions;

	if ((sample->extensions & 0x00000080) || (sample->extensions & 0x00000040))
		sample->invalidTimestamps = TRUE;
	else
		sample->invalidTimestamps = FALSE;

	sample->stream = stream;
	sample->channel_callback = pChannelCallback;
	sample->data_size = data_size;
	sample->data = calloc(1, data_size + TSMF_BUFFER_PADDING_SIZE);

	if (!sample->data)
	{
		WLog_ERR(TAG, "calloc sample->data failed!");
		free(sample);
		return FALSE;
	}

	CopyMemory(sample->data, data, data_size);
	return Queue_Enqueue(stream->sample_list, sample);
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

BOOL tsmf_media_init(void)
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

		if (!presentation_list)
			return FALSE;

		ArrayList_Object(presentation_list)->fnObjectFree = (OBJECT_FREE_FN)
		        _tsmf_presentation_free;
	}

	return TRUE;
}
