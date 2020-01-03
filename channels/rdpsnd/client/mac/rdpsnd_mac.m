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
#include <winpr/sysinfo.h>

#include <freerdp/types.h>

#include <AVFoundation/AVAudioBuffer.h>
#include <AVFoundation/AVFoundation.h>

#include "rdpsnd_main.h"

struct rdpsnd_mac_plugin
{
	rdpsndDevicePlugin device;

	BOOL isOpen;
	BOOL isPlaying;

	UINT32 latency;
	AUDIO_FORMAT format;

	AVAudioEngine *engine;
	AVAudioPlayerNode *player;
	UINT64 diff;
};
typedef struct rdpsnd_mac_plugin rdpsndMacPlugin;

static BOOL rdpsnd_mac_set_format(rdpsndDevicePlugin *device, const AUDIO_FORMAT *format,
                                  UINT32 latency)
{
	rdpsndMacPlugin *mac = (rdpsndMacPlugin *)device;
	if (!mac || !format)
		return FALSE;

	mac->latency = latency;
	mac->format = *format;

	audio_format_print(WLog_Get(TAG), WLOG_DEBUG, format);
	return TRUE;
}

static char *FormatError(OSStatus st)
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

static void rdpsnd_mac_release(rdpsndMacPlugin *mac)
{
	if (mac->player)
		[mac->player release];
	mac->player = NULL;

	if (mac->engine)
		[mac->engine release];
	mac->engine = NULL;
}

static BOOL rdpsnd_mac_open(rdpsndDevicePlugin *device, const AUDIO_FORMAT *format, UINT32 latency)
{
	AudioDeviceID outputDeviceID;
	UInt32 propertySize;
	OSStatus err;
	NSError *error;
	rdpsndMacPlugin *mac = (rdpsndMacPlugin *)device;
	AudioObjectPropertyAddress propertyAddress = { kAudioHardwarePropertyDefaultSystemOutputDevice,
		                                           kAudioObjectPropertyScopeGlobal,
		                                           kAudioObjectPropertyElementMaster };

	if (mac->isOpen)
		return TRUE;

	if (!rdpsnd_mac_set_format(device, format, latency))
		return FALSE;

	propertySize = sizeof(outputDeviceID);
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL,
	                                 &propertySize, &outputDeviceID);
	if (err)
	{
		WLog_ERR(TAG, "AudioHardwareGetProperty: %s", FormatError(err));
		return FALSE;
	}

	mac->engine = [[AVAudioEngine alloc] init];
	if (!mac->engine)
		return FALSE;

	if (@available(macOS 10.15, *))
	{
		/* Setting the output audio device on 10.15 or later breaks sound playback. Do not set for
		 * now until we find a proper fix for #5747 */
	}
	else
	{
		err = AudioUnitSetProperty(mac->engine.outputNode.audioUnit,
		                           kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global,
		                           0, &outputDeviceID, sizeof(outputDeviceID));
		if (err)
		{
			rdpsnd_mac_release(mac);
			WLog_ERR(TAG, "AudioUnitSetProperty: %s", FormatError(err));
			return FALSE;
		}
	}

	mac->player = [[AVAudioPlayerNode alloc] init];
	if (!mac->player)
	{
		rdpsnd_mac_release(mac);
		WLog_ERR(TAG, "AVAudioPlayerNode::init() failed");
		return FALSE;
	}

	[mac->engine attachNode:mac->player];

	[mac->engine connect:mac->player to:mac->engine.mainMixerNode format:nil];

	if (![mac->engine startAndReturnError:&error])
	{
		device->Close(device);
		WLog_ERR(TAG, "Failed to start audio player %s", [error.localizedDescription UTF8String]);
		return FALSE;
	}

	mac->isOpen = TRUE;
	return TRUE;
}

static void rdpsnd_mac_close(rdpsndDevicePlugin *device)
{
	rdpsndMacPlugin *mac = (rdpsndMacPlugin *)device;

	if (mac->isPlaying)
	{
		[mac->player stop];
		mac->isPlaying = FALSE;
	}

	if (mac->isOpen)
	{
		[mac->engine stop];
		mac->isOpen = FALSE;
	}

	rdpsnd_mac_release(mac);
}

static void rdpsnd_mac_free(rdpsndDevicePlugin *device)
{
	rdpsndMacPlugin *mac = (rdpsndMacPlugin *)device;
	device->Close(device);
	free(mac);
}

static BOOL rdpsnd_mac_format_supported(rdpsndDevicePlugin *device, const AUDIO_FORMAT *format)
{
	WINPR_UNUSED(device);

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (format->wBitsPerSample != 16)
				return FALSE;
			return TRUE;

		default:
			return FALSE;
	}
}

static BOOL rdpsnd_mac_set_volume(rdpsndDevicePlugin *device, UINT32 value)
{
	Float32 fVolume;
	UINT16 volumeLeft;
	UINT16 volumeRight;
	rdpsndMacPlugin *mac = (rdpsndMacPlugin *)device;

	if (!mac->player)
		return FALSE;

	volumeLeft = (value & 0xFFFF);
	volumeRight = ((value >> 16) & 0xFFFF);
	fVolume = ((float)volumeLeft) / 65535.0f;

	mac->player.volume = fVolume;

	return TRUE;
}

static void rdpsnd_mac_start(rdpsndDevicePlugin *device)
{
	rdpsndMacPlugin *mac = (rdpsndMacPlugin *)device;

	if (!mac->isPlaying)
	{
		[mac->player play];

		mac->isPlaying = TRUE;
		mac->diff = 100; /* Initial latency, corrected after first sample is played. */
	}
}

static UINT rdpsnd_mac_play(rdpsndDevicePlugin *device, const BYTE *data, size_t size)
{
	rdpsndMacPlugin *mac = (rdpsndMacPlugin *)device;
	AVAudioPCMBuffer *buffer;
	AVAudioFormat *format;
	float *const *db;
	size_t pos, step, x;
	AVAudioFrameCount count;
	UINT64 start = GetTickCount64();

	if (!mac->isOpen)
		return 0;

	step = 2 * mac->format.nChannels;

	count = size / step;
	format = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
	                                          sampleRate:mac->format.nSamplesPerSec
	                                            channels:mac->format.nChannels
	                                         interleaved:NO];
	if (!format)
	{
		WLog_WARN(TAG, "AVAudioFormat::init() failed");
		return 0;
	}
	buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format frameCapacity:count];
	if (!buffer)
	{
		[format release];
		WLog_WARN(TAG, "AVAudioPCMBuffer::init() failed");
		return 0;
	}

	buffer.frameLength = buffer.frameCapacity;
	db = buffer.floatChannelData;

	for (pos = 0; pos < count; pos++)
	{
		const BYTE *d = &data[pos * step];
		for (x = 0; x < mac->format.nChannels; x++)
		{
			const float val = (int16_t)((uint16_t)d[0] | ((uint16_t)d[1] << 8)) / 32768.0f;
			db[x][pos] = val;
			d += sizeof(int16_t);
		}
	}

	rdpsnd_mac_start(device);

	[mac->player scheduleBuffer:buffer
	          completionHandler:^{
		          UINT64 stop = GetTickCount64();
		          if (start > stop)
			          mac->diff = 0;
		          else
			          mac->diff = stop - start;
	          }];

	return mac->diff > UINT_MAX ? UINT_MAX : mac->diff;
}

#ifdef BUILTIN_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry mac_freerdp_rdpsnd_client_subsystem_entry
#else
#define freerdp_rdpsnd_client_subsystem_entry FREERDP_API freerdp_rdpsnd_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	rdpsndMacPlugin *mac;
	mac = (rdpsndMacPlugin *)calloc(1, sizeof(rdpsndMacPlugin));

	if (!mac)
		return CHANNEL_RC_NO_MEMORY;

	mac->device.Open = rdpsnd_mac_open;
	mac->device.FormatSupported = rdpsnd_mac_format_supported;
	mac->device.SetVolume = rdpsnd_mac_set_volume;
	mac->device.Play = rdpsnd_mac_play;
	mac->device.Close = rdpsnd_mac_close;
	mac->device.Free = rdpsnd_mac_free;
	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin *)mac);
	return CHANNEL_RC_OK;
}
