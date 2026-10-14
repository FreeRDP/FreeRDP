/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, GStreamer pipeline backend
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

/* A pseudo camera whose frames come from a GStreamer pipeline, selected with
 * /dvc:rdpecam,sys:gst,pipeline:<pipeline>|pipeline-file:<path>|file:<path>
 *     [,name:<camera name>][,loop:<on|off>]
 *
 * The pipeline is given in gst-launch syntax and must produce video, the backend appends the
 * conversion to I420 and the sink. The only media type announced is what the pipeline
 * negotiates (cropped to even dimensions). A pipeline reaching its end is played again unless
 * loop:off is given, live sources (pipewiresrc, v4l2src, ...) never end. */

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/cmdline.h>
#include <winpr/file.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include "camera.h"

#define TAG CHANNELS_TAG("rdpecam-gst.client")

/* the channel name of the device, what a Windows client would use for its first camera */
#define CAM_GST_DEVICE_ID "RDCamera_Device_0"

#define CAM_GST_SINK_NAME "freerdpcamsink"

#define CAM_GST_FRAMERATE_NUMERATOR_DEFAULT 30
#define CAM_GST_FRAMERATE_DENOMINATOR_DEFAULT 1

/* how long the pipeline may take to start, to preroll or to produce its first frame */
#define CAM_GST_START_TIMEOUT (10 * GST_SECOND)

/* longest wait for a frame, so that a stop request is seen quickly */
#define CAM_GST_PULL_TIMEOUT (100 * GST_MSECOND)

typedef struct
{
	ICamHal iHal;

	char* description; /* the complete pipeline, sink included */
	char* name;
	BOOL loop;
	BOOL live;

	/* characteristics negotiated by the pipeline, the only media type announced */
	UINT32 width;
	UINT32 height;
	UINT32 frameRateNumerator;
	UINT32 frameRateDenominator;

	GstElement* pipeline;
	GstAppSink* sink;

	CRITICAL_SECTION lock;
	BOOL streaming;
	HANDLE thread;

	CameraDevice* dev;
	size_t streamIndex;
	ICamHalSampleCapturedCallback sampleCallback;

	BYTE* sample;
	size_t sampleSize;
} CamGstHal;

/** logs the errors the pipeline posted, returns TRUE if there was one */
static BOOL cam_gst_log_errors(CamGstHal* hal)
{
	WINPR_ASSERT(hal);

	BOOL found = FALSE;
	GstBus* bus = gst_element_get_bus(hal->pipeline);
	if (!bus)
		return FALSE;

	GstMessage* msg = nullptr;
	while ((msg = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR)) != nullptr)
	{
		GError* error = nullptr;
		gchar* debug = nullptr;
		gst_message_parse_error(msg, &error, &debug);
		WLog_ERR(TAG, "Pipeline error from %s: %s (%s)", GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)),
		         error ? error->message : "unknown", debug ? debug : "no details");
		g_clear_error(&error);
		g_free(debug);
		gst_message_unref(msg);
		found = TRUE;
	}
	gst_object_unref(bus);
	return found;
}

static void cam_gst_pipeline_destroy(CamGstHal* hal)
{
	WINPR_ASSERT(hal);

	if (hal->sink)
	{
		gst_object_unref(hal->sink);
		hal->sink = nullptr;
	}
	if (hal->pipeline)
	{
		(void)gst_element_set_state(hal->pipeline, GST_STATE_NULL);
		gst_object_unref(hal->pipeline);
		hal->pipeline = nullptr;
	}
}

/** builds the pipeline from its description. The links to dynamic pads, such as the ones of
 * decodebin, are only made once: a pipeline back to NULL can't be played again */
WINPR_ATTR_NODISCARD
static BOOL cam_gst_pipeline_create(CamGstHal* hal)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(!hal->pipeline);

	GError* error = nullptr;
	hal->pipeline =
	    gst_parse_launch_full(hal->description, nullptr, GST_PARSE_FLAG_FATAL_ERRORS, &error);
	if (!hal->pipeline || error)
	{
		WLog_ERR(TAG, "Invalid pipeline \"%s\": %s", hal->description,
		         error ? error->message : "unknown error");
		g_clear_error(&error);
		cam_gst_pipeline_destroy(hal);
		return FALSE;
	}

	GstElement* sink = GST_IS_BIN(hal->pipeline)
	                       ? gst_bin_get_by_name(GST_BIN(hal->pipeline), CAM_GST_SINK_NAME)
	                       : nullptr;
	if (!sink)
	{
		WLog_ERR(TAG, "The pipeline has no %s element", CAM_GST_SINK_NAME);
		cam_gst_pipeline_destroy(hal);
		return FALSE;
	}
	hal->sink = GST_APP_SINK(sink);
	return TRUE;
}

/** sets the pipeline state and waits for the change, FALSE if it failed or timed out */
WINPR_ATTR_NODISCARD
static BOOL cam_gst_set_state(CamGstHal* hal, GstState state, GstStateChangeReturn* pResult)
{
	WINPR_ASSERT(hal);

	GstStateChangeReturn rc = gst_element_set_state(hal->pipeline, state);
	if (rc == GST_STATE_CHANGE_ASYNC)
		rc = gst_element_get_state(hal->pipeline, nullptr, nullptr, CAM_GST_START_TIMEOUT);

	if (pResult)
		*pResult = rc;

	switch (rc)
	{
		case GST_STATE_CHANGE_SUCCESS:
		case GST_STATE_CHANGE_NO_PREROLL:
			return TRUE;
		case GST_STATE_CHANGE_ASYNC:
			WLog_ERR(TAG, "Pipeline did not reach %s in time", gst_element_state_get_name(state));
			return FALSE;
		case GST_STATE_CHANGE_FAILURE:
		default:
			if (!cam_gst_log_errors(hal))
				WLog_ERR(TAG, "Pipeline failed to go to %s", gst_element_state_get_name(state));
			return FALSE;
	}
}

/** copies an I420 sample into hal->sample, cropping it to the announced size */
WINPR_ATTR_NODISCARD
static BOOL cam_gst_convert_sample(CamGstHal* hal, GstSample* sample)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(sample);

	GstVideoInfo info;
	GstCaps* caps = gst_sample_get_caps(sample);
	GstBuffer* buffer = gst_sample_get_buffer(sample);
	if (!caps || !buffer || !gst_video_info_from_caps(&info, caps))
	{
		WLog_WARN(TAG, "Skipping a sample without video caps");
		return FALSE;
	}

	if ((GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_I420) ||
	    (GST_VIDEO_INFO_WIDTH(&info) < 0) || (GST_VIDEO_INFO_HEIGHT(&info) < 0) ||
	    ((UINT32)GST_VIDEO_INFO_WIDTH(&info) < hal->width) ||
	    ((UINT32)GST_VIDEO_INFO_HEIGHT(&info) < hal->height))
	{
		WLog_WARN(
		    TAG, "Skipping a %s %dx%d frame, the announced format is I420 %" PRIu32 "x%" PRIu32,
		    gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(&info)), GST_VIDEO_INFO_WIDTH(&info),
		    GST_VIDEO_INFO_HEIGHT(&info), hal->width, hal->height);
		return FALSE;
	}

	GstVideoFrame frame;
	if (!gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ))
	{
		WLog_WARN(TAG, "Unable to map a frame");
		return FALSE;
	}

	/* planes may be padded, and larger than announced when cropping an odd size */
	BYTE* dst = hal->sample;
	for (guint plane = 0; plane < 3; plane++)
	{
		const size_t rowSize = (plane == 0) ? hal->width : hal->width / 2;
		const size_t rows = (plane == 0) ? hal->height : hal->height / 2;
		const BYTE* src = GST_VIDEO_FRAME_PLANE_DATA(&frame, plane);
		const size_t stride = (size_t)GST_VIDEO_FRAME_PLANE_STRIDE(&frame, plane);

		for (size_t y = 0; y < rows; y++)
		{
			memcpy(dst, &src[y * stride], rowSize);
			dst += rowSize;
		}
	}

	gst_video_frame_unmap(&frame);
	return TRUE;
}

/** releases the source while nobody wants frames: a live one is closed, others keep their
 * position */
static void cam_gst_release(CamGstHal* hal)
{
	WINPR_ASSERT(hal);

	if (hal->live || !cam_gst_set_state(hal, GST_STATE_PAUSED, nullptr))
		cam_gst_pipeline_destroy(hal);
}

static DWORD WINAPI cam_gst_capture_thread(LPVOID param)
{
	CamGstHal* hal = (CamGstHal*)param;
	WINPR_ASSERT(hal);

	BOOL endLogged = FALSE;
	for (;;)
	{
		EnterCriticalSection(&hal->lock);
		const BOOL streaming = hal->streaming;
		LeaveCriticalSection(&hal->lock);
		if (!streaming)
			break;

		GstSample* sample = gst_app_sink_try_pull_sample(hal->sink, CAM_GST_PULL_TIMEOUT);
		if (!sample)
		{
			if (cam_gst_log_errors(hal))
				break;

			if (!gst_app_sink_is_eos(hal->sink))
				continue;

			if (hal->loop && !hal->live)
			{
				WLog_DBG(TAG, "End of stream, playing it again");
				if (!gst_element_seek_simple(hal->pipeline, GST_FORMAT_TIME,
				                             GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0))
				{
					WLog_ERR(TAG, "Unable to rewind the pipeline");
					break;
				}
			}
			else if (!endLogged)
			{
				/* the camera keeps its last frame until the stream is stopped */
				WLog_INFO(TAG, "End of stream");
				endLogged = TRUE;
			}
			continue;
		}

		const BOOL converted = cam_gst_convert_sample(hal, sample);
		gst_sample_unref(sample);
		if (!converted)
			continue;

		const UINT error =
		    hal->sampleCallback(hal->dev, hal->streamIndex, hal->sample, hal->sampleSize);
		if (error == ECAM_SAMPLE_CAPTURE_DRAINED)
		{
			WLog_DBG(TAG, "No more sample requests, releasing the pipeline");
			cam_gst_release(hal);
			break;
		}
		else if (error != CHANNEL_RC_OK)
			WLog_ERR(TAG, "Failure in sampleCallback: %" PRIu32, error);
	}

	EnterCriticalSection(&hal->lock);
	hal->streaming = FALSE;
	LeaveCriticalSection(&hal->lock);
	return CHANNEL_RC_OK;
}

static void cam_gst_join(CamGstHal* hal)
{
	WINPR_ASSERT(hal);

	EnterCriticalSection(&hal->lock);
	hal->streaming = FALSE;
	LeaveCriticalSection(&hal->lock);

	if (hal->thread)
	{
		(void)WaitForSingleObject(hal->thread, INFINITE);
		(void)CloseHandle(hal->thread);
		hal->thread = nullptr;
	}
}

static BOOL cam_gst_activate(WINPR_ATTR_UNUSED ICamHal* ihal,
                             WINPR_ATTR_UNUSED const char* deviceId, CAM_ERROR_CODE* errorCode)
{
	*errorCode = CAM_ERROR_CODE_None;
	return TRUE;
}

static BOOL cam_gst_deactivate(WINPR_ATTR_UNUSED ICamHal* ihal,
                               WINPR_ATTR_UNUSED const char* deviceId, CAM_ERROR_CODE* errorCode)
{
	*errorCode = CAM_ERROR_CODE_None;
	return TRUE;
}

static UINT cam_gst_enumerate(ICamHal* ihal, ICamHalEnumCallback callback, CameraPlugin* ecam,
                              GENERIC_CHANNEL_CALLBACK* hchannel)
{
	CamGstHal* hal = (CamGstHal*)ihal;
	WINPR_ASSERT(hal);

	IFCALL(callback, ecam, hchannel, CAM_GST_DEVICE_ID, hal->name);
	return 1;
}

/**
 * Function description
 *
 * @return -1 if error, otherwise index of supportedFormats array and mediaTypes/nMediaTypes filled
 * in
 */
static INT16 cam_gst_get_media_type_descriptions(
    ICamHal* ihal, WINPR_ATTR_UNUSED const char* deviceId, WINPR_ATTR_UNUSED size_t streamIndex,
    const CAM_MEDIA_FORMAT_INFO* supportedFormats, size_t nSupportedFormats,
    CAM_MEDIA_TYPE_DESCRIPTION* mediaTypes, size_t* nMediaTypes)
{
	CamGstHal* hal = (CamGstHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(nMediaTypes);

	const size_t maxMediaTypes = *nMediaTypes;
	*nMediaTypes = 0;
	if (maxMediaTypes == 0)
		return -1;

	/* the pipeline always ends with I420, take the preferred encoding of it */
	for (size_t formatIndex = 0; formatIndex < nSupportedFormats; formatIndex++)
	{
		if (supportedFormats[formatIndex].inputFormat != CAM_MEDIA_FORMAT_I420)
			continue;
		if (formatIndex > INT16_MAX)
			break;

		*mediaTypes = (CAM_MEDIA_TYPE_DESCRIPTION){
			.Format = CAM_MEDIA_FORMAT_I420,
			.Width = hal->width,
			.Height = hal->height,
			.FrameRateNumerator = hal->frameRateNumerator,
			.FrameRateDenominator = hal->frameRateDenominator,
			.PixelAspectRatioNumerator = 1,
			.PixelAspectRatioDenominator = 1,
		};
		*nMediaTypes = 1;
		return (INT16)formatIndex;
	}

	WLog_ERR(TAG, "I420 is not a supported input format");
	return -1;
}

static CAM_ERROR_CODE cam_gst_stream_start(ICamHal* ihal, CameraDevice* dev, size_t streamIndex,
                                           const CAM_MEDIA_TYPE_DESCRIPTION* mediaType,
                                           ICamHalSampleCapturedCallback callback)
{
	CamGstHal* hal = (CamGstHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(dev);
	WINPR_ASSERT(mediaType);

	if ((mediaType->Format != CAM_MEDIA_FORMAT_I420) || (mediaType->Width != hal->width) ||
	    (mediaType->Height != hal->height))
	{
		WLog_ERR(TAG, "Unexpected media type %u %" PRIu32 "x%" PRIu32, mediaType->Format,
		         mediaType->Width, mediaType->Height);
		return CAM_ERROR_CODE_InvalidMediaType;
	}

	EnterCriticalSection(&hal->lock);
	const BOOL streaming = hal->streaming;
	LeaveCriticalSection(&hal->lock);

	if (streaming)
	{
		WLog_ERR(TAG, "Streaming already in progress, device %s, streamIndex %" PRIuz,
		         dev->deviceId, streamIndex);
		return CAM_ERROR_CODE_UnexpectedError;
	}

	/* the thread of a capture released for lack of sample requests */
	cam_gst_join(hal);

	hal->dev = dev;
	hal->streamIndex = streamIndex;
	hal->sampleCallback = callback;

	if (!hal->pipeline && !cam_gst_pipeline_create(hal))
		return CAM_ERROR_CODE_UnexpectedError;

	if (!cam_gst_set_state(hal, GST_STATE_PLAYING, nullptr))
	{
		cam_gst_pipeline_destroy(hal);
		return CAM_ERROR_CODE_UnexpectedError;
	}

	EnterCriticalSection(&hal->lock);
	hal->streaming = TRUE;
	LeaveCriticalSection(&hal->lock);

	hal->thread = CreateThread(nullptr, 0, cam_gst_capture_thread, hal, 0, nullptr);
	if (!hal->thread)
	{
		WLog_ERR(TAG, "CreateThread failure");
		EnterCriticalSection(&hal->lock);
		hal->streaming = FALSE;
		LeaveCriticalSection(&hal->lock);
		cam_gst_pipeline_destroy(hal);
		return CAM_ERROR_CODE_OutOfMemory;
	}

	WLog_INFO(TAG, "Pipeline playing as camera: %" PRIu32 "x%" PRIu32 ", fps: %" PRIu32 "/%" PRIu32,
	          hal->width, hal->height, hal->frameRateNumerator, hal->frameRateDenominator);
	return CAM_ERROR_CODE_None;
}

static CAM_ERROR_CODE cam_gst_stream_stop(ICamHal* ihal, WINPR_ATTR_UNUSED const char* deviceId,
                                          WINPR_ATTR_UNUSED size_t streamIndex)
{
	CamGstHal* hal = (CamGstHal*)ihal;
	WINPR_ASSERT(hal);

	cam_gst_join(hal);

	/* closes the source, the next stream starts from the beginning */
	cam_gst_pipeline_destroy(hal);
	return CAM_ERROR_CODE_None;
}

static CAM_ERROR_CODE cam_gst_free(ICamHal* ihal)
{
	CamGstHal* hal = (CamGstHal*)ihal;

	if (!hal)
		return CAM_ERROR_CODE_NotInitialized;

	cam_gst_join(hal);
	cam_gst_pipeline_destroy(hal);
	DeleteCriticalSection(&hal->lock);

	free(hal->sample);
	free(hal->description);
	free(hal->name);
	free(hal);
	return CAM_ERROR_CODE_None;
}

/** builds the pipeline and reads the size and frame rate it negotiates, the media type the
 * camera announces */
WINPR_ATTR_NODISCARD
static BOOL cam_gst_probe(CamGstHal* hal)
{
	WINPR_ASSERT(hal);

	WLog_DBG(TAG, "Pipeline: %s", hal->description);

	if (!cam_gst_pipeline_create(hal))
		return FALSE;

	/* a live source does not preroll, it has to play to show its format */
	BOOL rc = FALSE;
	GstSample* sample = nullptr;
	GstStateChangeReturn state = GST_STATE_CHANGE_FAILURE;
	if (!cam_gst_set_state(hal, GST_STATE_PAUSED, &state))
		goto out;

	hal->live = (state == GST_STATE_CHANGE_NO_PREROLL);
	if (hal->live)
	{
		if (!cam_gst_set_state(hal, GST_STATE_PLAYING, nullptr))
			goto out;
		sample = gst_app_sink_try_pull_sample(hal->sink, CAM_GST_START_TIMEOUT);
	}
	else
		sample = gst_app_sink_try_pull_preroll(hal->sink, CAM_GST_START_TIMEOUT);

	if (!sample)
	{
		if (!cam_gst_log_errors(hal))
			WLog_ERR(TAG, "The pipeline did not produce any frame");
		goto out;
	}

	GstVideoInfo info;
	GstCaps* caps = gst_sample_get_caps(sample);
	if (!caps || !gst_video_info_from_caps(&info, caps))
	{
		WLog_ERR(TAG, "The pipeline does not produce video");
		goto out;
	}

	/* I420 needs even dimensions: crop the last column or row if needed */
	const int width = GST_VIDEO_INFO_WIDTH(&info);
	const int height = GST_VIDEO_INFO_HEIGHT(&info);
	if ((width < 2) || (height < 2))
	{
		WLog_ERR(TAG, "Invalid video size %dx%d", width, height);
		goto out;
	}
	hal->width = (UINT32)width & ~1u;
	hal->height = (UINT32)height & ~1u;

	/* 0/1 is a variable frame rate */
	if ((GST_VIDEO_INFO_FPS_N(&info) > 0) && (GST_VIDEO_INFO_FPS_D(&info) > 0))
	{
		hal->frameRateNumerator = (UINT32)GST_VIDEO_INFO_FPS_N(&info);
		hal->frameRateDenominator = (UINT32)GST_VIDEO_INFO_FPS_D(&info);
	}
	else
	{
		WLog_WARN(TAG, "Variable frame rate, announcing %d fps",
		          CAM_GST_FRAMERATE_NUMERATOR_DEFAULT);
		hal->frameRateNumerator = CAM_GST_FRAMERATE_NUMERATOR_DEFAULT;
		hal->frameRateDenominator = CAM_GST_FRAMERATE_DENOMINATOR_DEFAULT;
	}

	hal->sampleSize = 3ull * hal->width * hal->height / 2;
	hal->sample = calloc(1, hal->sampleSize);
	if (!hal->sample)
		goto out;

	WLog_INFO(TAG,
	          "%s pipeline: %dx%d, announced as %" PRIu32 "x%" PRIu32 " at %" PRIu32 "/%" PRIu32
	          " fps",
	          hal->live ? "Live" : "Non live", width, height, hal->width, hal->height,
	          hal->frameRateNumerator, hal->frameRateDenominator);
	rc = TRUE;

out:
	if (sample)
		gst_sample_unref(sample);

	/* nothing runs until the server starts the camera */
	cam_gst_pipeline_destroy(hal);
	return rc;
}

static const char* cam_gst_basename(const char* path)
{
	const char* name = path;
	for (const char* cur = path; *cur; cur++)
	{
		if ((*cur == '/') || (*cur == '\\'))
			name = cur + 1;
	}
	return name;
}

WINPR_ATTR_MALLOC(free, 1)
static char* cam_gst_read_file(const char* path)
{
	WINPR_ASSERT(path);

	char* content = nullptr;
	FILE* fp = winpr_fopen(path, "rb");
	if (!fp)
	{
		WLog_ERR(TAG, "Unable to open pipeline file %s", path);
		return nullptr;
	}

	if (_fseeki64(fp, 0, SEEK_END) != 0)
		goto out;
	const INT64 size = _ftelli64(fp);
	if ((size <= 0) || (size > 1024 * 1024) || (_fseeki64(fp, 0, SEEK_SET) != 0))
		goto out;

	content = calloc(1, (size_t)size + 1);
	if (!content)
		goto out;

	if (fread(content, 1, (size_t)size, fp) != (size_t)size)
	{
		free(content);
		content = nullptr;
	}

out:
	if (!content)
		WLog_ERR(TAG, "Unable to read pipeline file %s", path);
	(void)fclose(fp);
	return content;
}

WINPR_ATTR_NODISCARD
static BOOL cam_gst_parse_bool(const char* value, BOOL* result)
{
	WINPR_ASSERT(value);
	WINPR_ASSERT(result);

	if ((_stricmp(value, "on") == 0) || (_stricmp(value, "true") == 0) || (strcmp(value, "1") == 0))
		*result = TRUE;
	else if ((_stricmp(value, "off") == 0) || (_stricmp(value, "false") == 0) ||
	         (strcmp(value, "0") == 0))
		*result = FALSE;
	else
		return FALSE;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL cam_gst_parse_args(CamGstHal* hal, const ADDIN_ARGV* args)
{
	WINPR_ASSERT(hal);

	COMMAND_LINE_ARGUMENT_A gst_args[] = {
		{ "pipeline", COMMAND_LINE_VALUE_REQUIRED, "<pipeline>", nullptr, nullptr, -1, nullptr,
		  "pipeline producing video, in gst-launch syntax" },
		{ "pipeline-file", COMMAND_LINE_VALUE_REQUIRED, "<path>", nullptr, nullptr, -1, nullptr,
		  "file containing the pipeline" },
		{ "file", COMMAND_LINE_VALUE_REQUIRED, "<path>", nullptr, nullptr, -1, nullptr,
		  "media file to play" },
		{ "name", COMMAND_LINE_VALUE_REQUIRED, "<name>", nullptr, nullptr, -1, nullptr,
		  "camera name" },
		{ "loop", COMMAND_LINE_VALUE_REQUIRED, "<on|off>", nullptr, nullptr, -1, nullptr,
		  "play the pipeline again when it ends" },
		{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
	};

	char* source = nullptr;
	char* defaultName = nullptr;
	size_t sources = 0;
	size_t len = 0;
	BOOL rc = FALSE;

	if (args && (args->argc > 1))
	{
		const DWORD flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON |
		                    COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
		const int status = CommandLineParseArgumentsA(args->argc, args->argv, gst_args, flags, hal,
		                                              nullptr, nullptr);
		if (status != 0)
			return FALSE;

		const COMMAND_LINE_ARGUMENT_A* arg = gst_args;
		do
		{
			if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
				continue;

			CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "pipeline")
			{
				sources++;
				free(source);
				source = _strdup(arg->Value);
				if (!source)
					goto out;
			}
			CommandLineSwitchCase(arg, "pipeline-file")
			{
				sources++;
				free(source);
				source = cam_gst_read_file(arg->Value);
				if (!source)
					goto out;
			}
			CommandLineSwitchCase(arg, "file")
			{
				sources++;
				free(source);
				source = nullptr;

				GError* error = nullptr;
				gchar* uri = gst_filename_to_uri(arg->Value, &error);
				if (!uri)
				{
					WLog_ERR(TAG, "Invalid file %s: %s", arg->Value,
					         error ? error->message : "unknown error");
					g_clear_error(&error);
					goto out;
				}
				/* only decode the video streams, the other ones are exposed undecoded and stay
				 * unlinked. Don't add expose-all-streams=false: combined with these caps,
				 * decodebin stops finding decoders at all ("Missing decoder: H.264") */
				winpr_asprintf(&source, &len, "uridecodebin uri=\"%s\" caps=video/x-raw", uri);
				g_free(uri);
				if (!source)
					goto out;

				free(defaultName);
				defaultName = nullptr;
				winpr_asprintf(&defaultName, &len, "Movie: %s", cam_gst_basename(arg->Value));
				if (!defaultName)
					goto out;
			}
			CommandLineSwitchCase(arg, "name")
			{
				free(hal->name);
				hal->name = _strdup(arg->Value);
				if (!hal->name)
					goto out;
			}
			CommandLineSwitchCase(arg, "loop")
			{
				if (!cam_gst_parse_bool(arg->Value, &hal->loop))
				{
					WLog_ERR(TAG, "Invalid loop value %s, expecting on or off", arg->Value);
					goto out;
				}
			}
			CommandLineSwitchDefault(arg)
			{
			}
			CommandLineSwitchEnd(arg)
		} while ((arg = CommandLineFindNextArgumentA(arg)) != nullptr);
	}

	if (sources != 1)
	{
		WLog_ERR(TAG, "The gst backend needs one of pipeline:<pipeline>, pipeline-file:<path> or "
		              "file:<path>");
		goto out;
	}

	winpr_asprintf(&hal->description, &len,
	               "%s ! videoconvert ! video/x-raw,format=I420 ! appsink name=" CAM_GST_SINK_NAME
	               " sync=true max-buffers=1 drop=true",
	               source);
	if (!hal->description)
		goto out;

	if (!hal->name)
	{
		hal->name = defaultName ? defaultName : _strdup("GStreamer camera");
		defaultName = nullptr;
		if (!hal->name)
			goto out;
	}
	rc = TRUE;

out:
	free(source);
	free(defaultName);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT VCAPITYPE gst_freerdp_rdpecam_client_subsystem_entry(
    PFREERDP_CAMERA_HAL_ENTRY_POINTS pEntryPoints))
{
	UINT ret = ERROR_INVALID_PARAMETER;
	WINPR_ASSERT(pEntryPoints);

	GError* error = nullptr;
	if (!gst_init_check(nullptr, nullptr, &error))
	{
		WLog_ERR(TAG, "Unable to initialize GStreamer: %s",
		         error ? error->message : "unknown error");
		g_clear_error(&error);
		return ERROR_INTERNAL_ERROR;
	}

	CamGstHal* hal = (CamGstHal*)calloc(1, sizeof(CamGstHal));
	if (!hal)
		return CHANNEL_RC_NO_MEMORY;

	hal->iHal.Enumerate = cam_gst_enumerate;
	hal->iHal.RequestDrivenCapture = TRUE;
	hal->iHal.GetMediaTypeDescriptions = cam_gst_get_media_type_descriptions;
	hal->iHal.Activate = cam_gst_activate;
	hal->iHal.Deactivate = cam_gst_deactivate;
	hal->iHal.StartStream = cam_gst_stream_start;
	hal->iHal.StopStream = cam_gst_stream_stop;
	hal->iHal.Free = cam_gst_free;
	hal->loop = TRUE;

	if (!InitializeCriticalSectionEx(&hal->lock, 0, 0))
	{
		free(hal);
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!cam_gst_parse_args(hal, pEntryPoints->args))
		goto error;

	/* fail now rather than when the server opens the camera */
	if (!cam_gst_probe(hal))
		goto error;

	ret = pEntryPoints->pRegisterCameraHal(pEntryPoints->plugin, &hal->iHal);
	if (ret != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "RegisterCameraHal failed with error %" PRIu32 "", ret);
		goto error;
	}

	return ret;

error:
	cam_gst_free(&hal->iHal);
	return ret;
}
