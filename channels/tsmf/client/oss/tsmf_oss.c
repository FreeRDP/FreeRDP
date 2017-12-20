/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - OSS Audio Device
 *
 * Copyright (c) 2015 Rozhuk Ivan <rozhuk.im@gmail.com>
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
#include <unistd.h>

#include <winpr/crt.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#if defined(__OpenBSD__)
    #include <soundcard.h>
#else
    #include <sys/soundcard.h>
#endif
#include <sys/ioctl.h>

#include <freerdp/types.h>
#include <freerdp/codec/dsp.h>

#include "tsmf_audio.h"


typedef struct _TSMFOSSAudioDevice
{
	ITSMFAudioDevice iface;

	char dev_name[PATH_MAX];
	int pcm_handle;

	UINT32 sample_rate;
	UINT32 channels;
	UINT32 bits_per_sample;

	UINT32 data_size_last;
} TSMFOssAudioDevice;


#define OSS_LOG_ERR(_text, _error) \
	if (_error != 0) \
		WLog_ERR(TAG, "%s: %i - %s", _text, _error, strerror(_error));


static BOOL tsmf_oss_open(ITSMFAudioDevice* audio, const char* device)
{
	int tmp;
	TSMFOssAudioDevice* oss = (TSMFOssAudioDevice*)audio;

	if (oss == NULL || oss->pcm_handle != -1)
		return FALSE;

	if (device == NULL)   /* Default device. */
	{
		strncpy(oss->dev_name, "/dev/dsp", sizeof(oss->dev_name));
	}
	else
	{
		strncpy(oss->dev_name, device, sizeof(oss->dev_name) - 1);
	}

	if ((oss->pcm_handle = open(oss->dev_name, O_WRONLY)) < 0)
	{
		OSS_LOG_ERR("sound dev open failed", errno);
		oss->pcm_handle = -1;
		return FALSE;
	}

#if 0 /* FreeBSD OSS implementation at this moment (2015.03) does not set PCM_CAP_OUTPUT flag. */
	tmp = 0;

	if (ioctl(oss->pcm_handle, SNDCTL_DSP_GETCAPS, &mask) == -1)
	{
		OSS_LOG_ERR("SNDCTL_DSP_GETCAPS failed, try ignory", errno);
	}
	else if ((mask & PCM_CAP_OUTPUT) == 0)
	{
		OSS_LOG_ERR("Device does not supports playback", EOPNOTSUPP);
		close(oss->pcm_handle);
		oss->pcm_handle = -1;
		return FALSE;
	}

#endif
	tmp = 0;

	if (ioctl(oss->pcm_handle, SNDCTL_DSP_GETFMTS, &tmp) == -1)
	{
		OSS_LOG_ERR("SNDCTL_DSP_GETFMTS failed", errno);
		close(oss->pcm_handle);
		oss->pcm_handle = -1;
		return FALSE;
	}

	if ((AFMT_S16_LE & tmp) == 0)
	{
		OSS_LOG_ERR("SNDCTL_DSP_GETFMTS - AFMT_S16_LE", EOPNOTSUPP);
		close(oss->pcm_handle);
		oss->pcm_handle = -1;
		return FALSE;
	}

	WLog_INFO(TAG, "open: %s", oss->dev_name);
	return TRUE;
}

static BOOL tsmf_oss_set_format(ITSMFAudioDevice* audio, UINT32 sample_rate, UINT32 channels, UINT32 bits_per_sample)
{
	int tmp;
	TSMFOssAudioDevice* oss = (TSMFOssAudioDevice*)audio;

	if (oss == NULL || oss->pcm_handle == -1)
		return FALSE;

	oss->sample_rate = sample_rate;
	oss->channels = channels;
	oss->bits_per_sample = bits_per_sample;
	tmp = AFMT_S16_LE;

	if (ioctl(oss->pcm_handle, SNDCTL_DSP_SETFMT, &tmp) == -1)
		OSS_LOG_ERR("SNDCTL_DSP_SETFMT failed", errno);

	tmp = channels;

	if (ioctl(oss->pcm_handle, SNDCTL_DSP_CHANNELS, &tmp) == -1)
		OSS_LOG_ERR("SNDCTL_DSP_CHANNELS failed", errno);

	tmp = sample_rate;

	if (ioctl(oss->pcm_handle, SNDCTL_DSP_SPEED, &tmp) == -1)
		OSS_LOG_ERR("SNDCTL_DSP_SPEED failed", errno);

	tmp = ((bits_per_sample / 8) * channels * sample_rate);

	if (ioctl(oss->pcm_handle, SNDCTL_DSP_SETFRAGMENT, &tmp) == -1)
		OSS_LOG_ERR("SNDCTL_DSP_SETFRAGMENT failed", errno);

	DEBUG_TSMF("sample_rate %"PRIu32" channels %"PRIu32" bits_per_sample %"PRIu32"",
			   sample_rate, channels, bits_per_sample);
	return TRUE;
}

static BOOL tsmf_oss_play(ITSMFAudioDevice* audio, BYTE* data, UINT32 data_size)
{
	int status;
	UINT32 offset;
	TSMFOssAudioDevice* oss = (TSMFOssAudioDevice*)audio;
	DEBUG_TSMF("tsmf_oss_play: data_size %"PRIu32"", data_size);

	if (oss == NULL || oss->pcm_handle == -1)
		return FALSE;

	if (data == NULL || data_size == 0)
	{
		free(data);
		return TRUE;
	}

	offset = 0;
	oss->data_size_last = data_size;

	while (offset < data_size)
	{
		status = write(oss->pcm_handle, &data[offset], (data_size - offset));

		if (status < 0)
		{
			OSS_LOG_ERR("write fail", errno);
			free(data);
			return FALSE;
		}

		offset += status;
	}

	free(data);
	return TRUE;
}

static UINT64 tsmf_oss_get_latency(ITSMFAudioDevice* audio)
{
	UINT64 latency = 0;
	TSMFOssAudioDevice* oss = (TSMFOssAudioDevice*)audio;

	if (oss == NULL)
		return 0;

	//latency = ((oss->data_size_last / (oss->bits_per_sample / 8)) * oss->sample_rate);
	//WLog_INFO(TAG, "latency: %zu", latency);
	return latency;
}

static BOOL tsmf_oss_flush(ITSMFAudioDevice* audio)
{
	return TRUE;
}

static void tsmf_oss_free(ITSMFAudioDevice* audio)
{
	TSMFOssAudioDevice* oss = (TSMFOssAudioDevice*)audio;

	if (oss == NULL)
		return;

	if (oss->pcm_handle != -1)
	{
		WLog_INFO(TAG, "close: %s", oss->dev_name);
		close(oss->pcm_handle);
	}

	free(oss);
}

#ifdef BUILTIN_CHANNELS
#define freerdp_tsmf_client_audio_subsystem_entry	oss_freerdp_tsmf_client_audio_subsystem_entry
#else
#define freerdp_tsmf_client_audio_subsystem_entry	FREERDP_API freerdp_tsmf_client_audio_subsystem_entry
#endif

ITSMFAudioDevice* freerdp_tsmf_client_audio_subsystem_entry(void)
{
	TSMFOssAudioDevice* oss;
	oss = (TSMFOssAudioDevice*)malloc(sizeof(TSMFOssAudioDevice));
	ZeroMemory(oss, sizeof(TSMFOssAudioDevice));
	oss->iface.Open = tsmf_oss_open;
	oss->iface.SetFormat = tsmf_oss_set_format;
	oss->iface.Play = tsmf_oss_play;
	oss->iface.GetLatency = tsmf_oss_get_latency;
	oss->iface.Flush = tsmf_oss_flush;
	oss->iface.Free = tsmf_oss_free;
	oss->pcm_handle = -1;
	return (ITSMFAudioDevice*)oss;
}
