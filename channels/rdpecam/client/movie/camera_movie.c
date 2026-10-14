/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, movie file backend
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

/* A pseudo camera playing a movie file in a loop, selected with
 * /dvc:rdpecam,sys:movie,file:<path>[,name:<camera name>]
 *
 * The only media type announced is the movie's own size (cropped to even dimensions) and frame
 * rate. Frames are decoded with FFmpeg and handed to the channel as I420, the channel encoder
 * does the rest as for a raw webcam. */

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/cmdline.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/thread.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "camera.h"

#define TAG CHANNELS_TAG("rdpecam-movie.client")

/* the channel name of the device, what a Windows client would use for its first camera */
#define CAM_MOVIE_DEVICE_ID "RDCamera_Device_0"

#define CAM_MOVIE_FRAMERATE_NUMERATOR_DEFAULT 30
#define CAM_MOVIE_FRAMERATE_DENOMINATOR_DEFAULT 1

/* longest sleep of the playback thread, so that a stop request is seen quickly */
#define CAM_MOVIE_MAX_SLEEP_MS 50

/* when decoding fell behind by more than this, restart the playback clock instead of catching up */
#define CAM_MOVIE_MAX_LATE_NS (1000ull * 1000ull * 1000ull)

typedef struct
{
	AVFormatContext* format;
	AVCodecContext* codec;
	int videoIndex;
	AVPacket* packet;
	AVFrame* frame;
	struct SwsContext* sws;
	BOOL draining;
	size_t framesSinceRewind;
} CamMovieDecoder;

typedef struct
{
	CRITICAL_SECTION lock;
	BOOL streaming;
	HANDLE thread;

	CameraDevice* dev;
	size_t streamIndex;
	ICamHalSampleCapturedCallback sampleCallback;

	/* kept open while the capture is released for lack of sample requests, so that playback
	 * resumes where it was */
	CamMovieDecoder decoder;
	BYTE* sample;
	size_t sampleSize;
} CamMovieStream;

typedef struct
{
	ICamHal iHal;

	char* file;
	char* name;

	/* characteristics of the movie, the only media type announced */
	UINT32 width;
	UINT32 height;
	UINT32 frameRateNumerator;
	UINT32 frameRateDenominator;

	CamMovieStream stream;
} CamMovieHal;

static void cam_movie_decoder_close(CamMovieDecoder* decoder)
{
	WINPR_ASSERT(decoder);

	sws_freeContext(decoder->sws);
	av_frame_free(&decoder->frame);
	av_packet_free(&decoder->packet);
	avcodec_free_context(&decoder->codec);
	avformat_close_input(&decoder->format);

	*decoder = (CamMovieDecoder){ .videoIndex = -1 };
}

static const char* cam_movie_av_error(int error, char* buffer, size_t size)
{
	if (av_strerror(error, buffer, size) < 0)
		(void)_snprintf(buffer, size, "error %d", error);
	return buffer;
}

WINPR_ATTR_NODISCARD
static BOOL cam_movie_decoder_open(CamMovieDecoder* decoder, const char* file)
{
	WINPR_ASSERT(decoder);
	WINPR_ASSERT(file);

	char buffer[128] = WINPR_C_ARRAY_INIT;
	*decoder = (CamMovieDecoder){ .videoIndex = -1 };

	int rc = avformat_open_input(&decoder->format, file, nullptr, nullptr);
	if (rc < 0)
	{
		WLog_ERR(TAG, "Unable to open movie %s: %s", file,
		         cam_movie_av_error(rc, buffer, sizeof(buffer)));
		goto fail;
	}

	rc = avformat_find_stream_info(decoder->format, nullptr);
	if (rc < 0)
	{
		WLog_ERR(TAG, "Unable to read stream information of %s: %s", file,
		         cam_movie_av_error(rc, buffer, sizeof(buffer)));
		goto fail;
	}

#if LIBAVFORMAT_VERSION_MAJOR >= 59
	const AVCodec* codec = nullptr;
#else
	AVCodec* codec = nullptr;
#endif
	rc = av_find_best_stream(decoder->format, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
	if (rc < 0)
	{
		WLog_ERR(TAG, "No decodable video stream in %s: %s", file,
		         cam_movie_av_error(rc, buffer, sizeof(buffer)));
		goto fail;
	}
	decoder->videoIndex = rc;

	decoder->codec = avcodec_alloc_context3(codec);
	if (!decoder->codec)
		goto fail;

	const AVStream* stream = decoder->format->streams[decoder->videoIndex];
	rc = avcodec_parameters_to_context(decoder->codec, stream->codecpar);
	if (rc < 0)
		goto fail;

	rc = avcodec_open2(decoder->codec, codec, nullptr);
	if (rc < 0)
	{
		WLog_ERR(TAG, "Unable to open the %s decoder: %s", codec->name,
		         cam_movie_av_error(rc, buffer, sizeof(buffer)));
		goto fail;
	}

	decoder->packet = av_packet_alloc();
	decoder->frame = av_frame_alloc();
	if (!decoder->packet || !decoder->frame)
		goto fail;

	return TRUE;

fail:
	cam_movie_decoder_close(decoder);
	return FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL cam_movie_decoder_rewind(CamMovieDecoder* decoder)
{
	WINPR_ASSERT(decoder);

	if (decoder->framesSinceRewind == 0)
	{
		WLog_ERR(TAG, "The movie did not produce any frame");
		return FALSE;
	}

	const AVStream* stream = decoder->format->streams[decoder->videoIndex];
	const int64_t start = (stream->start_time != AV_NOPTS_VALUE) ? stream->start_time : 0;
	const int rc = av_seek_frame(decoder->format, decoder->videoIndex, start, AVSEEK_FLAG_BACKWARD);
	if (rc < 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "Unable to rewind the movie: %s",
		         cam_movie_av_error(rc, buffer, sizeof(buffer)));
		return FALSE;
	}

	avcodec_flush_buffers(decoder->codec);
	decoder->draining = FALSE;
	decoder->framesSinceRewind = 0;
	WLog_DBG(TAG, "End of movie, playing it again");
	return TRUE;
}

/** decodes the next frame into decoder->frame, starting the movie again at its end */
WINPR_ATTR_NODISCARD
static BOOL cam_movie_decoder_next_frame(CamMovieDecoder* decoder)
{
	WINPR_ASSERT(decoder);

	char buffer[128] = WINPR_C_ARRAY_INIT;
	for (;;)
	{
		int rc = avcodec_receive_frame(decoder->codec, decoder->frame);
		if (rc == 0)
		{
			decoder->framesSinceRewind++;
			return TRUE;
		}

		/* a draining decoder has nothing more to give even if it does not say AVERROR_EOF */
		if ((rc == AVERROR_EOF) || ((rc == AVERROR(EAGAIN)) && decoder->draining))
		{
			if (!cam_movie_decoder_rewind(decoder))
				return FALSE;
			continue;
		}

		if (rc != AVERROR(EAGAIN))
		{
			WLog_ERR(TAG, "Unable to decode the movie: %s",
			         cam_movie_av_error(rc, buffer, sizeof(buffer)));
			return FALSE;
		}

		rc = av_read_frame(decoder->format, decoder->packet);
		if (rc == AVERROR_EOF)
		{
			/* flush the frames still in the decoder, it then reports AVERROR_EOF */
			decoder->draining = TRUE;
			rc = avcodec_send_packet(decoder->codec, nullptr);
		}
		else if (rc >= 0)
		{
			if (decoder->packet->stream_index == decoder->videoIndex)
				rc = avcodec_send_packet(decoder->codec, decoder->packet);
			av_packet_unref(decoder->packet);
		}

		if ((rc < 0) && (rc != AVERROR(EAGAIN)))
		{
			WLog_ERR(TAG, "Unable to read the movie: %s",
			         cam_movie_av_error(rc, buffer, sizeof(buffer)));
			return FALSE;
		}
	}
}

/** converts decoder->frame into a packed I420 sample of width x height (the movie, cropped) */
WINPR_ATTR_NODISCARD
static BOOL cam_movie_convert_frame(CamMovieStream* stream, UINT32 width, UINT32 height)
{
	WINPR_ASSERT(stream);

	CamMovieDecoder* decoder = &stream->decoder;
	const AVFrame* frame = decoder->frame;

	if ((frame->width < 0) || (frame->height < 0) || ((UINT32)frame->width < width) ||
	    ((UINT32)frame->height < height))
	{
		WLog_WARN(TAG, "Skipping a %dx%d frame, smaller than the announced %" PRIu32 "x%" PRIu32,
		          frame->width, frame->height, width, height);
		return FALSE;
	}

	const int w = WINPR_ASSERTING_INT_CAST(int, width);
	const int h = WINPR_ASSERTING_INT_CAST(int, height);

	/* a source of the announced size crops the right and bottom of a larger frame */
	decoder->sws =
	    sws_getCachedContext(decoder->sws, w, h, (enum AVPixelFormat)frame->format, w, h,
	                         AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
	if (!decoder->sws)
	{
		WLog_ERR(TAG, "Unable to convert %s frames to I420",
		         av_get_pix_fmt_name((enum AVPixelFormat)frame->format));
		return FALSE;
	}

	const size_t lumaSize = 1ull * width * height;
	BYTE* dst[4] = { stream->sample, stream->sample + lumaSize,
		             stream->sample + lumaSize + lumaSize / 4, nullptr };
	const int dstStride[4] = { w, w / 2, w / 2, 0 };

	const int rc = sws_scale(decoder->sws, (const uint8_t* const*)frame->data, frame->linesize, 0,
	                         h, dst, dstStride);
	return rc == h;
}

/** sleeps until the playback clock reaches due, giving up early when the stream is stopped */
WINPR_ATTR_NODISCARD
static BOOL cam_movie_wait_until(CamMovieStream* stream, UINT64 due)
{
	WINPR_ASSERT(stream);

	for (;;)
	{
		EnterCriticalSection(&stream->lock);
		const BOOL streaming = stream->streaming;
		LeaveCriticalSection(&stream->lock);
		if (!streaming)
			return FALSE;

		const UINT64 now = winpr_GetTickCount64NS();
		if (now >= due)
			return TRUE;

		UINT64 ms = (due - now) / 1000000ull;
		if (ms > CAM_MOVIE_MAX_SLEEP_MS)
			ms = CAM_MOVIE_MAX_SLEEP_MS;
		Sleep((DWORD)ms);
	}
}

static DWORD WINAPI cam_movie_playback_thread(LPVOID param)
{
	CamMovieHal* hal = (CamMovieHal*)param;
	WINPR_ASSERT(hal);

	CamMovieStream* stream = &hal->stream;
	const UINT64 frameDuration =
	    (1000000000ull * hal->frameRateDenominator) / hal->frameRateNumerator;
	UINT64 start = winpr_GetTickCount64NS();
	UINT64 frames = 0;

	for (;;)
	{
		if (!cam_movie_decoder_next_frame(&stream->decoder))
			break;

		if (!cam_movie_convert_frame(stream, hal->width, hal->height))
			continue;

		/* the movie is played at the frame rate announced to the server */
		UINT64 due = start + frames * frameDuration;
		const UINT64 now = winpr_GetTickCount64NS();
		if (now > due + CAM_MOVIE_MAX_LATE_NS)
		{
			WLog_DBG(TAG, "Playback late by %" PRIu64 " ms, resetting the clock",
			         (UINT64)((now - due) / 1000000ull));
			start = due = now;
			frames = 0;
		}
		frames++;

		if (!cam_movie_wait_until(stream, due))
			break;

		const UINT error = stream->sampleCallback(stream->dev, stream->streamIndex, stream->sample,
		                                          stream->sampleSize);
		if (error == ECAM_SAMPLE_CAPTURE_DRAINED)
		{
			WLog_DBG(TAG, "No more sample requests, pausing the movie");
			break;
		}
		else if (error != CHANNEL_RC_OK)
			WLog_ERR(TAG, "Failure in sampleCallback: %" PRIu32, error);
	}

	EnterCriticalSection(&stream->lock);
	stream->streaming = FALSE;
	LeaveCriticalSection(&stream->lock);
	return CHANNEL_RC_OK;
}

static void cam_movie_stream_join(CamMovieStream* stream)
{
	WINPR_ASSERT(stream);

	EnterCriticalSection(&stream->lock);
	stream->streaming = FALSE;
	LeaveCriticalSection(&stream->lock);

	if (stream->thread)
	{
		(void)WaitForSingleObject(stream->thread, INFINITE);
		(void)CloseHandle(stream->thread);
		stream->thread = nullptr;
	}
}

static BOOL cam_movie_activate(WINPR_ATTR_UNUSED ICamHal* ihal,
                               WINPR_ATTR_UNUSED const char* deviceId, CAM_ERROR_CODE* errorCode)
{
	*errorCode = CAM_ERROR_CODE_None;
	return TRUE;
}

static BOOL cam_movie_deactivate(WINPR_ATTR_UNUSED ICamHal* ihal,
                                 WINPR_ATTR_UNUSED const char* deviceId, CAM_ERROR_CODE* errorCode)
{
	*errorCode = CAM_ERROR_CODE_None;
	return TRUE;
}

static UINT cam_movie_enumerate(ICamHal* ihal, ICamHalEnumCallback callback, CameraPlugin* ecam,
                                GENERIC_CHANNEL_CALLBACK* hchannel)
{
	CamMovieHal* hal = (CamMovieHal*)ihal;
	WINPR_ASSERT(hal);

	IFCALL(callback, ecam, hchannel, CAM_MOVIE_DEVICE_ID, hal->name);
	return 1;
}

/**
 * Function description
 *
 * @return -1 if error, otherwise index of supportedFormats array and mediaTypes/nMediaTypes filled
 * in
 */
static INT16 cam_movie_get_media_type_descriptions(
    ICamHal* ihal, WINPR_ATTR_UNUSED const char* deviceId, WINPR_ATTR_UNUSED size_t streamIndex,
    const CAM_MEDIA_FORMAT_INFO* supportedFormats, size_t nSupportedFormats,
    CAM_MEDIA_TYPE_DESCRIPTION* mediaTypes, size_t* nMediaTypes)
{
	CamMovieHal* hal = (CamMovieHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(nMediaTypes);

	const size_t maxMediaTypes = *nMediaTypes;
	*nMediaTypes = 0;
	if (maxMediaTypes == 0)
		return -1;

	/* frames are always produced as I420, take the preferred encoding of it */
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

static CAM_ERROR_CODE cam_movie_stream_start(ICamHal* ihal, CameraDevice* dev, size_t streamIndex,
                                             const CAM_MEDIA_TYPE_DESCRIPTION* mediaType,
                                             ICamHalSampleCapturedCallback callback)
{
	CamMovieHal* hal = (CamMovieHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(dev);
	WINPR_ASSERT(mediaType);

	CamMovieStream* stream = &hal->stream;

	if ((mediaType->Format != CAM_MEDIA_FORMAT_I420) || (mediaType->Width != hal->width) ||
	    (mediaType->Height != hal->height))
	{
		WLog_ERR(TAG, "Unexpected media type %u %" PRIu32 "x%" PRIu32, mediaType->Format,
		         mediaType->Width, mediaType->Height);
		return CAM_ERROR_CODE_InvalidMediaType;
	}

	EnterCriticalSection(&stream->lock);
	const BOOL streaming = stream->streaming;
	LeaveCriticalSection(&stream->lock);

	if (streaming)
	{
		WLog_ERR(TAG, "Streaming already in progress, device %s, streamIndex %" PRIuz,
		         dev->deviceId, streamIndex);
		return CAM_ERROR_CODE_UnexpectedError;
	}

	/* the thread of a capture released for lack of sample requests */
	cam_movie_stream_join(stream);

	if (!stream->decoder.format && !cam_movie_decoder_open(&stream->decoder, hal->file))
		return CAM_ERROR_CODE_UnexpectedError;

	stream->dev = dev;
	stream->streamIndex = streamIndex;
	stream->sampleCallback = callback;

	EnterCriticalSection(&stream->lock);
	stream->streaming = TRUE;
	LeaveCriticalSection(&stream->lock);

	stream->thread = CreateThread(nullptr, 0, cam_movie_playback_thread, hal, 0, nullptr);
	if (!stream->thread)
	{
		WLog_ERR(TAG, "CreateThread failure");
		EnterCriticalSection(&stream->lock);
		stream->streaming = FALSE;
		LeaveCriticalSection(&stream->lock);
		return CAM_ERROR_CODE_OutOfMemory;
	}

	WLog_INFO(TAG, "Playing %s as camera: %" PRIu32 "x%" PRIu32 ", fps: %" PRIu32 "/%" PRIu32,
	          hal->file, hal->width, hal->height, hal->frameRateNumerator,
	          hal->frameRateDenominator);
	return CAM_ERROR_CODE_None;
}

static CAM_ERROR_CODE cam_movie_stream_stop(ICamHal* ihal, WINPR_ATTR_UNUSED const char* deviceId,
                                            WINPR_ATTR_UNUSED size_t streamIndex)
{
	CamMovieHal* hal = (CamMovieHal*)ihal;
	WINPR_ASSERT(hal);

	cam_movie_stream_join(&hal->stream);

	/* the next stream starts the movie from the beginning */
	cam_movie_decoder_close(&hal->stream.decoder);
	return CAM_ERROR_CODE_None;
}

static CAM_ERROR_CODE cam_movie_free(ICamHal* ihal)
{
	CamMovieHal* hal = (CamMovieHal*)ihal;

	if (!hal)
		return CAM_ERROR_CODE_NotInitialized;

	cam_movie_stream_join(&hal->stream);
	cam_movie_decoder_close(&hal->stream.decoder);
	DeleteCriticalSection(&hal->stream.lock);

	free(hal->stream.sample);
	free(hal->file);
	free(hal->name);
	free(hal);
	return CAM_ERROR_CODE_None;
}

/** reads the size and frame rate of the movie, the media type the camera announces */
WINPR_ATTR_NODISCARD
static BOOL cam_movie_probe(CamMovieHal* hal)
{
	WINPR_ASSERT(hal);

	CamMovieDecoder decoder = WINPR_C_ARRAY_INIT;
	if (!cam_movie_decoder_open(&decoder, hal->file))
		return FALSE;

	BOOL rc = FALSE;
	AVStream* stream = decoder.format->streams[decoder.videoIndex];
	const int width = decoder.codec->width;
	const int height = decoder.codec->height;

	/* I420 needs even dimensions: crop the last column or row if needed */
	if ((width < 2) || (height < 2))
	{
		WLog_ERR(TAG, "Invalid movie size %dx%d", width, height);
		goto out;
	}
	hal->width = (UINT32)width & ~1u;
	hal->height = (UINT32)height & ~1u;

	const AVRational rate = av_guess_frame_rate(decoder.format, stream, nullptr);
	if ((rate.num > 0) && (rate.den > 0))
	{
		hal->frameRateNumerator = (UINT32)rate.num;
		hal->frameRateDenominator = (UINT32)rate.den;
	}
	else
	{
		WLog_WARN(TAG, "Unknown frame rate, using %d fps", CAM_MOVIE_FRAMERATE_NUMERATOR_DEFAULT);
		hal->frameRateNumerator = CAM_MOVIE_FRAMERATE_NUMERATOR_DEFAULT;
		hal->frameRateDenominator = CAM_MOVIE_FRAMERATE_DENOMINATOR_DEFAULT;
	}

	hal->stream.sampleSize = 3ull * hal->width * hal->height / 2;
	hal->stream.sample = calloc(1, hal->stream.sampleSize);
	if (!hal->stream.sample)
		goto out;

	WLog_INFO(TAG,
	          "Movie %s: %dx%d %s, announced as %" PRIu32 "x%" PRIu32 " at %" PRIu32 "/%" PRIu32
	          " fps",
	          hal->file, width, height, avcodec_get_name(decoder.codec->codec_id), hal->width,
	          hal->height, hal->frameRateNumerator, hal->frameRateDenominator);
	rc = TRUE;

out:
	cam_movie_decoder_close(&decoder);
	return rc;
}

static const char* cam_movie_basename(const char* path)
{
	const char* name = path;
	for (const char* cur = path; *cur; cur++)
	{
		if ((*cur == '/') || (*cur == '\\'))
			name = cur + 1;
	}
	return name;
}

WINPR_ATTR_NODISCARD
static BOOL cam_movie_parse_args(CamMovieHal* hal, const ADDIN_ARGV* args)
{
	WINPR_ASSERT(hal);

	COMMAND_LINE_ARGUMENT_A movie_args[] = { { "file", COMMAND_LINE_VALUE_REQUIRED, "<path>",
		                                       nullptr, nullptr, -1, nullptr, "movie to play" },
		                                     { "name", COMMAND_LINE_VALUE_REQUIRED, "<name>",
		                                       nullptr, nullptr, -1, nullptr, "camera name" },
		                                     { nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr,
		                                       nullptr } };

	if (args && (args->argc > 1))
	{
		const DWORD flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON |
		                    COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
		const int status = CommandLineParseArgumentsA(args->argc, args->argv, movie_args, flags,
		                                              hal, nullptr, nullptr);
		if (status != 0)
			return FALSE;

		const COMMAND_LINE_ARGUMENT_A* arg = movie_args;
		do
		{
			if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
				continue;

			CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "file")
			{
				free(hal->file);
				hal->file = _strdup(arg->Value);
				if (!hal->file)
					return FALSE;
			}
			CommandLineSwitchCase(arg, "name")
			{
				free(hal->name);
				hal->name = _strdup(arg->Value);
				if (!hal->name)
					return FALSE;
			}
			CommandLineSwitchDefault(arg)
			{
			}
			CommandLineSwitchEnd(arg)
		} while ((arg = CommandLineFindNextArgumentA(arg)) != nullptr);
	}

	if (!hal->file)
	{
		WLog_ERR(TAG, "The movie backend needs a file:<path> argument");
		return FALSE;
	}

	if (!hal->name)
	{
		size_t len = 0;
		winpr_asprintf(&hal->name, &len, "Movie: %s", cam_movie_basename(hal->file));
		if (!hal->name)
			return FALSE;
	}
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT VCAPITYPE movie_freerdp_rdpecam_client_subsystem_entry(
    PFREERDP_CAMERA_HAL_ENTRY_POINTS pEntryPoints))
{
	UINT ret = ERROR_INVALID_PARAMETER;
	WINPR_ASSERT(pEntryPoints);

	CamMovieHal* hal = (CamMovieHal*)calloc(1, sizeof(CamMovieHal));
	if (!hal)
		return CHANNEL_RC_NO_MEMORY;

	hal->iHal.Enumerate = cam_movie_enumerate;
	hal->iHal.RequestDrivenCapture = TRUE;
	hal->iHal.GetMediaTypeDescriptions = cam_movie_get_media_type_descriptions;
	hal->iHal.Activate = cam_movie_activate;
	hal->iHal.Deactivate = cam_movie_deactivate;
	hal->iHal.StartStream = cam_movie_stream_start;
	hal->iHal.StopStream = cam_movie_stream_stop;
	hal->iHal.Free = cam_movie_free;
	hal->stream.decoder.videoIndex = -1;

	if (!InitializeCriticalSectionEx(&hal->stream.lock, 0, 0))
	{
		free(hal);
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!cam_movie_parse_args(hal, pEntryPoints->args))
		goto error;

	/* fail now rather than when the server opens the camera */
	if (!cam_movie_probe(hal))
		goto error;

	ret = pEntryPoints->pRegisterCameraHal(pEntryPoints->plugin, &hal->iHal);
	if (ret != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "RegisterCameraHal failed with error %" PRIu32 "", ret);
		goto error;
	}

	return ret;

error:
	cam_movie_free(&hal->iHal);
	return ret;
}
