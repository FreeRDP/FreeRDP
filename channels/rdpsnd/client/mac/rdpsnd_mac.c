/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2012 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Inuvika Inc.
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#include <winpr/crt.h>

#include <freerdp/types.h>

#define __COREFOUNDATION_CFPLUGINCOM__ 1
#define IUNKNOWN_C_GUTS void *_reserved; void* QueryInterface; void* AddRef; void* Release

#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/AudioQueue.h>

#include "rdpsnd_main.h"

#define MAC_AUDIO_QUEUE_NUM_BUFFERS	10
#define MAC_AUDIO_QUEUE_BUFFER_SIZE	32768

struct rdpsnd_mac_plugin
{
	rdpsndDevicePlugin device;

	BOOL isOpen;
	BOOL isPlaying;

	UINT32 latency;
	AUDIO_FORMAT format;
	size_t lastAudioBufferIndex;
	size_t audioBufferIndex;

	AudioQueueRef audioQueue;
	AudioStreamBasicDescription audioFormat;
	AudioQueueBufferRef audioBuffers[MAC_AUDIO_QUEUE_NUM_BUFFERS];
};
typedef struct rdpsnd_mac_plugin rdpsndMacPlugin;

static void mac_audio_queue_output_cb(void* inUserData, AudioQueueRef inAQ,
                                      AudioQueueBufferRef inBuffer)
{
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*)inUserData;

	if (inBuffer == mac->audioBuffers[mac->lastAudioBufferIndex])
	{
		AudioQueuePause(mac->audioQueue);
		mac->isPlaying = FALSE;
	}
}

static BOOL rdpsnd_mac_set_format(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                                  UINT32 latency)
{
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;
	mac->latency = (UINT32) latency;
	CopyMemory(&(mac->format), format, sizeof(AUDIO_FORMAT));
	mac->audioFormat.mSampleRate = format->nSamplesPerSec;
	mac->audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	mac->audioFormat.mFramesPerPacket = 1;
	mac->audioFormat.mChannelsPerFrame = format->nChannels;
	mac->audioFormat.mBitsPerChannel = format->wBitsPerSample;
	mac->audioFormat.mBytesPerFrame = mac->audioFormat.mBitsPerChannel *
	                                  mac->audioFormat.mChannelsPerFrame / 8;
	mac->audioFormat.mBytesPerPacket = mac->audioFormat.mBytesPerFrame *
	                                   mac->audioFormat.mFramesPerPacket;
	mac->audioFormat.mReserved = 0;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_ALAW:
			mac->audioFormat.mFormatID = kAudioFormatALaw;
			break;

		case WAVE_FORMAT_MULAW:
			mac->audioFormat.mFormatID = kAudioFormatULaw;
			break;

		case WAVE_FORMAT_PCM:
			mac->audioFormat.mFormatID = kAudioFormatLinearPCM;
			break;

		default:
			return FALSE;
	}

	audio_format_print(WLog_Get(TAG), WLOG_DEBUG, format);
	return TRUE;
}

static char* FormatError(OSStatus st)
{
	switch (st)
	{
		case kAudioFileUnspecifiedError:
			return "kAudioFileUnspecifiedError";

		case kAudioFileUnsupportedFileTypeError:
			return "kAudioFileUnsupportedFileTypeError";

		case kAudioFileUnsupportedDataFormatError:
			return "kAudioFileUnsupportedDataFormatError";

		case kAudioFileUnsupportedPropertyError:
			return "kAudioFileUnsupportedPropertyError";

		case kAudioFileBadPropertySizeError:
			return "kAudioFileBadPropertySizeError";

		case kAudioFilePermissionsError:
			return "kAudioFilePermissionsError";

		case kAudioFileNotOptimizedError:
			return "kAudioFileNotOptimizedError";

		case kAudioFileInvalidChunkError:
			return "kAudioFileInvalidChunkError";

		case kAudioFileDoesNotAllow64BitDataSizeError:
			return "kAudioFileDoesNotAllow64BitDataSizeError";

		case kAudioFileInvalidPacketOffsetError:
			return "kAudioFileInvalidPacketOffsetError";

		case kAudioFileInvalidFileError:
			return "kAudioFileInvalidFileError";

		case kAudioFileOperationNotSupportedError:
			return "kAudioFileOperationNotSupportedError";

		case kAudioFileNotOpenError:
			return "kAudioFileNotOpenError";

		case kAudioFileEndOfFileError:
			return "kAudioFileEndOfFileError";

		case kAudioFilePositionError:
			return "kAudioFilePositionError";

		case kAudioFileFileNotFoundError:
			return "kAudioFileFileNotFoundError";

		default:
			return "unknown error";
	}
}

static BOOL rdpsnd_mac_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format, UINT32 latency)
{
	int index;
	OSStatus status;
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;

	if (mac->isOpen)
		return TRUE;

	mac->audioBufferIndex = 0;

	if (!rdpsnd_mac_set_format(device, format, latency))
		return FALSE;

	status = AudioQueueNewOutput(&(mac->audioFormat),
	                             mac_audio_queue_output_cb, mac,
	                             NULL, NULL, 0, &(mac->audioQueue));

	if (status != 0)
	{
		const char* err = FormatError(status);
		WLog_ERR(TAG, "AudioQueueNewOutput failure %s", err);
		return FALSE;
	}

	UInt32 DecodeBufferSizeFrames;
	UInt32 propertySize = sizeof(DecodeBufferSizeFrames);
	status = AudioQueueGetProperty(mac->audioQueue,
	                               kAudioQueueProperty_DecodeBufferSizeFrames,
	                               &DecodeBufferSizeFrames,
	                               &propertySize);

	if (status != 0)
	{
		WLog_DBG(TAG, "AudioQueueGetProperty failure: kAudioQueueProperty_DecodeBufferSizeFrames\n");
		return FALSE;
	}

	for (index = 0; index < MAC_AUDIO_QUEUE_NUM_BUFFERS; index++)
	{
		status = AudioQueueAllocateBuffer(mac->audioQueue, MAC_AUDIO_QUEUE_BUFFER_SIZE,
		                                  &mac->audioBuffers[index]);

		if (status != 0)
		{
			WLog_ERR(TAG,  "AudioQueueAllocateBuffer failed\n");
			return FALSE;
		}
	}

	mac->isOpen = TRUE;
	return TRUE;
}

static void rdpsnd_mac_close(rdpsndDevicePlugin* device)
{
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;

	if (mac->isOpen)
	{
		size_t index;
		mac->isOpen = FALSE;
		AudioQueueStop(mac->audioQueue, true);

		for (index = 0; index < MAC_AUDIO_QUEUE_NUM_BUFFERS; index++)
		{
			AudioQueueFreeBuffer(mac->audioQueue, mac->audioBuffers[index]);
		}

		AudioQueueDispose(mac->audioQueue, true);
		mac->audioQueue = NULL;
		mac->isPlaying = FALSE;
	}
}

static void rdpsnd_mac_free(rdpsndDevicePlugin* device)
{
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;
	device->Close(device);
	free(mac);
}

static BOOL rdpsnd_mac_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
		case WAVE_FORMAT_ALAW:
		case WAVE_FORMAT_MULAW:
			return TRUE;

		default:
			return FALSE;
	}
}

static BOOL rdpsnd_mac_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	OSStatus status;
	Float32 fVolume;
	UINT16 volumeLeft;
	UINT16 volumeRight;
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;

	if (!mac->audioQueue)
		return FALSE;

	volumeLeft = (value & 0xFFFF);
	volumeRight = ((value >> 16) & 0xFFFF);
	fVolume = ((float) volumeLeft) / 65535.0;
	status = AudioQueueSetParameter(mac->audioQueue, kAudioQueueParam_Volume, fVolume);

	if (status != 0)
	{
		WLog_ERR(TAG,  "AudioQueueSetParameter kAudioQueueParam_Volume failed: %f\n", fVolume);
		return FALSE;
	}

	return TRUE;
}

static void rdpsnd_mac_start(rdpsndDevicePlugin* device)
{
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;

	if (!mac->isPlaying)
	{
		OSStatus status;

		if (!mac->audioQueue)
			return;

		status = AudioQueueStart(mac->audioQueue, NULL);

		if (status != 0)
		{
			WLog_ERR(TAG,  "AudioQueueStart failed\n");
		}

		mac->isPlaying = TRUE;
	}
}

static UINT rdpsnd_mac_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	size_t length;
	AudioQueueBufferRef audioBuffer;
	AudioTimeStamp outActualStartTime;
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;

	if (!mac->isOpen)
		return 0;

	audioBuffer = mac->audioBuffers[mac->audioBufferIndex];
	length = size > audioBuffer->mAudioDataBytesCapacity ? audioBuffer->mAudioDataBytesCapacity : size;
	CopyMemory(audioBuffer->mAudioData, data, length);
	audioBuffer->mAudioDataByteSize = length;
	audioBuffer->mUserData = mac;
	AudioQueueEnqueueBufferWithParameters(mac->audioQueue, audioBuffer, 0, 0, 0, 0, 0, NULL, NULL,
	                                      &outActualStartTime);
	mac->lastAudioBufferIndex = mac->audioBufferIndex;
	mac->audioBufferIndex++;
	mac->audioBufferIndex %= MAC_AUDIO_QUEUE_NUM_BUFFERS;
	rdpsnd_mac_start(device);
	return 10; /* TODO: Get real latencry in [ms] */
}

#ifdef BUILTIN_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry	mac_freerdp_rdpsnd_client_subsystem_entry
#else
#define freerdp_rdpsnd_client_subsystem_entry	FREERDP_API freerdp_rdpsnd_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	rdpsndMacPlugin* mac;
	mac = (rdpsndMacPlugin*) calloc(1, sizeof(rdpsndMacPlugin));

	if (!mac)
		return CHANNEL_RC_NO_MEMORY;

	mac->device.Open = rdpsnd_mac_open;
	mac->device.FormatSupported = rdpsnd_mac_format_supported;
	mac->device.SetVolume = rdpsnd_mac_set_volume;
	mac->device.Play = rdpsnd_mac_play;
	mac->device.Close = rdpsnd_mac_close;
	mac->device.Free = rdpsnd_mac_free;
	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*) mac);
	return CHANNEL_RC_OK;
}
