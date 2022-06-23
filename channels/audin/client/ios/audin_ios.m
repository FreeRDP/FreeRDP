/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - iOS implementation
 *
 * Copyright (c) 2015 Armin Novak <armin.novak@thincast.com>
 * Copyright 2015 Thincast Technologies GmbH
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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/thread.h>
#include <winpr/debug.h>
#include <winpr/cmdline.h>

#import <AVFoundation/AVFoundation.h>

#define __COREFOUNDATION_CFPLUGINCOM__ 1
#define IUNKNOWN_C_GUTS   \
	void *_reserved;      \
	void *QueryInterface; \
	void *AddRef;         \
	void *Release

#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/AudioQueue.h>

#include <freerdp/addin.h>
#include <freerdp/channels/rdpsnd.h>

#include "audin_main.h"

#define IOS_AUDIO_QUEUE_NUM_BUFFERS 100

typedef struct
{
	IAudinDevice iface;

	AUDIO_FORMAT format;
	UINT32 FramesPerPacket;
	int dev_unit;

	AudinReceive receive;
	void *user_data;

	rdpContext *rdpcontext;

	bool isOpen;
	AudioQueueRef audioQueue;
	AudioStreamBasicDescription audioFormat;
	AudioQueueBufferRef audioBuffers[IOS_AUDIO_QUEUE_NUM_BUFFERS];
} AudinIosDevice;

static AudioFormatID audin_ios_get_format(const AUDIO_FORMAT *format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			return kAudioFormatLinearPCM;

		default:
			return 0;
	}
}

static AudioFormatFlags audin_ios_get_flags_for_format(const AUDIO_FORMAT *format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			return kAudioFormatFlagIsSignedInteger;

		default:
			return 0;
	}
}

static BOOL audin_ios_format_supported(IAudinDevice *device, const AUDIO_FORMAT *format)
{
	AudinIosDevice *ios = (AudinIosDevice *)device;
	AudioFormatID req_fmt = 0;

	if (device == NULL || format == NULL)
		return FALSE;

	req_fmt = audin_ios_get_format(format);

	if (req_fmt == 0)
		return FALSE;

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_ios_set_format(IAudinDevice *device, const AUDIO_FORMAT *format,
                                 UINT32 FramesPerPacket)
{
	AudinIosDevice *ios = (AudinIosDevice *)device;

	if (device == NULL || format == NULL)
		return ERROR_INVALID_PARAMETER;

	ios->FramesPerPacket = FramesPerPacket;
	ios->format = *format;
	WLog_INFO(TAG, "Audio Format %s [channels=%d, samples=%d, bits=%d]",
	          audio_format_get_tag_string(format->wFormatTag), format->nChannels,
	          format->nSamplesPerSec, format->wBitsPerSample);
	ios->audioFormat.mBitsPerChannel = format->wBitsPerSample;

	if (format->wBitsPerSample == 0)
		ios->audioFormat.mBitsPerChannel = 16;

	ios->audioFormat.mChannelsPerFrame = ios->format.nChannels;
	ios->audioFormat.mFramesPerPacket = 1;

	ios->audioFormat.mBytesPerFrame =
	    ios->audioFormat.mChannelsPerFrame * (ios->audioFormat.mBitsPerChannel / 8);
	ios->audioFormat.mBytesPerPacket =
	    ios->audioFormat.mBytesPerFrame * ios->audioFormat.mFramesPerPacket;

	ios->audioFormat.mFormatFlags = audin_ios_get_flags_for_format(format);
	ios->audioFormat.mFormatID = audin_ios_get_format(format);
	ios->audioFormat.mReserved = 0;
	ios->audioFormat.mSampleRate = ios->format.nSamplesPerSec;
	return CHANNEL_RC_OK;
}

static void ios_audio_queue_input_cb(void *aqData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer,
                                     const AudioTimeStamp *inStartTime, UInt32 inNumPackets,
                                     const AudioStreamPacketDescription *inPacketDesc)
{
	AudinIosDevice *ios = (AudinIosDevice *)aqData;
	UINT error = CHANNEL_RC_OK;
	const BYTE *buffer = inBuffer->mAudioData;
	int buffer_size = inBuffer->mAudioDataByteSize;
	(void)inAQ;
	(void)inStartTime;
	(void)inNumPackets;
	(void)inPacketDesc;

	if (buffer_size > 0)
		error = ios->receive(&ios->format, buffer, buffer_size, ios->user_data);

	AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);

	if (error)
	{
		WLog_ERR(TAG, "ios->receive failed with error %" PRIu32 "", error);
		SetLastError(ERROR_INTERNAL_ERROR);
	}
}

static UINT audin_ios_close(IAudinDevice *device)
{
	UINT errCode = CHANNEL_RC_OK;
	char errString[1024];
	OSStatus devStat;
	AudinIosDevice *ios = (AudinIosDevice *)device;

	if (device == NULL)
		return ERROR_INVALID_PARAMETER;

	if (ios->isOpen)
	{
		devStat = AudioQueueStop(ios->audioQueue, true);

		if (devStat != 0)
		{
			errCode = GetLastError();
			WLog_ERR(TAG, "AudioQueueStop failed with %s [%" PRIu32 "]",
			         winpr_strerror(errCode, errString, sizeof(errString)), errCode);
		}

		ios->isOpen = false;
	}

	if (ios->audioQueue)
	{
		devStat = AudioQueueDispose(ios->audioQueue, true);

		if (devStat != 0)
		{
			errCode = GetLastError();
			WLog_ERR(TAG, "AudioQueueDispose failed with %s [%" PRIu32 "]",
			         winpr_strerror(errCode, errString, sizeof(errString)), errCode);
		}

		ios->audioQueue = NULL;
	}

	ios->receive = NULL;
	ios->user_data = NULL;
	return errCode;
}

static UINT audin_ios_open(IAudinDevice *device, AudinReceive receive, void *user_data)
{
	AudinIosDevice *ios = (AudinIosDevice *)device;
	DWORD errCode;
	char errString[1024];
	OSStatus devStat;
	size_t index;

	ios->receive = receive;
	ios->user_data = user_data;
	devStat = AudioQueueNewInput(&(ios->audioFormat), ios_audio_queue_input_cb, ios, NULL,
	                             kCFRunLoopCommonModes, 0, &(ios->audioQueue));

	if (devStat != 0)
	{
		errCode = GetLastError();
		WLog_ERR(TAG, "AudioQueueNewInput failed with %s [%" PRIu32 "]",
		         winpr_strerror(errCode, errString, sizeof(errString)), errCode);
		goto err_out;
	}

	for (index = 0; index < IOS_AUDIO_QUEUE_NUM_BUFFERS; index++)
	{
		devStat = AudioQueueAllocateBuffer(ios->audioQueue,
		                                   ios->FramesPerPacket * 2 * ios->format.nChannels,
		                                   &ios->audioBuffers[index]);

		if (devStat != 0)
		{
			errCode = GetLastError();
			WLog_ERR(TAG, "AudioQueueAllocateBuffer failed with %s [%" PRIu32 "]",
			         winpr_strerror(errCode, errString, sizeof(errString)), errCode);
			goto err_out;
		}

		devStat = AudioQueueEnqueueBuffer(ios->audioQueue, ios->audioBuffers[index], 0, NULL);

		if (devStat != 0)
		{
			errCode = GetLastError();
			WLog_ERR(TAG, "AudioQueueEnqueueBuffer failed with %s [%" PRIu32 "]",
			         winpr_strerror(errCode, errString, sizeof(errString)), errCode);
			goto err_out;
		}
	}

	devStat = AudioQueueStart(ios->audioQueue, NULL);

	if (devStat != 0)
	{
		errCode = GetLastError();
		WLog_ERR(TAG, "AudioQueueStart failed with %s [%" PRIu32 "]",
		         winpr_strerror(errCode, errString, sizeof(errString)), errCode);
		goto err_out;
	}

	ios->isOpen = true;
	return CHANNEL_RC_OK;
err_out:
	audin_ios_close(device);
	return CHANNEL_RC_INITIALIZATION_ERROR;
}

static UINT audin_ios_free(IAudinDevice *device)
{
	AudinIosDevice *ios = (AudinIosDevice *)device;
	int error;

	if (device == NULL)
		return ERROR_INVALID_PARAMETER;

	if ((error = audin_ios_close(device)))
	{
		WLog_ERR(TAG, "audin_oss_close failed with error code %d!", error);
	}

	free(ios);
	return CHANNEL_RC_OK;
}

UINT ios_freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	DWORD errCode;
	char errString[1024];
	const ADDIN_ARGV *args;
	AudinIosDevice *ios;
	UINT error;
	ios = (AudinIosDevice *)calloc(1, sizeof(AudinIosDevice));

	if (!ios)
	{
		errCode = GetLastError();
		WLog_ERR(TAG, "calloc failed with %s [%" PRIu32 "]",
		         winpr_strerror(errCode, errString, sizeof(errString)), errCode);
		return CHANNEL_RC_NO_MEMORY;
	}

	ios->iface.Open = audin_ios_open;
	ios->iface.FormatSupported = audin_ios_format_supported;
	ios->iface.SetFormat = audin_ios_set_format;
	ios->iface.Close = audin_ios_close;
	ios->iface.Free = audin_ios_free;
	ios->rdpcontext = pEntryPoints->rdpcontext;
	ios->dev_unit = -1;
	args = pEntryPoints->args;

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice *)ios)))
	{
		WLog_ERR(TAG, "RegisterAudinDevice failed with error %" PRIu32 "!", error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	free(ios);
	return error;
}
