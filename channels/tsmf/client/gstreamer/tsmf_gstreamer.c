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

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include "tsmf_constants.h"
#include "tsmf_decoder.h"
#include "tsmf_platform.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

static const char *NAME_GST_STATE_PLAYING = "GST_STATE_PLAYING";
static const char *NAME_GST_STATE_PAUSED = "GST_STATE_PAUSED";
static const char *NAME_GST_STATE_READY = "GST_STATE_READY";
static const char *NAME_GST_STATE_NULL = "GST_STATE_NULL";
static const char *NAME_GST_STATE_VOID_PENDING = "GST_STATE_VOID_PENDING";
static const char *NAME_GST_STATE_OTHER = "GST_STATE_?";

static BOOL tsmf_gstreamer_pipeline_build(TSMFGstreamerDecoder *mdecoder);
static void tsmf_gstreamer_clean_up(TSMFGstreamerDecoder *mdecoder);
static void tsmf_gstreamer_clean_up_pad(TSMFGstreamerDecoder *mdecoder);
static int tsmf_gstreamer_pipeline_set_state(TSMFGstreamerDecoder *mdecoder,
		GstState desired_state);

const char *get_type(TSMFGstreamerDecoder *mdecoder)
{
	assert(mdecoder);
	if(mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		return "VIDEO";
	else
		return "AUDIO";
}

static void tsmf_gstreamer_enough_data(GstAppSrc *src, gpointer user_data)
{
	TSMFGstreamerDecoder *mdecoder = user_data;
	(void)mdecoder;
	DEBUG_TSMF("%s", get_type(mdecoder));
}

static void tsmf_gstreamer_need_data(GstAppSrc *src, guint length, gpointer user_data)
{
	TSMFGstreamerDecoder *mdecoder = user_data;
	(void)mdecoder;
	DEBUG_TSMF("%s length=%lu", get_type(mdecoder), length);
}

static gboolean tsmf_gstreamer_seek_data(GstAppSrc *src, guint64 offset, gpointer user_data)
{
	TSMFGstreamerDecoder *mdecoder = user_data;
	(void)mdecoder;
	DEBUG_TSMF("%s offset=%llu", get_type(mdecoder), offset);
	if(!mdecoder->paused)
		gst_element_set_state(mdecoder->pipe, GST_STATE_PAUSED);
	gst_app_src_end_of_stream((GstAppSrc *)mdecoder->src);
	if(!mdecoder->paused)
		gst_element_set_state(mdecoder->pipe, GST_STATE_PLAYING);
	if(mdecoder->sync_cb)
		mdecoder->sync_cb(mdecoder->stream);
	return TRUE;
}

static inline const GstClockTime tsmf_gstreamer_timestamp_ms_to_gst(UINT64 ms_timestamp)
{
	/*
	 * Convert Microsoft 100ns timestamps to Gstreamer 1ns units.
	 */
	return (GstClockTime)(ms_timestamp * 100);
}

static const char *tsmf_gstreamer_state_name(GstState state)
{
	const char *name;
	if(state == GST_STATE_PLAYING)
		name = NAME_GST_STATE_PLAYING;
	else
		if(state == GST_STATE_PAUSED)
			name = NAME_GST_STATE_PAUSED;
		else
			if(state == GST_STATE_READY)
				name = NAME_GST_STATE_READY;
			else
				if(state == GST_STATE_NULL)
					name = NAME_GST_STATE_NULL;
				else
					if(state == GST_STATE_VOID_PENDING)
						name = NAME_GST_STATE_VOID_PENDING;
					else
						name = NAME_GST_STATE_OTHER;
	return name;
}

int tsmf_gstreamer_pipeline_set_state(TSMFGstreamerDecoder *mdecoder, GstState desired_state)
{
	GstStateChangeReturn state_change;
	const char *name;
	if(!mdecoder)
		return 0;
	if(!mdecoder->pipe)
		return 0;  /* Just in case this is called during startup or shutdown when we don't expect it */
	if(desired_state == mdecoder->state)
		return 0;  /* Redundant request - Nothing to do */
	name = tsmf_gstreamer_state_name(desired_state); /* For debug */
	DEBUG_TSMF("%s to %s", get_type(mdecoder), name);
	state_change = gst_element_set_state(mdecoder->pipe, desired_state);
	if(state_change == GST_STATE_CHANGE_FAILURE)
		DEBUG_WARN("(%s) GST_STATE_CHANGE_FAILURE.", name);
	else
		if(state_change == GST_STATE_CHANGE_ASYNC)
		{
			DEBUG_WARN("(%s) GST_STATE_CHANGE_ASYNC.", name);
			mdecoder->state = desired_state;
		}
		else
			mdecoder->state = desired_state;
	return 0;
}

static GstBuffer *tsmf_get_buffer_from_data(const void *raw_data, gsize size)
{
	gpointer data;
	GstMemory *mem;
	GstBuffer *buffer = gst_buffer_new();
	assert(buffer);
	assert(size > 0);
	data = g_malloc(size);
	memcpy(data, raw_data, size);
	mem = gst_memory_new_wrapped(0, data, size, 0, size, data, g_free);
	gst_buffer_insert_memory(buffer, -1, mem);
	return buffer;
}

static GstBuffer *tsmf_get_buffer_from_payload(TS_AM_MEDIA_TYPE *media_type)
{
	return tsmf_get_buffer_from_data(media_type->ExtraData, media_type->ExtraDataSize);
}

static BOOL tsmf_gstreamer_set_format(ITSMFDecoder *decoder, TS_AM_MEDIA_TYPE *media_type)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	if(!mdecoder)
		return FALSE;
	DEBUG_TSMF("");
	switch(media_type->MajorType)
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
	switch(media_type->SubType)
	{
		case TSMF_SUB_TYPE_WVC1:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-wmv",
								 "bitrate", G_TYPE_UINT, media_type->BitRate,
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "wmvversion", G_TYPE_INT, 3,
								 "format", G_TYPE_STRING, "WVC1",
								 //"framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								 NULL);
			break;
		case TSMF_SUB_TYPE_MP4S:
			mdecoder->gst_caps =  gst_caps_new_simple("video/x-divx",
								  "divxversion", G_TYPE_INT, 5,
								  "bitrate", G_TYPE_UINT, media_type->BitRate,
								  "width", G_TYPE_INT, media_type->Width,
								  "height", G_TYPE_INT, media_type->Height,
								  NULL);
			break;
		case TSMF_SUB_TYPE_MP42:
			mdecoder->gst_caps =  gst_caps_new_simple("video/x-msmpeg",
								  "msmpegversion", G_TYPE_INT, 42,
								  "bitrate", G_TYPE_UINT, media_type->BitRate,
								  "width", G_TYPE_INT, media_type->Width,
								  "height", G_TYPE_INT, media_type->Height,
								  NULL);
			break;
		case TSMF_SUB_TYPE_MP43:
			mdecoder->gst_caps =  gst_caps_new_simple("video/x-msmpeg",
								  "bitrate", G_TYPE_UINT, media_type->BitRate,
								  "width", G_TYPE_INT, media_type->Width,
								  "height", G_TYPE_INT, media_type->Height,
								  "format", G_TYPE_STRING, "MP43",
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
								 "format", G_TYPE_STRING, "WMV1",
								 NULL);
			break;
		case TSMF_SUB_TYPE_WMV2:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-wmv",
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "wmvversion", G_TYPE_INT, 2,
								 "format", G_TYPE_STRING, "WMV2",
								 NULL);
			break;
		case TSMF_SUB_TYPE_WMV3:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-wmv",
								 "bitrate", G_TYPE_UINT, media_type->BitRate,
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 "wmvversion", G_TYPE_INT, 3,
								 "format", G_TYPE_STRING, "WMV3",
								 //"framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
								 NULL);
			break;
		case TSMF_SUB_TYPE_AVC1:
		case TSMF_SUB_TYPE_H264:
			mdecoder->gst_caps = gst_caps_new_simple("video/x-h264",
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 //"framerate", GST_TYPE_FRACTION, media_type->SamplesPerSecond.Numerator, media_type->SamplesPerSecond.Denominator,
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
			if(media_type->ExtraData)
			{
				media_type->ExtraData += 12;
				media_type->ExtraDataSize -= 12;
			}
			mdecoder->gst_caps = gst_caps_new_simple("audio/mpeg",
								 "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								 "channels", G_TYPE_INT, media_type->Channels,
								 "mpegversion", G_TYPE_INT, 4,
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
			mdecoder->gst_caps = gst_caps_new_simple("video/x-raw-yuv",
								 "format", G_TYPE_STRING, "YUY2",
								 //"bitrate", G_TYPE_UINT, media_type->BitRate,
								 "width", G_TYPE_INT, media_type->Width,
								 "height", G_TYPE_INT, media_type->Height,
								 NULL);
			break;
		case TSMF_SUB_TYPE_MP2V:
			mdecoder->gst_caps = gst_caps_new_simple("video/mpeg",
								 //"bitrate", G_TYPE_UINT, media_type->BitRate,
								 //"width", G_TYPE_INT, media_type->Width,
								 //"height", G_TYPE_INT, media_type->Height,
								 "mpegversion", G_TYPE_INT, 2,
								 "systemstream", G_TYPE_BOOLEAN, FALSE,
								 NULL);
			break;
		case TSMF_SUB_TYPE_MP2A:
			mdecoder->gst_caps =  gst_caps_new_simple("audio/mpeg",
								  "mpegversion", G_TYPE_INT, 2,
								  "rate", G_TYPE_INT, media_type->SamplesPerSecond.Numerator,
								  "channels", G_TYPE_INT, media_type->Channels,
								  NULL);
			break;
		default:
			DEBUG_WARN("unknown format:(%d).", media_type->SubType);
			return FALSE;
	}
	if(media_type->ExtraDataSize > 0)
	{
		GValue val = G_VALUE_INIT;
		GstBuffer *buffer = tsmf_get_buffer_from_payload(media_type);
		DEBUG_TSMF("Extra data available (%d)", media_type->ExtraDataSize);
		g_value_init(&val, GST_TYPE_BUFFER);
		g_value_set_boxed(&val, buffer);
		gst_caps_set_value(mdecoder->gst_caps, "codec_data", &val);
	}
	DEBUG_TSMF("%p format '%s'", mdecoder, gst_caps_to_string(mdecoder->gst_caps));
	tsmf_platform_set_format(mdecoder);
	/* Create the pipeline... */
	if(!tsmf_gstreamer_pipeline_build(mdecoder))
		return FALSE;
	return TRUE;
}

void tsmf_gstreamer_clean_up_pad(TSMFGstreamerDecoder *mdecoder)
{
	if(!mdecoder || !mdecoder->pipe)
		return;
	mdecoder->outconv = NULL;
	mdecoder->outresample = NULL;
	mdecoder->outsink = NULL;
	mdecoder->volume = NULL;
}

void tsmf_gstreamer_clean_up(TSMFGstreamerDecoder *mdecoder)
{
	//Cleaning up elements
	if(!mdecoder || !mdecoder->pipe)
		return;
	if(mdecoder->pipe && GST_OBJECT_REFCOUNT_VALUE(mdecoder->pipe) > 0)
	{
		gst_element_set_state(mdecoder->pipe, GST_STATE_NULL);
		gst_object_unref(mdecoder->pipe);
	}
	tsmf_gstreamer_clean_up_pad(mdecoder);
	tsmf_window_destroy(mdecoder);
	mdecoder->ready = FALSE;
	mdecoder->pipe = NULL;
	mdecoder->src = NULL;
	mdecoder->queue = NULL;
	mdecoder->decbin = NULL;
	mdecoder->outbin = NULL;
}

static void
cb_newpad(GstElement *decodebin,
		  GstPad     *pad,
		  gpointer    data)
{
	GstPad *outpad;
	TSMFGstreamerDecoder *mdecoder = data;
	/* only link once */
	outpad = gst_element_get_static_pad(mdecoder->outbin, "sink");
	if(GST_PAD_IS_LINKED(outpad))
	{
		DEBUG_WARN("sink already linded!");
		gst_object_unref(outpad);
		return;
	}
	/* link'n'play */
	gst_pad_link(pad, outpad);
	gst_object_unref(outpad);
}

static void
cb_freepad(GstElement *decodebin,
		   GstPad     *pad,
		   gpointer    data)
{
	GstPad *outpad;
	TSMFGstreamerDecoder *mdecoder = data;
	outpad = gst_element_get_static_pad(mdecoder->outbin, "sink");
	if(!outpad)
	{
		DEBUG_WARN("sink pad does not exist!");
		return;
	}
	if(!GST_PAD_IS_LINKED(outpad))
	{
		DEBUG_WARN("sink not linded!");
		gst_object_unref(outpad);
		return;
	}
	/* link'n'play */
	gst_pad_unlink(pad, outpad);
	gst_object_unref(outpad);
}

void tsmf_gstreamer_remove_pad(TSMFGstreamerDecoder *mdecoder)
{
	if(mdecoder->outbin)
	{
		gst_element_remove_pad(mdecoder->outbin, mdecoder->ghost_pad);
		gst_object_unref(mdecoder->ghost_pad);
		mdecoder->ghost_pad = NULL;
	}
}

BOOL tsmf_gstreamer_add_pad(TSMFGstreamerDecoder *mdecoder)
{
	GstPad *out_pad;
	gboolean linkResult = FALSE;
	switch(mdecoder->media_type)
	{
		case TSMF_MAJOR_TYPE_VIDEO:
			{
				mdecoder->outconv = gst_element_factory_make("videoconvert", "videoconvert");
				mdecoder->outsink = gst_element_factory_make(tsmf_platform_get_video_sink(), "videosink");
				mdecoder->outresample = gst_element_factory_make("videoscale", "videoscale");
				mdecoder->volume = NULL;
				DEBUG_TSMF("building Video Pipe");
				break;
			}
		case TSMF_MAJOR_TYPE_AUDIO:
			{
				mdecoder->outconv = gst_element_factory_make("audioconvert", "audioconvert");
				mdecoder->outresample = gst_element_factory_make("audioresample", "audioresample");
				mdecoder->volume = gst_element_factory_make("volume", "audiovolume");
				mdecoder->outsink = gst_element_factory_make(tsmf_platform_get_audio_sink(), "audiosink");
				DEBUG_TSMF("building Audio Pipe");
				break;
			}
		default:
			DEBUG_WARN("Invalid media type %08X", mdecoder->media_type);
			tsmf_gstreamer_clean_up_pad(mdecoder);
			return FALSE;
	}
	/* Add audio / video specific elements to outbin */
	if(mdecoder->outconv)
		gst_bin_add(GST_BIN(mdecoder->outbin), mdecoder->outconv);
	if(mdecoder->outresample)
		gst_bin_add(GST_BIN(mdecoder->outbin), mdecoder->outresample);
	if(mdecoder->volume)
		gst_bin_add(GST_BIN(mdecoder->outbin), mdecoder->volume);
	if(mdecoder->outsink)
		gst_bin_add(GST_BIN(mdecoder->outbin), mdecoder->outsink);
	if(!mdecoder->outconv || ! mdecoder->outsink || !mdecoder->outresample)
	{
		DEBUG_WARN("Failed to load (some) output pipe elements");
		DEBUG_WARN("converter=%p, sink=%p, resample=%p",
				   mdecoder->outconv, mdecoder->outsink, mdecoder->outresample);
		tsmf_gstreamer_clean_up_pad(mdecoder);
		return FALSE;
	}
	if(mdecoder->media_type == TSMF_MAJOR_TYPE_AUDIO)
	{
		if(!mdecoder->volume)
		{
			DEBUG_WARN("Failed to load (some) audio output pipe elements");
			DEBUG_WARN("volume=%p", mdecoder->volume);
			tsmf_gstreamer_clean_up_pad(mdecoder);
			return FALSE;
		}
	}
	out_pad = gst_element_get_static_pad(mdecoder->outconv, "sink");
	if(mdecoder->media_type == TSMF_MAJOR_TYPE_AUDIO)
	{
		g_object_set(mdecoder->volume, "mute", mdecoder->gstMuted, NULL);
		g_object_set(mdecoder->volume, "volume", mdecoder->gstVolume, NULL);
		linkResult = gst_element_link_many(mdecoder->outconv, mdecoder->outresample,
										   mdecoder->volume, mdecoder->outsink, NULL);
	}
	else
	{
		linkResult = gst_element_link_many(mdecoder->outconv,
										   mdecoder->outresample, mdecoder->outsink, NULL);
	}
	if(!linkResult)
	{
		DEBUG_WARN("Failed to link these elements: converter->sink");
		tsmf_gstreamer_clean_up_pad(mdecoder);
		return FALSE;
	}
	mdecoder->ghost_pad = gst_ghost_pad_new("sink", out_pad);
	gst_element_add_pad(mdecoder->outbin, mdecoder->ghost_pad);
	gst_object_unref(out_pad);
	return TRUE;
}

BOOL tsmf_gstreamer_pipeline_build(TSMFGstreamerDecoder *mdecoder)
{
	gboolean linkResult;
	if(!mdecoder)
		return FALSE;
	mdecoder->pipe = gst_pipeline_new("pipeline");
	if(!mdecoder->pipe)
	{
		DEBUG_WARN("Failed to create new pipe");
		return FALSE;
	}
	tsmf_platform_register_handler(mdecoder);
	mdecoder->src = gst_element_factory_make("appsrc", "source");
	mdecoder->queue = gst_element_factory_make("queue2", "queue");
	mdecoder->decbin = gst_element_factory_make("decodebin", "decoder");
	mdecoder->outbin = gst_bin_new("outbin");
	/* Add all elements to the bin allowing proper resource cleanup,
	 * if any step failed. */
	if(mdecoder->src)
		gst_bin_add(GST_BIN(mdecoder->pipe), mdecoder->src);
	if(mdecoder->queue)
		gst_bin_add(GST_BIN(mdecoder->pipe), mdecoder->queue);
	if(mdecoder->decbin)
		gst_bin_add(GST_BIN(mdecoder->pipe), mdecoder->decbin);
	if(mdecoder->outbin)
		gst_bin_add(GST_BIN(mdecoder->pipe), mdecoder->outbin);
	/* Check, if everything was loaded correctly. */
	if(!mdecoder->src || !mdecoder->queue || !mdecoder->decbin ||
			!mdecoder->outbin)
	{
		DEBUG_WARN("Failed to load (some) pipeline stage");
		DEBUG_WARN("src=%p, queue=%p, decoder=%p, output=%p",
				   mdecoder->src, mdecoder->queue, mdecoder->decbin, mdecoder->outbin);
		tsmf_gstreamer_clean_up(mdecoder);
		return FALSE;
	}
	gst_pipeline_set_delay((GstPipeline *)mdecoder->pipe, 5000);
	/* AppSrc settings */
	GstAppSrcCallbacks callbacks =
	{
		tsmf_gstreamer_need_data,
		tsmf_gstreamer_enough_data,
		tsmf_gstreamer_seek_data
	};
	g_object_set(mdecoder->src, "format", GST_FORMAT_TIME, NULL);
	g_object_set(mdecoder->src, "is-live", TRUE, NULL);
	g_object_set(mdecoder->src, "block", TRUE, NULL);
	gst_app_src_set_callbacks((GstAppSrc *)mdecoder->src, &callbacks, mdecoder, NULL);
	gst_app_src_set_stream_type((GstAppSrc *) mdecoder->src, GST_APP_STREAM_TYPE_SEEKABLE);
	gst_app_src_set_caps((GstAppSrc *) mdecoder->src, mdecoder->gst_caps);
	/* Queue2 settings */
	g_object_set(mdecoder->queue, "use-buffering", FALSE, NULL);
	//    g_object_set(mdecoder->queue, "use-rate-estimate", TRUE, NULL);
	g_object_set(mdecoder->queue, "max-size-buffers", 2, NULL);
	/* DecodeBin settings */
	g_signal_connect(mdecoder->decbin, "pad-added", G_CALLBACK(cb_newpad), mdecoder);
	g_signal_connect(mdecoder->decbin, "pad-removed", G_CALLBACK(cb_freepad), mdecoder);
	/* Sink settings */
	//    g_object_set(mdecoder->outsink, "async-handling", TRUE, NULL);
	linkResult = gst_element_link_many(mdecoder->src, mdecoder->queue,
									   mdecoder->decbin, NULL);
	if(!linkResult)
	{
		DEBUG_WARN("Failed to link elements.");
		tsmf_gstreamer_clean_up(mdecoder);
		return FALSE;
	}
	tsmf_gstreamer_add_pad(mdecoder);
	tsmf_window_create(mdecoder);
	tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_READY);
	tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PLAYING);
	mdecoder->pipeline_start_time_valid = 0;
	mdecoder->shutdown = 0;
	return TRUE;
}

static BOOL tsmf_gstreamer_decodeEx(ITSMFDecoder *decoder, const BYTE *data, UINT32 data_size, UINT32 extensions,
									UINT64 start_time, UINT64 end_time, UINT64 duration)
{
	GstBuffer *gst_buf;
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	UINT64 sample_time = tsmf_gstreamer_timestamp_ms_to_gst(start_time);
	UINT64 sample_duration = tsmf_gstreamer_timestamp_ms_to_gst(duration);
	if(!mdecoder)
	{
		DEBUG_WARN("Decoder not initialized!");
		return FALSE;
	}
	/*
	 * This function is always called from a stream-specific thread.
	 * It should be alright to block here if necessary.
	 * We don't expect to block here often, since the pipeline should
	 * have more than enough buffering.
	 */
	DEBUG_TSMF("%s. Start:(%llu) End:(%llu) Duration:(%llu) Last End:(%llu)",
			   get_type(mdecoder), start_time, end_time, duration,
			   mdecoder->last_sample_end_time);
	if(mdecoder->gst_caps == NULL)
	{
		DEBUG_WARN("tsmf_gstreamer_set_format not called or invalid format.");
		return FALSE;
	}
	if(!mdecoder->src)
	{
		DEBUG_WARN("failed to construct pipeline correctly. Unable to push buffer to source element.");
		return FALSE;
	}
	gst_buf = tsmf_get_buffer_from_data(data, data_size);
	if(gst_buf == NULL)
	{
		DEBUG_WARN("tsmf_get_buffer_from_data(%p, %d) failed.", data, data_size);
		return FALSE;
	}
	if(mdecoder->pipeline_start_time_valid)
	{
		long long diff = start_time;
		diff -= mdecoder->last_sample_end_time;
		if(diff < 0)
			diff *= -1;
		/* The pipe is initialized, but there is a discontinuity.
		 * Seek to the start position... */
		if(diff > 50)
		{
			DEBUG_TSMF("%s seeking to %lld", get_type(mdecoder), start_time);
			if(!gst_element_seek(mdecoder->pipe, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
								 GST_SEEK_TYPE_SET, sample_time,
								 GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
			{
				DEBUG_WARN("seek failed");
			}
			mdecoder->pipeline_start_time_valid = 0;
		}
	}
	else
	{
		DEBUG_TSMF("%s start time %llu", get_type(mdecoder), sample_time);
		mdecoder->pipeline_start_time_valid = 1;
	}
	GST_BUFFER_PTS(gst_buf) = sample_time;
	GST_BUFFER_DURATION(gst_buf) = sample_duration;
	gst_app_src_push_buffer(GST_APP_SRC(mdecoder->src), gst_buf);
	if(mdecoder->ack_cb)
		mdecoder->ack_cb(mdecoder->stream, TRUE);
	mdecoder->last_sample_end_time = end_time;
	if(GST_STATE(mdecoder->pipe) != GST_STATE_PLAYING)
	{
		DEBUG_TSMF("state=%s", tsmf_gstreamer_state_name(GST_STATE(mdecoder->pipe)));
		if(!mdecoder->paused && !mdecoder->shutdown && mdecoder->ready)
			tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PLAYING);
	}
	return TRUE;
}

static void tsmf_gstreamer_change_volume(ITSMFDecoder *decoder, UINT32 newVolume, UINT32 muted)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	if(!mdecoder || !mdecoder->pipe)
		return;
	if(mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		return;
	mdecoder->gstMuted = (BOOL) muted;
	DEBUG_TSMF("mute=[%d]", mdecoder->gstMuted);
	mdecoder->gstVolume = (double) newVolume / (double) 10000;
	DEBUG_TSMF("gst_new_vol=[%f]", mdecoder->gstVolume);
	if(!mdecoder->volume)
		return;
	if(!G_IS_OBJECT(mdecoder->volume))
		return;
	g_object_set(mdecoder->volume, "mute", mdecoder->gstMuted, NULL);
	g_object_set(mdecoder->volume, "volume", mdecoder->gstVolume, NULL);
}

static void tsmf_gstreamer_control(ITSMFDecoder *decoder, ITSMFControlMsg control_msg, UINT32 *arg)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	if(!mdecoder)
		return;
	if(control_msg == Control_Pause)
	{
		DEBUG_TSMF("Control_Pause %s", get_type(mdecoder));
		if(mdecoder->paused)
		{
			DEBUG_WARN("%s: Ignoring control PAUSE, already received!", get_type(mdecoder));
			return;
		}
		tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PAUSED);
		mdecoder->paused = TRUE;
		if(mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
			tsmf_window_pause(mdecoder);
	}
	else
		if(control_msg == Control_Resume)
		{
			DEBUG_TSMF("Control_Resume %s", get_type(mdecoder));
			if(!mdecoder->paused && !mdecoder->shutdown)
			{
				DEBUG_WARN("%s: Ignoring control RESUME, already received!", get_type(mdecoder));
				return;
			}

			mdecoder->paused = FALSE;
			mdecoder->shutdown = FALSE;
			if(mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
				tsmf_window_resume(mdecoder);
			tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PLAYING);
		}
		else
			if(control_msg == Control_Stop)
			{
				DEBUG_WARN("Control_Stop %s", get_type(mdecoder));
				if(mdecoder->shutdown)
				{
					DEBUG_WARN("%s: Ignoring control STOP, already received!", get_type(mdecoder));
					return;
				}
				mdecoder->shutdown = TRUE;
				/* Reset stamps, flush buffers, etc */
				tsmf_gstreamer_pipeline_set_state(mdecoder, GST_STATE_PAUSED);
				if(mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
					tsmf_window_pause(mdecoder);
				gst_app_src_end_of_stream((GstAppSrc *)mdecoder->src);
			}
			else
				DEBUG_WARN("Unknown control message %08x", control_msg);
}

static BOOL tsmf_gstreamer_buffer_filled(ITSMFDecoder *decoder)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("");
	if(!mdecoder)
		return FALSE;
	if(!G_IS_OBJECT(mdecoder->queue))
		return FALSE;
	guint buff_max = 0;
	g_object_get(mdecoder->queue, "max-size-buffers", &buff_max, NULL);
	guint clbuff = 0;
	g_object_get(mdecoder->queue, "current-level-buffers", &clbuff, NULL);
	DEBUG_TSMF("%s buffer fill %u/%u", get_type(mdecoder), clbuff, buff_max);
	return clbuff >= buff_max ? TRUE : FALSE;
}

static void tsmf_gstreamer_free(ITSMFDecoder *decoder)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_WARN("%s", get_type(mdecoder));
	if(mdecoder)
	{
		mdecoder->shutdown = 1;
		tsmf_gstreamer_clean_up(mdecoder);
		if(mdecoder->gst_caps)
			gst_caps_unref(mdecoder->gst_caps);
		tsmf_platform_free(mdecoder);
		memset(mdecoder, 0, sizeof(TSMFGstreamerDecoder));
		free(mdecoder);
		mdecoder = NULL;
	}
}

static UINT64 tsmf_gstreamer_get_running_time(ITSMFDecoder *decoder)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	if(!mdecoder)
		return 0;
	if(!mdecoder->outsink)
		return mdecoder->last_sample_end_time;
	if(GST_STATE(mdecoder->pipe) != GST_STATE_PLAYING)
		return 0;
	GstFormat fmt = GST_FORMAT_TIME;
	gint64 pos = 0;
	gst_element_query_position(mdecoder->outsink, fmt, &pos);
	return pos/100;
}

static void tsmf_gstreamer_update_rendering_area(ITSMFDecoder *decoder,
		int newX, int newY, int newWidth, int newHeight, int numRectangles,
		RDP_RECT *rectangles)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("x=%d, y=%d, w=%d, h=%d, rect=%d", newX, newY, newWidth,
			   newHeight, numRectangles);
	if(mdecoder->media_type == TSMF_MAJOR_TYPE_VIDEO)
		tsmf_window_resize(mdecoder, newX, newY, newWidth, newHeight,
						   numRectangles, rectangles);
}

BOOL tsmf_gstreamer_ack(ITSMFDecoder *decoder, BOOL (*cb)(void *, BOOL), void *stream)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("");
	mdecoder->ack_cb = cb;
	mdecoder->stream = stream;
	return TRUE;
}

BOOL tsmf_gstreamer_sync(ITSMFDecoder *decoder, void (*cb)(void *), void *stream)
{
	TSMFGstreamerDecoder *mdecoder = (TSMFGstreamerDecoder *) decoder;
	DEBUG_TSMF("");
	mdecoder->sync_cb = NULL;
	mdecoder->stream = stream;
	return TRUE;
}

#ifdef STATIC_CHANNELS
#define freerdp_tsmf_client_decoder_subsystem_entry	gstreamer_freerdp_tsmf_client_decoder_subsystem_entry
#endif

ITSMFDecoder *freerdp_tsmf_client_decoder_subsystem_entry(void)
{
	TSMFGstreamerDecoder *decoder;
	if(!gst_is_initialized())
	{
		gst_init(NULL, NULL);
	}
	decoder = malloc(sizeof(TSMFGstreamerDecoder));
	memset(decoder, 0, sizeof(TSMFGstreamerDecoder));
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
	decoder->iface.BufferFilled = tsmf_gstreamer_buffer_filled;
	decoder->iface.SetAckFunc = tsmf_gstreamer_ack;
	decoder->iface.SetSyncFunc = tsmf_gstreamer_sync;
	decoder->paused = FALSE;
	decoder->gstVolume = 0.5;
	decoder->gstMuted = FALSE;
	decoder->state = GST_STATE_VOID_PENDING;  /* No real state yet */
	tsmf_platform_create(decoder);
	return (ITSMFDecoder *) decoder;
}
