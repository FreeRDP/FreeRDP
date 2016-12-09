/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - GStreamer Decoder
 * platform specific functions
 *
 * (C) Copyright 2014 Thincast Technologies GmbH
 * (C) Copyright 2014 Armin Novak <armin.novak@thincast.com>
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

#ifndef _TSMF_PLATFORM_H_
#define _TSMF_PLATFORM_H_

#include <gst/gst.h>
#include <tsmf_decoder.h>

typedef struct _TSMFGstreamerDecoder
{
	ITSMFDecoder iface;

	int media_type; /* TSMF_MAJOR_TYPE_AUDIO or TSMF_MAJOR_TYPE_VIDEO */

	gint64 duration;

	GstState state;
	GstCaps *gst_caps;

	GstElement *pipe;
	GstElement *src;
	GstElement *queue;
	GstElement *outsink;
	GstElement *volume;

	BOOL ready;
	BOOL paused;
	UINT64 last_sample_start_time;
	UINT64 last_sample_end_time;
	BOOL seeking;
	UINT64 seek_offset;

	double gstVolume;
	BOOL gstMuted;

	int pipeline_start_time_valid; /* We've set the start time and have not reset the pipeline */
	int shutdown; /* The decoder stream is shutting down */

	void *platform;

	BOOL (*ack_cb)(void *,BOOL);
	void (*sync_cb)(void *);
	void *stream;

} TSMFGstreamerDecoder;

const char* get_type(TSMFGstreamerDecoder* mdecoder);

const char* tsmf_platform_get_video_sink(void);
const char* tsmf_platform_get_audio_sink(void);

int tsmf_platform_create(TSMFGstreamerDecoder* decoder);
int tsmf_platform_set_format(TSMFGstreamerDecoder* decoder);
int tsmf_platform_register_handler(TSMFGstreamerDecoder* decoder);
int tsmf_platform_free(TSMFGstreamerDecoder* decoder);

int tsmf_window_create(TSMFGstreamerDecoder* decoder);
int tsmf_window_resize(TSMFGstreamerDecoder* decoder, int x, int y,
					   int width, int height, int nr_rect, RDP_RECT *visible);
int tsmf_window_destroy(TSMFGstreamerDecoder* decoder);

int tsmf_window_map(TSMFGstreamerDecoder* decoder);
int tsmf_window_unmap(TSMFGstreamerDecoder* decoder);

BOOL tsmf_gstreamer_add_pad(TSMFGstreamerDecoder* mdecoder);
void tsmf_gstreamer_remove_pad(TSMFGstreamerDecoder* mdecoder);

#endif
