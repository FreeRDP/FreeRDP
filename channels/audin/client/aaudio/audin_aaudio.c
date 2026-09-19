/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - AAudio implementation
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/freerdp.h>
#include <freerdp/addin.h>
#include <freerdp/channels/rdpsnd.h>
#include <freerdp/client/audin.h>

#include <aaudio/AAudio.h>

#include "audin_main.h"

/* Read blocks at most this long so the capture thread can observe a stop request. */
#define AAUDIO_READ_TIMEOUT_NS (200LL * 1000LL * 1000LL)

#define AAUDIO_MAX_RATE 48000

typedef struct
{
	IAudinDevice iface;

	AAudioStream* stream;
	HANDLE thread;
	HANDLE stopEvent;

	/* Guards the stream pointer: the capture thread drops it on a route change while
	 * Close/Free may be stopping it from the channel thread. */
	CRITICAL_SECTION lock;

	AUDIO_FORMAT format;
	UINT32 frames_per_packet;

	AudinReceive receive;
	void* user_data;

	rdpContext* rdpcontext;
	wLog* log;
} AudinAAudioDevice;

WINPR_ATTR_NODISCARD
static UINT audin_aaudio_close(IAudinDevice* device);

WINPR_ATTR_NODISCARD
static BOOL audin_aaudio_pcm_valid(const AUDIO_FORMAT* format)
{
	WINPR_ASSERT(format);

	return (format->wFormatTag == WAVE_FORMAT_PCM) && (format->cbSize == 0) &&
	       (format->wBitsPerSample == 16) && (format->nSamplesPerSec > 0) &&
	       (format->nSamplesPerSec <= AAUDIO_MAX_RATE) &&
	       ((format->nChannels == 1) || (format->nChannels == 2));
}

static void audin_aaudio_stop_stream(AudinAAudioDevice* aaudio, AAudioStream* stream)
{
	WINPR_ASSERT(aaudio);
	WINPR_ASSERT(stream);

	const aaudio_result_t rc = AAudioStream_requestStop(stream);
	if (rc != AAUDIO_OK)
		WLog_Print(aaudio->log, WLOG_WARN, "AAudioStream_requestStop: %s",
		           AAudio_convertResultToText(rc));
}

static void audin_aaudio_close_stream(AudinAAudioDevice* aaudio, AAudioStream* stream)
{
	WINPR_ASSERT(aaudio);
	WINPR_ASSERT(stream);

	const aaudio_result_t rc = AAudioStream_close(stream);
	if (rc != AAUDIO_OK)
		WLog_Print(aaudio->log, WLOG_WARN, "AAudioStream_close: %s",
		           AAudio_convertResultToText(rc));
}

WINPR_ATTR_NODISCARD
static BOOL audin_aaudio_open_stream(AudinAAudioDevice* aaudio)
{
	WINPR_ASSERT(aaudio);

	AAudioStreamBuilder* builder = nullptr;

	aaudio_result_t rc = AAudio_createStreamBuilder(&builder);
	if (rc != AAUDIO_OK)
	{
		WLog_Print(aaudio->log, WLOG_ERROR, "AAudio_createStreamBuilder: %s",
		           AAudio_convertResultToText(rc));
		return FALSE;
	}

	AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setSampleRate(
	    builder, WINPR_ASSERTING_INT_CAST(int32_t, aaudio->format.nSamplesPerSec));
	AAudioStreamBuilder_setChannelCount(
	    builder, WINPR_ASSERTING_INT_CAST(int32_t, aaudio->format.nChannels));
#if __ANDROID_API__ >= 28
	AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
#endif

	AAudioStream* stream = nullptr;
	rc = AAudioStreamBuilder_openStream(builder, &stream);
	AAudioStreamBuilder_delete(builder);

	if (rc != AAUDIO_OK)
	{
		WLog_Print(aaudio->log, WLOG_ERROR, "AAudioStreamBuilder_openStream: %s",
		           AAudio_convertResultToText(rc));
		return FALSE;
	}

	/* The frame size is fixed by the negotiated format and there is no conversion here, so a
	 * stream that came back with a different channel count or sample format would have
	 * AAudioStream_read write past the accumulation buffer. */
	const int32_t actualRate = AAudioStream_getSampleRate(stream);
	const int32_t actualChannels = AAudioStream_getChannelCount(stream);
	const aaudio_format_t actualFormat = AAudioStream_getFormat(stream);

	if ((actualChannels < 0) || ((UINT32)actualChannels != aaudio->format.nChannels) ||
	    (actualFormat != AAUDIO_FORMAT_PCM_I16))
	{
		WLog_Print(aaudio->log, WLOG_ERROR,
		           "AAudio input opened as %dch/fmt%d, need %" PRIu16 "ch/I16", actualChannels,
		           (int)actualFormat, aaudio->format.nChannels);
		audin_aaudio_close_stream(aaudio, stream);
		return FALSE;
	}

	if ((actualRate < 0) || ((UINT32)actualRate != aaudio->format.nSamplesPerSec))
	{
		WLog_Print(aaudio->log, WLOG_WARN,
		           "AAudio input rate adjusted: requested %" PRIu32 "Hz, got %dHz",
		           aaudio->format.nSamplesPerSec, actualRate);
	}

	rc = AAudioStream_requestStart(stream);
	if (rc != AAUDIO_OK)
	{
		WLog_Print(aaudio->log, WLOG_ERROR, "AAudioStream_requestStart: %s",
		           AAudio_convertResultToText(rc));
		audin_aaudio_close_stream(aaudio, stream);
		return FALSE;
	}

	EnterCriticalSection(&aaudio->lock);
	aaudio->stream = stream;
	LeaveCriticalSection(&aaudio->lock);
	return TRUE;
}

/* Close and forget the capture stream, if it is still the one the caller saw. */
static void audin_aaudio_drop_stream(AudinAAudioDevice* aaudio, AAudioStream* stream)
{
	WINPR_ASSERT(aaudio);

	EnterCriticalSection(&aaudio->lock);
	if (aaudio->stream && (!stream || (aaudio->stream == stream)))
	{
		audin_aaudio_stop_stream(aaudio, aaudio->stream);
		audin_aaudio_close_stream(aaudio, aaudio->stream);
		aaudio->stream = nullptr;
	}
	LeaveCriticalSection(&aaudio->lock);
}

WINPR_ATTR_NODISCARD
static DWORD WINAPI audin_aaudio_thread(LPVOID arg)
{
	AudinAAudioDevice* aaudio = (AudinAAudioDevice*)arg;
	WINPR_ASSERT(aaudio);
	WINPR_ASSERT(aaudio->receive);
	WINPR_ASSERT(aaudio->stopEvent);

	const size_t bytesPerFrame = aaudio->format.nChannels * sizeof(INT16);
	const size_t framesPerPacket = aaudio->frames_per_packet;
	UINT error = CHANNEL_RC_OK;

	if ((bytesPerFrame == 0) || (framesPerPacket == 0) || (framesPerPacket > INT32_MAX) ||
	    (framesPerPacket > SIZE_MAX / bytesPerFrame))
	{
		audin_aaudio_drop_stream(aaudio, nullptr);
		return ERROR_INVALID_DATA;
	}

	const size_t accumulationBytes = framesPerPacket * bytesPerFrame;
	BYTE* accumulationBuffer = (BYTE*)calloc(framesPerPacket, bytesPerFrame);
	if (!accumulationBuffer)
	{
		audin_aaudio_drop_stream(aaudio, nullptr);
		return CHANNEL_RC_NO_MEMORY;
	}

	size_t currentFrames = 0;

	while (WaitForSingleObject(aaudio->stopEvent, 0) != WAIT_OBJECT_0)
	{
		EnterCriticalSection(&aaudio->lock);
		AAudioStream* stream = aaudio->stream;
		LeaveCriticalSection(&aaudio->lock);

		/* Reconnect loop if disconnected */
		if (!stream)
		{
			if (!audin_aaudio_open_stream(aaudio))
			{
				if (WaitForSingleObject(aaudio->stopEvent, 500) == WAIT_OBJECT_0)
					break;
			}
			continue;
		}

		const int32_t framesToRead =
		    WINPR_ASSERTING_INT_CAST(int32_t, framesPerPacket - currentFrames);
		BYTE* writePtr = accumulationBuffer + (currentFrames * bytesPerFrame);

		/* Read outside the lock: this thread is the only one closing the stream it reads from,
		 * Close stops it from outside and only closes it after joining. */
		aaudio_result_t r =
		    AAudioStream_read(stream, writePtr, framesToRead, AAUDIO_READ_TIMEOUT_NS);

		if (r < 0)
		{
			if (WaitForSingleObject(aaudio->stopEvent, 0) == WAIT_OBJECT_0)
				break;

			WLog_Print(aaudio->log, WLOG_ERROR, "AAudioStream_read: %s",
			           AAudio_convertResultToText(r));
			/* A timeout is transient. Everything else (route change, audioserver restart,
			 * internal fault) leaves the stream unusable, so drop it and reopen on the next
			 * iteration; reading the dead handle again would loop forever. */
			if (r != AAUDIO_ERROR_TIMEOUT)
			{
				audin_aaudio_drop_stream(aaudio, stream);
				currentFrames = 0;
			}

			/* Let the route settle; reopening into a half-torn-down route just fails again
			 * and spins on the audio server. */
			if (WaitForSingleObject(aaudio->stopEvent, 100) == WAIT_OBJECT_0)
				break;
			continue;
		}

		if (r == 0)
			continue;

		if (r > framesToRead)
		{
			WLog_Print(aaudio->log, WLOG_ERROR, "AAudioStream_read returned %d > %d frames", r,
			           framesToRead);
			audin_aaudio_drop_stream(aaudio, stream);
			currentFrames = 0;
			continue;
		}

		currentFrames += (size_t)r;

		/* Only dispatch full framesPerPacket packets to prevent codec frame corruption */
		if (currentFrames == framesPerPacket)
		{
			error = aaudio->receive(&aaudio->format, accumulationBuffer, accumulationBytes,
			                        aaudio->user_data);
			currentFrames = 0;
			if (error)
			{
				if (aaudio->rdpcontext)
					setChannelError(aaudio->rdpcontext, error,
					                "audin_aaudio receive reported an error");
				break;
			}
		}
	}

	audin_aaudio_drop_stream(aaudio, nullptr);
	free(accumulationBuffer);
	return error;
}

WINPR_ATTR_NODISCARD
static BOOL audin_aaudio_format_supported(IAudinDevice* device, const AUDIO_FORMAT* format)
{
	AudinAAudioDevice* aaudio = (AudinAAudioDevice*)device;

	if (!aaudio || !format)
		return FALSE;

	return audin_aaudio_pcm_valid(format);
}

WINPR_ATTR_NODISCARD
static UINT audin_aaudio_set_format(IAudinDevice* device, const AUDIO_FORMAT* format,
                                    UINT32 FramesPerPacket)
{
	AudinAAudioDevice* aaudio = (AudinAAudioDevice*)device;

	if (!aaudio || !format)
		return ERROR_INVALID_PARAMETER;

	if (!audin_aaudio_pcm_valid(format))
		return ERROR_UNSUPPORTED_TYPE;

	const size_t bytesPerFrame = format->nChannels * sizeof(INT16);
	const UINT32 frames = (FramesPerPacket > 0) ? FramesPerPacket : 1;
	if ((frames > INT32_MAX) || (frames > SIZE_MAX / bytesPerFrame))
	{
		WLog_Print(aaudio->log, WLOG_ERROR, "invalid FramesPerPacket %" PRIu32, FramesPerPacket);
		return ERROR_INVALID_DATA;
	}

	aaudio->format = *format;
	aaudio->frames_per_packet = frames;
	return CHANNEL_RC_OK;
}

WINPR_ATTR_NODISCARD
static UINT audin_aaudio_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	AudinAAudioDevice* aaudio = (AudinAAudioDevice*)device;

	if (!aaudio || !receive || !user_data)
		return ERROR_INVALID_PARAMETER;

	/* Reap a capture thread that already exited, otherwise its stale handle blocks the
	 * restart forever. */
	if (aaudio->thread && (WaitForSingleObject(aaudio->thread, 0) == WAIT_OBJECT_0))
	{
		if (!CloseHandle(aaudio->thread))
			WLog_Print(aaudio->log, WLOG_WARN, "CloseHandle on the exited capture thread failed");
		aaudio->thread = nullptr;
	}

	if (aaudio->stream || aaudio->thread)
		return CHANNEL_RC_OK;

	/* SetFormat must have negotiated a valid PCM format first */
	if ((aaudio->format.nSamplesPerSec == 0) ||
	    ((aaudio->format.nChannels != 1) && (aaudio->format.nChannels != 2)))
	{
		WLog_Print(aaudio->log, WLOG_ERROR, "SetFormat must run with a valid PCM format first");
		return ERROR_INVALID_PARAMETER;
	}

	aaudio->receive = receive;
	aaudio->user_data = user_data;

	if (!audin_aaudio_open_stream(aaudio))
		goto error_out;

	if (aaudio->stopEvent)
	{
		if (!ResetEvent(aaudio->stopEvent))
		{
			WLog_Print(aaudio->log, WLOG_ERROR, "ResetEvent failed");
			goto error_out;
		}
	}
	else
	{
		aaudio->stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		if (!aaudio->stopEvent)
		{
			WLog_Print(aaudio->log, WLOG_ERROR, "CreateEvent failed");
			goto error_out;
		}
	}

	aaudio->thread = CreateThread(nullptr, 0, audin_aaudio_thread, aaudio, 0, nullptr);
	if (!aaudio->thread)
	{
		WLog_Print(aaudio->log, WLOG_ERROR, "CreateThread failed");
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	if (audin_aaudio_close(device) != CHANNEL_RC_OK)
		WLog_Print(aaudio->log, WLOG_WARN, "audin_aaudio_close failed");
	return ERROR_INTERNAL_ERROR;
}

WINPR_ATTR_NODISCARD
static UINT audin_aaudio_close(IAudinDevice* device)
{
	AudinAAudioDevice* aaudio = (AudinAAudioDevice*)device;

	if (!aaudio)
		return ERROR_INVALID_PARAMETER;

	if (aaudio->stopEvent && !SetEvent(aaudio->stopEvent))
		WLog_Print(aaudio->log, WLOG_WARN, "SetEvent failed");

	/* Unblock a pending read; requestStop on a stream being read is safe, closing it is not. */
	EnterCriticalSection(&aaudio->lock);
	if (aaudio->stream)
		audin_aaudio_stop_stream(aaudio, aaudio->stream);
	LeaveCriticalSection(&aaudio->lock);

	if (aaudio->thread)
	{
		if (WaitForSingleObject(aaudio->thread, INFINITE) != WAIT_OBJECT_0)
			WLog_Print(aaudio->log, WLOG_WARN, "WaitForSingleObject failed");
		if (!CloseHandle(aaudio->thread))
			WLog_Print(aaudio->log, WLOG_WARN, "CloseHandle failed");
		aaudio->thread = nullptr;
	}

	EnterCriticalSection(&aaudio->lock);
	if (aaudio->stream)
	{
		audin_aaudio_close_stream(aaudio, aaudio->stream);
		aaudio->stream = nullptr;
	}
	LeaveCriticalSection(&aaudio->lock);

	if (aaudio->stopEvent)
	{
		if (!CloseHandle(aaudio->stopEvent))
			WLog_Print(aaudio->log, WLOG_WARN, "CloseHandle failed");
		aaudio->stopEvent = nullptr;
	}

	aaudio->receive = nullptr;
	aaudio->user_data = nullptr;
	return CHANNEL_RC_OK;
}

WINPR_ATTR_NODISCARD
static UINT audin_aaudio_free(IAudinDevice* device)
{
	AudinAAudioDevice* aaudio = (AudinAAudioDevice*)device;

	if (!aaudio)
		return ERROR_INVALID_PARAMETER;

	if (audin_aaudio_close(device) != CHANNEL_RC_OK)
		WLog_Print(aaudio->log, WLOG_WARN, "audin_aaudio_close failed");
	DeleteCriticalSection(&aaudio->lock);
	free(aaudio);
	return CHANNEL_RC_OK;
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE aaudio_freerdp_audin_client_subsystem_entry(
    PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints))
{
	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(pEntryPoints->pRegisterAudinDevice);

	UINT error = ERROR_INTERNAL_ERROR;
	AudinAAudioDevice* aaudio = (AudinAAudioDevice*)calloc(1, sizeof(AudinAAudioDevice));

	if (!aaudio)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	InitializeCriticalSection(&aaudio->lock);

	aaudio->log = WLog_Get(TAG);
	aaudio->iface.Open = audin_aaudio_open;
	aaudio->iface.FormatSupported = audin_aaudio_format_supported;
	aaudio->iface.SetFormat = audin_aaudio_set_format;
	aaudio->iface.Close = audin_aaudio_close;
	aaudio->iface.Free = audin_aaudio_free;
	aaudio->rdpcontext = pEntryPoints->rdpcontext;

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*)aaudio)))
	{
		WLog_Print(aaudio->log, WLOG_ERROR, "RegisterAudinDevice failed with error %" PRIu32 "!",
		           error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	DeleteCriticalSection(&aaudio->lock);
	free(aaudio);
	return error;
}
