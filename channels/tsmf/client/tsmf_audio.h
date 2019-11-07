/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Audio Device Manager
 *
 * Copyright 2010-2011 Vic Lee
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

#ifndef FREERDP_CHANNEL_TSMF_CLIENT_AUDIO_H
#define FREERDP_CHANNEL_TSMF_CLIENT_AUDIO_H

#include "tsmf_types.h"

typedef struct _ITSMFAudioDevice ITSMFAudioDevice;

struct _ITSMFAudioDevice
{
	/* Open the audio device. */
	BOOL (*Open)(ITSMFAudioDevice* audio, const char* device);
	/* Set the audio data format. */
	BOOL(*SetFormat)
	(ITSMFAudioDevice* audio, UINT32 sample_rate, UINT32 channels, UINT32 bits_per_sample);
	/* Play audio data. */
	BOOL (*Play)(ITSMFAudioDevice* audio, const BYTE* data, UINT32 data_size);
	/* Get the latency of the last written sample, in 100ns */
	UINT64 (*GetLatency)(ITSMFAudioDevice* audio);
	/* Change the playback volume level */
	BOOL (*ChangeVolume)(ITSMFAudioDevice* audio, UINT32 newVolume, UINT32 muted);
	/* Flush queued audio data */
	BOOL (*Flush)(ITSMFAudioDevice* audio);
	/* Free the audio device */
	void (*Free)(ITSMFAudioDevice* audio);
};

#define TSMF_AUDIO_DEVICE_EXPORT_FUNC_NAME "TSMFAudioDeviceEntry"
typedef ITSMFAudioDevice* (*TSMF_AUDIO_DEVICE_ENTRY)(void);

ITSMFAudioDevice* tsmf_load_audio_device(const char* name, const char* device);

#endif /* FREERDP_CHANNEL_TSMF_CLIENT_AUDIO_H */
