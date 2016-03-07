/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - GStreamer Decoder
 *
 * (C) Copyright 2012 HP Development Company, LLC
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <winpr/string.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include "tsmf_constants.h"
#include "tsmf_decoder.h"
#include "tsmf_platform.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

/* 1 second = 10,000,000 100ns units*/
#define SEEK_TOLERANCE 10*1000*1000

static BOOL tsmf_gstreamer_pipeline_build(TSMFGstreamerDecoder* mdecoder);
static void tsmf_gstreamer_clean_up(TSMFGstreamerDecoder* mdecoder);
static int tsmf_gstreamer_pipeline_set_state(TSMFGstreamerDecoder* mdecoder,
		GstState desired_state);
static BOOL tsmf_gstreamer_buffer_level(ITSMFDecoder* decoder);

const char* get_type(TSMFGstreamerDecoder* mdecoder)
{
	if (!mdecoder)
		return NULL;

	switch (mdecoder->media_type)
	{
		case TSMF_MAJOR_TYPE_VIDEO:
			return "VIDEO";
		case TSMF_MAJOR_TYPE_AUDIO:
			return "AUDIO";
		default:
			return "UNKNOWN";
	}
}

static void cb_child_added(GstChildProxy *child_proxy, GObject *object, TSMFGstreamerDecoder* mdecoder)
{
	DEBUG_TSMF("NAME: %s", G_OBJECT_TYPE_NAME(object));

	if (!g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstXvImageSink") || !g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstXImageSink") || !g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstFluVAAutoSink"))
	{
		gst_base_sink_set_max_lateness((GstBaseSink *) object, 10000000); /* nanoseconds */
		g_object_set(G_OBJECT(object), "sync", TRUE, NULL); /* synchronize on the clock */
		g_object_set(G_OBJECT(object), "async", TRUE, NULL); /* no async state changes */
	}

	else if (!g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstAlsaSink") || !g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstPulseSink"))
	{
		gst_base_sink_set_max_lateness((GstBaseSink *) object, 10000000); /* nanoseconds */
		g_object_set(G_OBJECT(object), "slave-method", 1, NULL);
		g_object_set(G_OBJECT(object), "buffer-time", (gint64) 20000, NULL); /* microseconds */
		g_object_set(G_OBJECT(object), "drift-tolerance", (gint64) 20000, NULL); /* microseconds */
		g_object_set(G_OBJECT(object), "latency-time", (gint64) 10000, NULL); /* microseconds */
		g_object_set(G_OBJECT(object), "sync", TRUE, NULL); /* synchronize on the clock */
		g_object_set(G_OBJECT(object), "async", TRUE, NULL); /* no async state changes */
	}
}

static void tsmf_gstreamer_enough_data(GstAppSrc *src, gpointer user_data)
{
	TSMFGstreamerDecoder* mdecoder = user_data;
	(void) mdecoder;
	DEBUG_TSMF("%s", get_type(mdecoder));
}

static void tsmf_gstreamer_need_data(GstAppSrc *src, guint length, gpointer user_data)
{
	TSMFGstreamerDecoder* mdecoder = user_data;
	(void) mdecoder;
	DEBUG_TSMF("%s length=%lu", get_type(mdecoder), length);
}

static gboolean tsmf_gstreamer_seek_data(GstAppSrc *src, guint64 offset, gpointer user_data)
{
	TSMFGstreamerDecoder* mdecoder = user_data;
	(void) mdecoder;
	DEBUG_TSMF("%s offset=%llu", get_type(mdecoder), offset);

	return TRUE;
}

static BOOL tsmf_gstreamer_change_volume(ITSMFDecoder* decoder, UINT32 newVolume, UINT32 muted)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;

	if (!mdecoder || !mdecoder->pipe)
		return TRUE;

	if (mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		return TRUE;

	mdecoder->gstMuted = (BOOL) muted;
	DEBUG_TSMF("mute=[%d]", mdecoder->gstMuted);
	mdecoder->gstVolume = (double) newVolume / (double) 10000;
	DEBUG_TSMF("gst_new_vol=[%f]", mdecoder->gstVolume);

	if (!mdecoder->volume)
		return TRUE;

	if (!G_IS_OBJECT(mdecoder->volume))
		return TRUE;

	g_object_set(mdecoder->volume, "mute", mdecoder->gstMuted, NULL);
	g_object_set(mdecoder->volume, "volume", mdecoder->gstVolume, NULL);

	return TRUE;
}

#ifdef __OpenBSD__
static inline GstClockTime tsmf_gstreamer_timestamp_ms_to_gst(UINT64 ms_timestamp)
#else
static inline const GstClockTime tsmf_gstreamer_timestamp_ms_to_gst(UINT64 ms_timestamp)
#endif
{
	/*
	 * Convert Microsoft 100ns timestamps to Gstreamer 1ns units.
	 */
	return (GstClockTime)(ms_timestamp * 100);
}

int tsmf_gstreamer_pipeline_set_state(TSMFGstreamerDecoder* mdecoder, GstState desired_state)
{
	GstStateChangeReturn state_change;
	const char* name;
	const char* sname = get_type(mdecoder);

	if (!mdecoder)
		return 0;

	if (!mdecoder->pipe)
		return 0;  /* Just in case this is called during startup or shutdown when we don't expect it */

	if (desired_state == mdecoder->state)
		return 0;  /* Redundant request - Nothing to do */

	name = gst_element_state_get_name(desired_state); /* For debug */
	DEBUG_TSMF("%s to %s", sname, name);
	state_change = gst_element_set_state(mdecoder->pipe, desired_state);

	if (state_change == GST_STATE_CHANGE_FAILURE)
	{
		WLog_ERR(TAG, "%s: (%s) GST_STATE_CHANGE_FAILURE.", sname, name);
	}
	else if (state_change == GST_STATE_CHANGE_ASYNC)
	{
		WLog_ERR(TAG, "%s: (%s) GST_STATE_CHANGE_ASYNC.", sname, name);
		mdecoder->state = desired_state;
	}
	else
	{
		mdecoder->state = desired_state;
	}

	return 0;
}

static GstBuffer* tsmf_get_buffer_from_data(const void* raw_data, gsize size)
{
	GstBuffer* buffer;
	gpointer data;

	if (!raw_data)
		return NULL;

	if (size < 1)
		return NULL;

	data = g_malloc(size);

	if (!data)
	{
		WLog_ERR(TAG, "Could not allocate %"G_GSIZE_FORMAT" bytes of data.", size);
		return NULL;
	}

	CopyMemory(data, raw_data, size);

#if GST_VERSION_MAJOR > 0
	buffer = gst_buffer_new_wrapped(data, size);
#else
	buffer = gst_buffer_new();

	if (!buffer)
	{
		WLog_ERR(TAG, "Could not create GstBuffer");
		free(data);
		return NULL;
	}

	GST_BUFFER_MALLOCDATA(buffer) = data;
	GST_BUFFER_SIZE(buffer) = size;
	GST_BUFFER_DATA(buffer) = GST_BUFFER_MALLOCDATA(buffer);
#endif

	return buffer;
}

static BOOL tsmf_gstreamer_set_format(ITSMFDecoder* decoder, TS_AM_MEDIA_TYPE* media_type)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder*) decoder;

	if (!mdecoder)
		return FALSE;

	DEBUG_TSMF("");

	switch (media_type->MajorType)
	{
		case TSMF_MAJOR_TYPE_VIDEO:
			mdecoder->media_type = TSMF_MAJOR_TYPE_VIDEO;
			break;
		case TSMF_MAJOR_TYPE_AUDIO:
			mdecoder->media_type = TSMF_MAJOR_TYPE_AUDIO;
			break;
		default:
			return FALSE;
	}

	switch (media_type->SubType)
	{
		case TSMF_SUB_TYPE_WVC1:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-wmv",
								 "bitrate", G_TYPE_UINT, media_type->BitRate,
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "wmvversion", G_TYPE_INT, 3,
#if GST_VERSION_MAJOR > 0
								 "format", G_TYPE_STRING, "WVC1",
#else
								 "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('W', 'V', 'C', '1'),
#endif
								 "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								 "pixel-aspect-ratio", GST_TYPE_FRACTION, 1 , 1,
								 NULL);
			break;
		case TSMF_SUB_TYPE_MP4S:
			mdecoder->gst_caps =  gst_caps_new_simple("video/x-divx",
								  "divxversion", G_TYPE_INT, 5,
								  "bitrate", G_TYPE_UINT, media_type->BitRate,
								  "width", G_TYPE_INT, media_type->Width,
								  "height", G_TYPE_INT, media_type->Height,
#if GST_VERSION_MAJOR > 0
								  "format", G_TYPE_STRING, "MP42",
#else  
								  "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('M', 'P', '4', '2'),
#endif
								  "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								  NULL);
			break;
		case TSMF_SUB_TYPE_MP42:
			mdecoder->gst_caps =  gst_caps_new_simple("video/x-msmpeg",
								  "msmpegversion", G_TYPE_INT, 42,
								  "bitrate", G_TYPE_UINT, media_type->BitRate,
								  "width", G_TYPE_INT, media_type->Width,
								  "height", G_TYPE_INT, media_type->Height,
#if GST_VERSION_MAJOR > 0                                         
                                                                  "format", G_TYPE_STRING, "MP42",
#else
                                                                  "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('M', 'P', '4', '2'),
#endif
								  "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								  NULL);
			break;
		case TSMF_SUB_TYPE_MP43:
			mdecoder->gst_caps =  gst_caps_new_simple("video/x-msmpeg",
								  "msmpegversion", G_TYPE_INT, 43,
								  "bitrate", G_TYPE_UINT, media_type->BitRate,
								  "width", G_TYPE_INT, media_type->Width,
								  "height", G_TYPE_INT, media_type->Height,
#if GST_VERSION_MAJOR > 0                                         
								  "format", G_TYPE_STRING, "MP43",
#else   
								  "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('M', 'P', '4', '3'),
#endif
								  "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								  NULL);
			break;
		case TSMF_SUB_TYPE_M4S2:
			mdecoder->gst_caps =  gst_caps_new_simple ("video/mpeg",
								   "mpegversion", G_TYPE_INT, 4,
								   "width", G_TYPE_INT, media_type->Width,
								   "height", G_TYPE_INT, media_type->Height,
#if GST_VERSION_MAJOR > 0                                         
								   "format", G_TYPE_STRING, "M4S2",
#else   
								   "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('M', '4', 'S', '2'),
#endif
								   "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								   NULL);
			break;
		case TSMF_SUB_TYPE_WMA9:
			mdecoder->gst_caps =  gst_caps_new_simple("audio/x-wma",
								  "wmaversion", G_TYPE_INT, 3,
								  "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								  "channels", G_TYPE_INT, media_type->Channels,
								  "bitrate", G_TYPE_INT, media_type->BitRate,
								  "depth", G_TYPE_INT, media_type->BitsPerSample,
								  "width", G_TYPE_INT, media_type->BitsPerSample,
								  "block_align", G_TYPE_INT, media_type->BlockAlign,
								  NULL);
			break;
		case TSMF_SUB_TYPE_WMA1:
			mdecoder->gst_caps =  gst_caps_new_simple ("audio/x-wma",
								   "wmaversion", G_TYPE_INT, 1,
								   "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								   "channels", G_TYPE_INT, media_type->Channels,
								   "bitrate", G_TYPE_INT, media_type->BitRate,
								   "depth", G_TYPE_INT, media_type->BitsPerSample,
								   "width", G_TYPE_INT, media_type->BitsPerSample,
								   "block_align", G_TYPE_INT, media_type->BlockAlign,
								   NULL);
			break;
		case TSMF_SUB_TYPE_WMA2:
			mdecoder->gst_caps =  gst_caps_new_simple("audio/x-wma",
								  "wmaversion", G_TYPE_INT, 2,
								  "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								  "channels", G_TYPE_INT, media_type->Channels,
								  "bitrate", G_TYPE_INT, media_type->BitRate,
								  "depth", G_TYPE_INT, media_type->BitsPerSample,
								  "width", G_TYPE_INT, media_type->BitsPerSample,
								  "block_align", G_TYPE_INT, media_type->BlockAlign,
								  NULL);
			break;
		case TSMF_SUB_TYPE_MP3:
			mdecoder->gst_caps =  gst_caps_new_simple("audio/mpeg",
								  "mpegversion", G_TYPE_INT, 1,
								  "layer", G_TYPE_INT, 3,
								  "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								  "channels", G_TYPE_INT, media_type->Channels,
								  NULL);
			break;
		case TSMF_SUB_TYPE_WMV1:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-wmv",
								 "bitrate", G_TYPE_UINT, media_type->BitRate,
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "wmvversion", G_TYPE_INT, 1,
#if GST_VERSION_MAJOR > 0                                         
								 "format", G_TYPE_STRING, "WMV1",
#else   
								 "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('W', 'M', 'V', '1'),
#endif
								 "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								 NULL);
			break;
		case TSMF_SUB_TYPE_WMV2:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-wmv",
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "wmvversion", G_TYPE_INT, 2,
#if GST_VERSION_MAJOR > 0                                         
								 "format", G_TYPE_STRING, "WMV2",
#else
								 "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('W', 'M', 'V', '2'),
#endif
								 "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								 "pixel-aspect-ratio", GST_TYPE_FRACTION, 1 , 1,
								 NULL);
			break;
		case TSMF_SUB_TYPE_WMV3:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-wmv",
								 "bitrate", G_TYPE_UINT, media_type->BitRate,
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "wmvversion", G_TYPE_INT, 3,
#if GST_VERSION_MAJOR > 0                                         
								  "format", G_TYPE_STRING, "WMV3",
#else
								 "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('W', 'M', 'V', '3'),
#endif
								 "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								 "pixel-aspect-ratio", GST_TYPE_FRACTION, 1 , 1,
								 NULL);
			break;
		case TSMF_SUB_TYPE_AVC1:
		case TSMF_SUB_TYPE_H264:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-h264",
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								 "pixel-aspect-ratio", GST_TYPE_FRACTION, 1 , 1,
								 "stream-format", G_TYPE_STRING, "byte-stream",
								 "alignment", G_TYPE_STRING, "nal",
								 NULL);
			break;
		case TSMF_SUB_TYPE_AC3:
			mdecoder->gst_caps =  gst_caps_new_simple("audio/x-ac3",
								  "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								  "channels", G_TYPE_INT, media_type->Channels,
								  NULL);
			break;
		case TSMF_SUB_TYPE_AAC:

			/* For AAC the pFormat is a HEAACWAVEINFO struct, and the codec data
			   is at the end of it. See
			   http://msdn.microsoft.com/en-us/library/dd757806.aspx */
			if (media_type->ExtraData)
			{
				media_type->ExtraData += 12;
				media_type->ExtraDataSize -= 12;
			}

			mdecoder->gst_caps = gst_caps_new_simple("audio/mpeg",
								 "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								 "channels", G_TYPE_INT, media_type->Channels,
								 "mpegversion", G_TYPE_INT, 4,
								 "framed", G_TYPE_BOOLEAN, TRUE,
								 "stream-format", G_TYPE_STRING, "raw",
								 NULL);
			break;
		case TSMF_SUB_TYPE_MP1A:
			mdecoder->gst_caps =  gst_caps_new_simple("audio/mpeg",
								  "mpegversion", G_TYPE_INT, 1,
								  "channels", G_TYPE_INT, media_type->Channels,
								  NULL);
			break;
		case TSMF_SUB_TYPE_MP1V:
			mdecoder->gst_caps = gst_caps_new_simple("video/mpeg",
								 "mpegversion", G_TYPE_INT, 1,
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "systemstream", G_TYPE_BOOLEAN, FALSE,
								 NULL);
			break;
		case TSMF_SUB_TYPE_YUY2:
#if GST_VERSION_MAJOR > 0
			mdecoder->gst_caps = gst_caps_new_simple("video/x-raw",
								 "format", G_TYPE_STRING, "YUY2",
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 NULL);
#else
			mdecoder->gst_caps = gst_caps_new_simple("video/x-raw-yuv",
								 "format", G_TYPE_STRING, "YUY2",
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator, 
								 NULL);
#endif
			break;
		case TSMF_SUB_TYPE_MP2V:
			mdecoder->gst_caps = gst_caps_new_simple("video/mpeg",
								 "mpegversion", G_TYPE_INT, 2,
								 "systemstream", G_TYPE_BOOLEAN, FALSE,
								 NULL);
			break;
		case TSMF_SUB_TYPE_MP2A:
			mdecoder->gst_caps =  gst_caps_new_simple("audio/mpeg",
								  "mpegversion", G_TYPE_INT, 1,
								  "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								  "channels", G_TYPE_INT, media_type->Channels,
								  NULL);
			break;
		case TSMF_SUB_TYPE_FLAC:
			mdecoder->gst_caps =  gst_caps_new_simple("audio/x-flac", "", NULL);
			break;
		default:
			WLog_ERR(TAG, "unknown format:(%d).", media_type->SubType);
			return FALSE;
	}

	if (media_type->ExtraDataSize > 0)
	{
		GstBuffer *buffer;
		DEBUG_TSMF("Extra data available (%d)", media_type->ExtraDataSize);
		buffer = tsmf_get_buffer_from_data(media_type->ExtraData, media_type->ExtraDataSize);

		if (!buffer)
		{
			WLog_ERR(TAG, "could not allocate GstBuffer!");
			return FALSE;
		}

		gst_caps_set_simple(mdecoder->gst_caps, "codec_data", GST_TYPE_BUFFER, buffer, NULL);
	}

	DEBUG_TSMF("%p format '%s'", mdecoder, gst_caps_to_string(mdecoder->gst_caps));
	tsmf_platform_set_format(mdecoder);

	/* Create the pipeline... */
	if (!tsmf_gstreamer_pipeline_build(mdecoder))
		return FALSE;

	return TRUE;
}

void tsmf_gstreamer_clean_up(TSMFGstreamerDecoder* mdecoder)
{
	if (!mdecoder || !mdecoder->pipe)
		return;

	if (mdecoder->pipe && GST_OBJECT_REFCOUNT_VALUE(mdecoder->pipe) > 0)
	{
		tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_NULL);
		gst_object_unref(mdecoder->pipe);
	}

	mdecoder->ready = FALSE;
	mdecoder->paused = FALSE;

	mdecoder->pipe = NULL;
	mdecoder->src = NULL;
	mdecoder->queue = NULL;
}

BOOL tsmf_gstreamer_pipeline_build(TSMFGstreamerDecoder* mdecoder)
{
#if GST_VERSION_MAJOR > 0
	const char* video = "appsrc name=videosource ! queue2 name=videoqueue ! decodebin name=videodecoder !";
        const char* audio = "appsrc name=audiosource ! queue2 name=audioqueue ! decodebin name=audiodecoder ! audioconvert ! audiorate ! audioresample ! volume name=audiovolume !";
#else
	const char* video = "appsrc name=videosource ! queue2 name=videoqueue ! decodebin2 name=videodecoder !";
	const char* audio = "appsrc name=audiosource ! queue2 name=audioqueue ! decodebin2 name=audiodecoder ! audioconvert ! audiorate ! audioresample ! volume name=audiovolume !";
#endif
	char pipeline[1024];

	if (!mdecoder)
		return FALSE;

	/* TODO: Construction of the pipeline from a string allows easy overwrite with arguments.
	 *       The only fixed elements necessary are appsrc and the volume element for audio streams.
	 *       The rest could easily be provided in gstreamer pipeline notation from command line. */
	if (mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		sprintf_s(pipeline, sizeof(pipeline), "%s %s name=videosink", video, tsmf_platform_get_video_sink());
	else
		sprintf_s(pipeline, sizeof(pipeline), "%s %s name=audiosink", audio, tsmf_platform_get_audio_sink());

	DEBUG_TSMF("pipeline=%s", pipeline);
	mdecoder->pipe = gst_parse_launch(pipeline, NULL);

	if (!mdecoder->pipe)
	{
		WLog_ERR(TAG, "Failed to create new pipe");
		return FALSE;
	}

	if (mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		mdecoder->src = gst_bin_get_by_name(GST_BIN(mdecoder->pipe), "videosource");
	else
		mdecoder->src = gst_bin_get_by_name(GST_BIN(mdecoder->pipe), "audiosource");

	if (!mdecoder->src)
	{
		WLog_ERR(TAG, "Failed to get appsrc");
		return FALSE;
	}

	if (mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		mdecoder->queue = gst_bin_get_by_name(GST_BIN(mdecoder->pipe), "videoqueue");
	else
		mdecoder->queue = gst_bin_get_by_name(GST_BIN(mdecoder->pipe), "audioqueue");

	if (!mdecoder->queue)
	{
		WLog_ERR(TAG, "Failed to get queue");
		return FALSE;
	}

	if (mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		mdecoder->outsink = gst_bin_get_by_name(GST_BIN(mdecoder->pipe), "videosink");
	else
		mdecoder->outsink = gst_bin_get_by_name(GST_BIN(mdecoder->pipe), "audiosink");

	if (!mdecoder->outsink)
	{
		WLog_ERR(TAG, "Failed to get sink");
		return FALSE;
	}

	g_signal_connect(mdecoder->outsink, "child-added", G_CALLBACK(cb_child_added), mdecoder);

	if (mdecoder->media_type == TSMF_MAJOR_TYPE_AUDIO)
	{
		mdecoder->volume = gst_bin_get_by_name(GST_BIN(mdecoder->pipe), "audiovolume");

		if (!mdecoder->volume)
		{
			WLog_ERR(TAG, "Failed to get volume");
			return FALSE;
		}

		tsmf_gstreamer_change_volume((ITSMFDecoder*)mdecoder, mdecoder->gstVolume*((double) 10000), mdecoder->gstMuted);
	}

	tsmf_platform_register_handler(mdecoder);
	/* AppSrc settings */
	GstAppSrcCallbacks callbacks =
	{
		tsmf_gstreamer_need_data,
		tsmf_gstreamer_enough_data,
		tsmf_gstreamer_seek_data
	};
	g_object_set(mdecoder->src, "format", GST_FORMAT_TIME, NULL);
	g_object_set(mdecoder->src, "is-live", FALSE, NULL);
	g_object_set(mdecoder->src, "block", FALSE, NULL);
	g_object_set(mdecoder->src, "blocksize", 1024, NULL);
	gst_app_src_set_caps((GstAppSrc *) mdecoder->src, mdecoder->gst_caps);
	gst_app_src_set_callbacks((GstAppSrc *)mdecoder->src, &callbacks, mdecoder, NULL);
	gst_app_src_set_stream_type((GstAppSrc *) mdecoder->src, GST_APP_STREAM_TYPE_SEEKABLE);
	gst_app_src_set_latency((GstAppSrc *) mdecoder->src, 0, -1);
	gst_app_src_set_max_bytes((GstAppSrc *) mdecoder->src, (guint64) 0);//unlimited
	g_object_set(G_OBJECT(mdecoder->queue), "use-buffering", FALSE, NULL);
	g_object_set(G_OBJECT(mdecoder->queue), "use-rate-estimate", FALSE, NULL);
	g_object_set(G_OBJECT(mdecoder->queue), "max-size-buffers", 0, NULL);
	g_object_set(G_OBJECT(mdecoder->queue), "max-size-bytes", 0, NULL);
	g_object_set(G_OBJECT(mdecoder->queue), "max-size-time", (guint64) 0, NULL);

	/* Only set these properties if not an autosink, otherwise we will set properties when real sinks are added */
	if (!g_strcmp0(G_OBJECT_TYPE_NAME(mdecoder->outsink), "GstAutoVideoSink") && !g_strcmp0(G_OBJECT_TYPE_NAME(mdecoder->outsink), "GstAutoAudioSink"))
	{
		if (mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		{
			gst_base_sink_set_max_lateness((GstBaseSink *) mdecoder->outsink, 10000000); /* nanoseconds */
		}
		else
		{
			gst_base_sink_set_max_lateness((GstBaseSink *) mdecoder->outsink, 10000000); /* nanoseconds */
			g_object_set(G_OBJECT(mdecoder->outsink), "buffer-time", (gint64) 20000, NULL); /* microseconds */
			g_object_set(G_OBJECT(mdecoder->outsink), "drift-tolerance", (gint64) 20000, NULL); /* microseconds */
			g_object_set(G_OBJECT(mdecoder->outsink), "latency-time", (gint64) 10000, NULL); /* microseconds */
			g_object_set(G_OBJECT(mdecoder->outsink), "slave-method", 1, NULL);
		}
		g_object_set(G_OBJECT(mdecoder->outsink), "sync", TRUE, NULL); /* synchronize on the clock */
		g_object_set(G_OBJECT(mdecoder->outsink), "async", TRUE, NULL); /* no async state changes */
	}

	tsmf_window_create(mdecoder);
	tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_READY);
	tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PLAYING);
	mdecoder->pipeline_start_time_valid = 0;
	mdecoder->shutdown = 0;
	mdecoder->paused = FALSE;

	GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(mdecoder->pipe), GST_DEBUG_GRAPH_SHOW_ALL, get_type(mdecoder));

	return TRUE;
}

static BOOL tsmf_gstreamer_decodeEx(ITSMFDecoder* decoder, const BYTE *data, UINT32 data_size, UINT32 extensions,
									UINT64 start_time, UINT64 end_time, UINT64 duration)
{
	GstBuffer *gst_buf;
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;
	UINT64 sample_time = tsmf_gstreamer_timestamp_ms_to_gst(start_time);
	BOOL useTimestamps = TRUE;

	if (!mdecoder)
	{
		WLog_ERR(TAG, "Decoder not initialized!");
		return FALSE;
	}

	/*
	 * This function is always called from a stream-specific thread.
	 * It should be alright to block here if necessary.
	 * We don't expect to block here often, since the pipeline should
	 * have more than enough buffering.
	 */
	DEBUG_TSMF("%s. Start:(%lu) End:(%lu) Duration:(%d) Last Start:(%lu)",
			   get_type(mdecoder), start_time, end_time, (int)duration,
			   mdecoder->last_sample_start_time);

	if (mdecoder->shutdown)
	{
		WLog_ERR(TAG, "decodeEx called on shutdown decoder");
		return TRUE;
	}

	if (mdecoder->gst_caps == NULL)
	{
		WLog_ERR(TAG, "tsmf_gstreamer_set_format not called or invalid format.");
		return FALSE;
	}

	if (!mdecoder->pipe)
		tsmf_gstreamer_pipeline_build(mdecoder);

	if (!mdecoder->src)
	{
		WLog_ERR(TAG, "failed to construct pipeline correctly. Unable to push buffer to source element.");
		return FALSE;
	}

	gst_buf = tsmf_get_buffer_from_data(data, data_size);

	if (gst_buf == NULL)
	{
		WLog_ERR(TAG, "tsmf_get_buffer_from_data(%p, %d) failed.", data, data_size);
		return FALSE;
	}

	/* Relative timestamping will sometimes be set to 0
	 * so we ignore these timestamps just to be safe(bit 8)
	 */
	if (extensions & 0x00000080)
	{
		DEBUG_TSMF("Ignoring the timestamps - relative - bit 8");
		useTimestamps = FALSE;
	}

	/* If no timestamps exist then we dont want to look at the timestamp values (bit 7) */
	if (extensions & 0x00000040)
	{
		DEBUG_TSMF("Ignoring the timestamps - none - bit 7");
		useTimestamps = FALSE;
	}

	/* If performing a seek */
	if (mdecoder->seeking)
	{
		mdecoder->seeking = FALSE;
		tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PAUSED);
		mdecoder->pipeline_start_time_valid = 0;	
	}

	if (mdecoder->pipeline_start_time_valid)
	{
		DEBUG_TSMF("%s start time %lu", get_type(mdecoder), start_time);

		/* Adjusted the condition for a seek to be based on start time only
		 * WMV1 and WMV2 files in particular have bad end time and duration values
		 * there seems to be no real side effects of just using the start time instead
		 */
		UINT64 minTime = mdecoder->last_sample_start_time - (UINT64) SEEK_TOLERANCE;
		UINT64 maxTime = mdecoder->last_sample_start_time + (UINT64) SEEK_TOLERANCE;

		/* Make sure the minTime stops at 0 , should we be at the beginning of the stream */ 
		if (mdecoder->last_sample_start_time < (UINT64) SEEK_TOLERANCE)
			minTime = 0;    

		/* If the start_time is valid and different from the previous start time by more than the seek tolerance, then we have a seek condition */
		if (((start_time > maxTime) || (start_time < minTime)) && useTimestamps)
		{
			DEBUG_TSMF("tsmf_gstreamer_decodeEx: start_time=[%lu] > last_sample_start_time=[%lu] OR ", start_time, mdecoder->last_sample_start_time);
			DEBUG_TSMF("tsmf_gstreamer_decodeEx: start_time=[%lu] < last_sample_start_time=[%lu] with", start_time, mdecoder->last_sample_start_time);
			DEBUG_TSMF("tsmf_gstreamer_decodeEX: a tolerance of more than [%lu] from the last sample", SEEK_TOLERANCE);
			DEBUG_TSMF("tsmf_gstreamer_decodeEX: minTime=[%lu] maxTime=[%lu]", minTime, maxTime);

			mdecoder->seeking = TRUE;

			/* since we cant make the gstreamer pipeline jump to the new start time after a seek - we just maintain
			 * a offset between realtime and gstreamer time
			 */
			mdecoder->seek_offset = start_time;
		}
	}
	else
	{
		DEBUG_TSMF("%s start time %lu", get_type(mdecoder), start_time);
		/* Always set base/start time to 0. Will use seek offset to translate real buffer times
		 * back to 0. This allows the video to be started from anywhere and the ability to handle seeks
		 * without rebuilding the pipeline, etc. since that is costly
		 */
		gst_element_set_base_time(mdecoder->pipe, tsmf_gstreamer_timestamp_ms_to_gst(0));
		gst_element_set_start_time(mdecoder->pipe, tsmf_gstreamer_timestamp_ms_to_gst(0));
		mdecoder->pipeline_start_time_valid = 1;

		/* Set the seek offset if buffer has valid timestamps. */
		if (useTimestamps)
			mdecoder->seek_offset = start_time;

		if (!gst_element_seek(mdecoder->pipe, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
						GST_SEEK_TYPE_SET, 0,
						GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
		{
			WLog_ERR(TAG, "seek failed");
		}
	}

#if GST_VERSION_MAJOR > 0
	if (useTimestamps)
		GST_BUFFER_PTS(gst_buf) = sample_time - tsmf_gstreamer_timestamp_ms_to_gst(mdecoder->seek_offset);
	else
		GST_BUFFER_PTS(gst_buf) = GST_CLOCK_TIME_NONE;
#else
	if (useTimestamps)
		GST_BUFFER_TIMESTAMP(gst_buf) = sample_time - tsmf_gstreamer_timestamp_ms_to_gst(mdecoder->seek_offset);
	else
		GST_BUFFER_TIMESTAMP(gst_buf) = GST_CLOCK_TIME_NONE;
#endif
	GST_BUFFER_DURATION(gst_buf) = GST_CLOCK_TIME_NONE;
	GST_BUFFER_OFFSET(gst_buf) = GST_BUFFER_OFFSET_NONE;
#if GST_VERSION_MAJOR > 0
#else
	gst_buffer_set_caps(gst_buf, mdecoder->gst_caps);
#endif
	gst_app_src_push_buffer(GST_APP_SRC(mdecoder->src), gst_buf);

	/* Should only update the last timestamps if the current ones are valid */
	if (useTimestamps)
	{
		mdecoder->last_sample_start_time = start_time;
		mdecoder->last_sample_end_time = end_time;
	}

	if (mdecoder->pipe && (GST_STATE(mdecoder->pipe) != GST_STATE_PLAYING))
	{
		DEBUG_TSMF("%s: state=%s", get_type(mdecoder), gst_element_state_get_name(GST_STATE(mdecoder->pipe)));	

		DEBUG_TSMF("%s Paused: %i   Shutdown: %i   Ready: %i", get_type(mdecoder), mdecoder->paused, mdecoder->shutdown, mdecoder->ready);
		if (!mdecoder->paused && !mdecoder->shutdown && mdecoder->ready)
			tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PLAYING);
	}

	return TRUE;
}

static BOOL tsmf_gstreamer_control(ITSMFDecoder* decoder, ITSMFControlMsg control_msg, UINT32 *arg)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;

	if (!mdecoder)
	{
		WLog_ERR(TAG, "Control called with no decoder!");
		return TRUE;
	}

	if (control_msg == Control_Pause)
	{
		DEBUG_TSMF("Control_Pause %s", get_type(mdecoder));

		if (mdecoder->paused)
		{
			WLog_ERR(TAG, "%s: Ignoring Control_Pause, already received!", get_type(mdecoder));
			return TRUE;
		}

		tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PAUSED);
		mdecoder->shutdown = 0;
		mdecoder->paused = TRUE;
	}
	else if (control_msg == Control_Resume)
	{
		DEBUG_TSMF("Control_Resume %s", get_type(mdecoder));

		if (!mdecoder->paused && !mdecoder->shutdown)
		{
			WLog_ERR(TAG, "%s: Ignoring Control_Resume, already received!", get_type(mdecoder));
			return TRUE;
		}

		mdecoder->shutdown = 0;
		mdecoder->paused = FALSE;
	}
	else if (control_msg == Control_Stop)
	{
		DEBUG_TSMF("Control_Stop %s", get_type(mdecoder));

		if (mdecoder->shutdown)
		{
			WLog_ERR(TAG, "%s: Ignoring Control_Stop, already received!", get_type(mdecoder));
			return TRUE;
		}

		/* Reset stamps, flush buffers, etc */
		if (mdecoder->pipe)
		{
			tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_NULL);
			tsmf_window_destroy(mdecoder);
			tsmf_gstreamer_clean_up(mdecoder);
		}
		mdecoder->seek_offset = 0;
		mdecoder->pipeline_start_time_valid = 0;
		mdecoder->shutdown = 1;
	}
	else if (control_msg == Control_Restart)
	{
		DEBUG_TSMF("Control_Restart %s", get_type(mdecoder));
		mdecoder->shutdown = 0;
		mdecoder->paused = FALSE;

		if (mdecoder->pipeline_start_time_valid)
			tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PLAYING);
	}
	else
		WLog_ERR(TAG, "Unknown control message %08x", control_msg);

	return TRUE;
}

static BOOL tsmf_gstreamer_buffer_level(ITSMFDecoder* decoder)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("");

	if (!mdecoder)
		return FALSE;

	guint clbuff = 0;

	if (G_IS_OBJECT(mdecoder->queue))
		g_object_get(mdecoder->queue, "current-level-buffers", &clbuff, NULL);

	DEBUG_TSMF("%s buffer level %u", get_type(mdecoder), clbuff);
	return clbuff;
}

static void tsmf_gstreamer_free(ITSMFDecoder* decoder)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("%s", get_type(mdecoder));

	if (mdecoder)
	{
		tsmf_window_destroy(mdecoder);
		tsmf_gstreamer_clean_up(mdecoder);

		if (mdecoder->gst_caps)
			gst_caps_unref(mdecoder->gst_caps);

		tsmf_platform_free(mdecoder);
		ZeroMemory(mdecoder, sizeof(TSMFGstreamerDecoder));
		free(mdecoder);
		mdecoder = NULL;
	}
}

static UINT64 tsmf_gstreamer_get_running_time(ITSMFDecoder* decoder)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;

	if (!mdecoder)
		return 0;

	if (!mdecoder->outsink)
		return mdecoder->last_sample_start_time;

	if (!mdecoder->pipe)
		return 0;

	GstFormat fmt = GST_FORMAT_TIME;
	gint64 pos = 0;
#if GST_VERSION_MAJOR > 0
	gst_element_query_position(mdecoder->pipe, fmt, &pos);
#else
	gst_element_query_position(mdecoder->pipe, &fmt, &pos);
#endif
	return (UINT64) (pos/100 + mdecoder->seek_offset);
}

static BOOL tsmf_gstreamer_update_rendering_area(ITSMFDecoder* decoder,
		int newX, int newY, int newWidth, int newHeight, int numRectangles,
		RDP_RECT *rectangles)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("x=%d, y=%d, w=%d, h=%d, rect=%d", newX, newY, newWidth,
			   newHeight, numRectangles);

	if (mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
	{
		return tsmf_window_resize(mdecoder, newX, newY, newWidth, newHeight,
						   numRectangles, rectangles) == 0;
	}

	return TRUE;
}

BOOL tsmf_gstreamer_ack(ITSMFDecoder* decoder, BOOL (*cb)(void *, BOOL), void *stream)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("");
	mdecoder->ack_cb = NULL;
	mdecoder->stream = stream;
	return TRUE;
}

BOOL tsmf_gstreamer_sync(ITSMFDecoder* decoder, void (*cb)(void *), void *stream)
{
	TSMFGstreamerDecoder* mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("");
	mdecoder->sync_cb = NULL;
	mdecoder->stream = stream;
	return TRUE;
}

#ifdef STATIC_CHANNELS
#define freerdp_tsmf_client_subsystem_entry	gstreamer_freerdp_tsmf_client_decoder_subsystem_entry
#else
#define freerdp_tsmf_client_subsystem_entry	FREERDP_API freerdp_tsmf_client_decoder_subsystem_entry
#endif

ITSMFDecoder* freerdp_tsmf_client_subsystem_entry(void)
{
	TSMFGstreamerDecoder *decoder;

	if (!gst_is_initialized())
	{
		gst_init(NULL, NULL);
	}

	decoder = calloc(1, sizeof(TSMFGstreamerDecoder));

	if (!decoder)
		return NULL;

	decoder->iface.SetFormat = tsmf_gstreamer_set_format;
	decoder->iface.Decode = NULL;
	decoder->iface.GetDecodedData = NULL;
	decoder->iface.GetDecodedFormat = NULL;
	decoder->iface.GetDecodedDimension = NULL;
	decoder->iface.GetRunningTime = tsmf_gstreamer_get_running_time;
	decoder->iface.UpdateRenderingArea = tsmf_gstreamer_update_rendering_area;
	decoder->iface.Free = tsmf_gstreamer_free;
	decoder->iface.Control = tsmf_gstreamer_control;
	decoder->iface.DecodeEx = tsmf_gstreamer_decodeEx;
	decoder->iface.ChangeVolume = tsmf_gstreamer_change_volume;
	decoder->iface.BufferLevel = tsmf_gstreamer_buffer_level;
	decoder->iface.SetAckFunc = tsmf_gstreamer_ack;
	decoder->iface.SetSyncFunc = tsmf_gstreamer_sync;
	decoder->paused = FALSE;
	decoder->gstVolume = 0.5;
	decoder->gstMuted = FALSE;
	decoder->state = GST_STATE_VOID_PENDING;  /* No real state yet */
	decoder->last_sample_start_time = 0;
	decoder->last_sample_end_time = 0;
	decoder->seek_offset = 0;
	decoder->seeking = FALSE;

	if (tsmf_platform_create(decoder) < 0)
	{
		free(decoder);
		return NULL;
	}

	return (ITSMFDecoder*) decoder;
}
