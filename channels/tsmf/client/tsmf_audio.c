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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tsmf_audio.h"

static ITSMFAudioDevice* tsmf_load_audio_device_by_name(const char* name, const char* device)
{
	ITSMFAudioDevice* audio;
	TSMF_AUDIO_DEVICE_ENTRY entry;

	entry =
	    (TSMF_AUDIO_DEVICE_ENTRY)(void*)freerdp_load_channel_addin_entry("tsmf", name, "audio", 0);

	if (!entry)
		return NULL;

	audio = entry();

	if (!audio)
	{
		WLog_ERR(TAG, "failed to call export function in %s", name);
		return NULL;
	}

	if (!audio->Open(audio, device))
	{
		audio->Free(audio);
		audio = NULL;
		WLog_ERR(TAG, "failed to open, name: %s, device: %s", name, device);
	}
	else
	{
		WLog_DBG(TAG, "name: %s, device: %s", name, device);
	}

	return audio;
}

ITSMFAudioDevice* tsmf_load_audio_device(const char* name, const char* device)
{
	ITSMFAudioDevice* audio = NULL;

	if (name)
	{
		audio = tsmf_load_audio_device_by_name(name, device);
	}
	else
	{
#if defined(WITH_PULSE)
		if (!audio)
			audio = tsmf_load_audio_device_by_name("pulse", device);
#endif

#if defined(WITH_OSS)
		if (!audio)
			audio = tsmf_load_audio_device_by_name("oss", device);
#endif

#if defined(WITH_ALSA)
		if (!audio)
			audio = tsmf_load_audio_device_by_name("alsa", device);
#endif
	}

	if (audio == NULL)
	{
		WLog_ERR(TAG, "no sound device.");
	}
	else
	{
		WLog_DBG(TAG, "name: %s, device: %s", name, device);
	}

	return audio;
}
