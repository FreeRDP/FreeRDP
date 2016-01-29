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
#include <freerdp/codec/dsp.h>

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
	int audioBufferIndex;
    
	AudioQueueRef audioQueue;
	AudioStreamBasicDescription audioFormat;
	AudioQueueBufferRef audioBuffers[MAC_AUDIO_QUEUE_NUM_BUFFERS];
	
	Float64 lastStartTime;
	
	int wformat;
	int block_size;
	FREERDP_DSP_CONTEXT* dsp_context;
};
typedef struct rdpsnd_mac_plugin rdpsndMacPlugin;

static void mac_audio_queue_output_cb(void* inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer)
{
	
}

static BOOL rdpsnd_mac_set_format(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;
	
	mac->latency = (UINT32) latency;
	CopyMemory(&(mac->format), format, sizeof(AUDIO_FORMAT));
	
	mac->audioFormat.mSampleRate = format->nSamplesPerSec;
	mac->audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	mac->audioFormat.mFramesPerPacket = 1;
	mac->audioFormat.mChannelsPerFrame = format->nChannels;
	mac->audioFormat.mBitsPerChannel = format->wBitsPerSample;
	mac->audioFormat.mBytesPerFrame = (format->wBitsPerSample * format->nChannels) / 8;
	mac->audioFormat.mBytesPerPacket = format->nBlockAlign;
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
			
		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			mac->audioFormat.mFormatID = kAudioFormatLinearPCM;
			mac->audioFormat.mBitsPerChannel = 16;
			mac->audioFormat.mBytesPerFrame = (16 * format->nChannels) / 8;
			mac->audioFormat.mBytesPerPacket = mac->audioFormat.mFramesPerPacket * mac->audioFormat.mBytesPerFrame;
			break;
			
		case WAVE_FORMAT_GSM610:
			mac->audioFormat.mFormatID = kAudioFormatMicrosoftGSM;
			break;
			
		default:
			break;
	}
	
	mac->wformat = format->wFormatTag;
	mac->block_size = format->nBlockAlign;
	
	rdpsnd_print_audio_format(format);
	return TRUE;
}

static BOOL rdpsnd_mac_open(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	int index;
	OSStatus status;
    
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;
	
	if (mac->isOpen)
		return TRUE;
    
	mac->audioBufferIndex = 0;
    
	if (!device->SetFormat(device, format, 0))
	{
		WLog_ERR(TAG, "SetFormat failure\n");
		return FALSE;
	}
    
	freerdp_dsp_context_reset_adpcm(mac->dsp_context);

	status = AudioQueueNewOutput(&(mac->audioFormat),
				     mac_audio_queue_output_cb, mac,
				     NULL, NULL, 0, &(mac->audioQueue));
	
	if (status != 0)
	{
		WLog_ERR(TAG, "AudioQueueNewOutput failure\n");
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
		status = AudioQueueAllocateBuffer(mac->audioQueue, MAC_AUDIO_QUEUE_BUFFER_SIZE, &mac->audioBuffers[index]);
		
		if (status != 0)
		{
			WLog_ERR(TAG,  "AudioQueueAllocateBuffer failed\n");
			return FALSE;
		}
	}
	
	mac->lastStartTime = 0;
	
	mac->isOpen = TRUE;
	return TRUE;
}

static void rdpsnd_mac_close(rdpsndDevicePlugin* device)
{
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;
	
	if (mac->isOpen)
	{
		mac->isOpen = FALSE;
		
		AudioQueueStop(mac->audioQueue, true);
		
		AudioQueueDispose(mac->audioQueue, true);
		mac->audioQueue = NULL;
		
		mac->isPlaying = FALSE;
	}
}

static void rdpsnd_mac_free(rdpsndDevicePlugin* device)
{
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;
	
	device->Close(device);
	
	freerdp_dsp_context_free(mac->dsp_context);
	
	free(mac);
}

static BOOL rdpsnd_mac_format_supported(rdpsndDevicePlugin* device, AUDIO_FORMAT* format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
		case WAVE_FORMAT_ALAW:
		case WAVE_FORMAT_MULAW:
		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			return TRUE;
		case WAVE_FORMAT_GSM610:
			return FALSE;
	}
	
	return FALSE;
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

static BOOL rdpsnd_mac_wave_decode(rdpsndDevicePlugin* device, RDPSND_WAVE* wave)
{
	int length;
	BYTE* data;
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;
	
	if (mac->wformat == WAVE_FORMAT_ADPCM)
	{
		mac->dsp_context->decode_ms_adpcm(mac->dsp_context, wave->data, wave->length, mac->format.nChannels, mac->block_size);
		length = mac->dsp_context->adpcm_size;
		data = mac->dsp_context->adpcm_buffer;
	}
	else if (mac->wformat == WAVE_FORMAT_DVI_ADPCM)
	{
		mac->dsp_context->decode_ima_adpcm(mac->dsp_context, wave->data, wave->length, mac->format.nChannels, mac->block_size);
		length = mac->dsp_context->adpcm_size;
		data = mac->dsp_context->adpcm_buffer;
	}
	else
	{
		length = wave->length;
		data = wave->data;
	}
	
	wave->data = (BYTE*) malloc(length);
	CopyMemory(wave->data, data, length);
	wave->length = length;
	
	return TRUE;
}

static void rdpsnd_mac_waveplay(rdpsndDevicePlugin* device, RDPSND_WAVE* wave)
{
	int length;
	AudioQueueBufferRef audioBuffer;
	AudioTimeStamp outActualStartTime;
	rdpsndMacPlugin* mac = (rdpsndMacPlugin*) device;
	
	if (!mac->isOpen)
		return;

	audioBuffer = mac->audioBuffers[mac->audioBufferIndex];
    
	length = wave->length > audioBuffer->mAudioDataBytesCapacity ? audioBuffer->mAudioDataBytesCapacity : wave->length;
    
	CopyMemory(audioBuffer->mAudioData, wave->data, length);
	audioBuffer->mAudioDataByteSize = length;
	audioBuffer->mUserData = wave;
	
	AudioQueueEnqueueBufferWithParameters(mac->audioQueue, audioBuffer, 0, 0, 0, 0, 0, NULL, NULL, &outActualStartTime);
	UInt64 startTimeDelta = (outActualStartTime.mSampleTime - mac->lastStartTime) / 100.0;
	wave->wLocalTimeB = wave->wLocalTimeA + startTimeDelta + wave->wAudioLength;
	wave->wTimeStampB = wave->wTimeStampA + wave->wLocalTimeB - wave->wLocalTimeA;
	mac->lastStartTime = outActualStartTime.mSampleTime;
	
	mac->audioBufferIndex++;

	if (mac->audioBufferIndex >= MAC_AUDIO_QUEUE_NUM_BUFFERS)
	{
		mac->audioBufferIndex = 0;
	}
	
	device->Start(device);
}

#ifdef STATIC_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry	mac_freerdp_rdpsnd_client_subsystem_entry
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
	mac->device.SetFormat = rdpsnd_mac_set_format;
	mac->device.SetVolume = rdpsnd_mac_set_volume;
	mac->device.WaveDecode = rdpsnd_mac_wave_decode;
	mac->device.WavePlay = rdpsnd_mac_waveplay;
	mac->device.Start = rdpsnd_mac_start;
	mac->device.Close = rdpsnd_mac_close;
	mac->device.Free = rdpsnd_mac_free;
	
	mac->dsp_context = freerdp_dsp_context_new();

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*) mac);

	return CHANNEL_RC_OK;
}
