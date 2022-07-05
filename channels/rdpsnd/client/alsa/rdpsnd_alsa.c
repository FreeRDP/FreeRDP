/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>

#include <alsa/asoundlib.h>

#include <freerdp/types.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/channels/log.h>

#include "rdpsnd_main.h"

typedef struct
{
	rdpsndDevicePlugin device;

	UINT32 latency;
	AUDIO_FORMAT aformat;
	char* device_name;
	snd_pcm_t* pcm_handle;
	snd_mixer_t* mixer_handle;

	UINT32 actual_rate;
	snd_pcm_format_t format;
	UINT32 actual_channels;

	snd_pcm_uframes_t buffer_size;
	snd_pcm_uframes_t period_size;
} rdpsndAlsaPlugin;

#define SND_PCM_CHECK(_func, _status)                  \
	do                                                 \
	{                                                  \
		if (_status < 0)                               \
		{                                              \
			WLog_ERR(TAG, "%s: %d\n", _func, _status); \
			return -1;                                 \
		}                                              \
	} while (0)

static int rdpsnd_alsa_set_hw_params(rdpsndAlsaPlugin* alsa)
{
	int status;
	snd_pcm_hw_params_t* hw_params;
	snd_pcm_uframes_t buffer_size_max;
	status = snd_pcm_hw_params_malloc(&hw_params);
	SND_PCM_CHECK("snd_pcm_hw_params_malloc", status);
	status = snd_pcm_hw_params_any(alsa->pcm_handle, hw_params);
	SND_PCM_CHECK("snd_pcm_hw_params_any", status);
	/* Set interleaved read/write access */
	status =
	    snd_pcm_hw_params_set_access(alsa->pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	SND_PCM_CHECK("snd_pcm_hw_params_set_access", status);
	/* Set sample format */
	status = snd_pcm_hw_params_set_format(alsa->pcm_handle, hw_params, alsa->format);
	SND_PCM_CHECK("snd_pcm_hw_params_set_format", status);
	/* Set sample rate */
	status = snd_pcm_hw_params_set_rate_near(alsa->pcm_handle, hw_params, &alsa->actual_rate, NULL);
	SND_PCM_CHECK("snd_pcm_hw_params_set_rate_near", status);
	/* Set number of channels */
	status = snd_pcm_hw_params_set_channels(alsa->pcm_handle, hw_params, alsa->actual_channels);
	SND_PCM_CHECK("snd_pcm_hw_params_set_channels", status);
	/* Get maximum buffer size */
	status = snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_size_max);
	SND_PCM_CHECK("snd_pcm_hw_params_get_buffer_size_max", status);
	/**
	 * ALSA Parameters
	 *
	 * http://www.alsa-project.org/main/index.php/FramesPeriods
	 *
	 * buffer_size = period_size * periods
	 * period_bytes = period_size * bytes_per_frame
	 * bytes_per_frame = channels * bytes_per_sample
	 *
	 * A frame is equivalent of one sample being played,
	 * irrespective of the number of channels or the number of bits
	 *
	 * A period is the number of frames in between each hardware interrupt.
	 *
	 * The buffer size always has to be greater than one period size.
	 * Commonly this is (2 * period_size), but some hardware can do 8 periods per buffer.
	 * It is also possible for the buffer size to not be an integer multiple of the period size.
	 */
	int interrupts_per_sec_near = 50;
	int bytes_per_sec =
	    (alsa->actual_rate * alsa->aformat.wBitsPerSample / 8 * alsa->actual_channels);
	alsa->buffer_size = buffer_size_max;
	alsa->period_size = (bytes_per_sec / interrupts_per_sec_near);

	if (alsa->period_size > buffer_size_max)
	{
		WLog_ERR(TAG, "Warning: requested sound buffer size %lu, got %lu instead\n",
		         alsa->buffer_size, buffer_size_max);
		alsa->period_size = (buffer_size_max / 8);
	}

	/* Set buffer size */
	status =
	    snd_pcm_hw_params_set_buffer_size_near(alsa->pcm_handle, hw_params, &alsa->buffer_size);
	SND_PCM_CHECK("snd_pcm_hw_params_set_buffer_size_near", status);
	/* Set period size */
	status = snd_pcm_hw_params_set_period_size_near(alsa->pcm_handle, hw_params, &alsa->period_size,
	                                                NULL);
	SND_PCM_CHECK("snd_pcm_hw_params_set_period_size_near", status);
	status = snd_pcm_hw_params(alsa->pcm_handle, hw_params);
	SND_PCM_CHECK("snd_pcm_hw_params", status);
	snd_pcm_hw_params_free(hw_params);
	return 0;
}

static int rdpsnd_alsa_set_sw_params(rdpsndAlsaPlugin* alsa)
{
	int status;
	snd_pcm_sw_params_t* sw_params;
	status = snd_pcm_sw_params_malloc(&sw_params);
	SND_PCM_CHECK("snd_pcm_sw_params_malloc", status);
	status = snd_pcm_sw_params_current(alsa->pcm_handle, sw_params);
	SND_PCM_CHECK("snd_pcm_sw_params_current", status);
	status = snd_pcm_sw_params_set_avail_min(alsa->pcm_handle, sw_params,
	                                         (alsa->aformat.nChannels * alsa->actual_channels));
	SND_PCM_CHECK("snd_pcm_sw_params_set_avail_min", status);
	status = snd_pcm_sw_params_set_start_threshold(alsa->pcm_handle, sw_params,
	                                               alsa->aformat.nBlockAlign);
	SND_PCM_CHECK("snd_pcm_sw_params_set_start_threshold", status);
	status = snd_pcm_sw_params(alsa->pcm_handle, sw_params);
	SND_PCM_CHECK("snd_pcm_sw_params", status);
	snd_pcm_sw_params_free(sw_params);
	status = snd_pcm_prepare(alsa->pcm_handle);
	SND_PCM_CHECK("snd_pcm_prepare", status);
	return 0;
}

static int rdpsnd_alsa_validate_params(rdpsndAlsaPlugin* alsa)
{
	int status;
	snd_pcm_uframes_t buffer_size;
	snd_pcm_uframes_t period_size;
	status = snd_pcm_get_params(alsa->pcm_handle, &buffer_size, &period_size);
	SND_PCM_CHECK("snd_pcm_get_params", status);
	return 0;
}

static int rdpsnd_alsa_set_params(rdpsndAlsaPlugin* alsa)
{
	snd_pcm_drop(alsa->pcm_handle);

	if (rdpsnd_alsa_set_hw_params(alsa) < 0)
		return -1;

	if (rdpsnd_alsa_set_sw_params(alsa) < 0)
		return -1;

	return rdpsnd_alsa_validate_params(alsa);
}

static BOOL rdpsnd_alsa_set_format(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                                   UINT32 latency)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	if (format)
	{
		alsa->aformat = *format;
		alsa->actual_rate = format->nSamplesPerSec;
		alsa->actual_channels = format->nChannels;

		switch (format->wFormatTag)
		{
			case WAVE_FORMAT_PCM:
				switch (format->wBitsPerSample)
				{
					case 8:
						alsa->format = SND_PCM_FORMAT_S8;
						break;

					case 16:
						alsa->format = SND_PCM_FORMAT_S16_LE;
						break;

					default:
						return FALSE;
				}

				break;

			default:
				return FALSE;
		}
	}

	alsa->latency = latency;
	return (rdpsnd_alsa_set_params(alsa) == 0);
}

static void rdpsnd_alsa_close_mixer(rdpsndAlsaPlugin* alsa)
{
	if (alsa && alsa->mixer_handle)
	{
		snd_mixer_close(alsa->mixer_handle);
		alsa->mixer_handle = NULL;
	}
}

static BOOL rdpsnd_alsa_open_mixer(rdpsndAlsaPlugin* alsa)
{
	int status;

	if (alsa->mixer_handle)
		return TRUE;

	status = snd_mixer_open(&alsa->mixer_handle, 0);

	if (status < 0)
	{
		WLog_ERR(TAG, "snd_mixer_open failed");
		goto fail;
	}

	status = snd_mixer_attach(alsa->mixer_handle, alsa->device_name);

	if (status < 0)
	{
		WLog_ERR(TAG, "snd_mixer_attach failed");
		goto fail;
	}

	status = snd_mixer_selem_register(alsa->mixer_handle, NULL, NULL);

	if (status < 0)
	{
		WLog_ERR(TAG, "snd_mixer_selem_register failed");
		goto fail;
	}

	status = snd_mixer_load(alsa->mixer_handle);

	if (status < 0)
	{
		WLog_ERR(TAG, "snd_mixer_load failed");
		goto fail;
	}

	return TRUE;
fail:
	rdpsnd_alsa_close_mixer(alsa);
	return FALSE;
}

static void rdpsnd_alsa_pcm_close(rdpsndAlsaPlugin* alsa)
{
	if (alsa && alsa->pcm_handle)
	{
		snd_pcm_drain(alsa->pcm_handle);
		snd_pcm_close(alsa->pcm_handle);
		alsa->pcm_handle = 0;
	}
}

static BOOL rdpsnd_alsa_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format, UINT32 latency)
{
	int mode;
	int status;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	if (alsa->pcm_handle)
		return TRUE;

	mode = 0;
	/*mode |= SND_PCM_NONBLOCK;*/
	status = snd_pcm_open(&alsa->pcm_handle, alsa->device_name, SND_PCM_STREAM_PLAYBACK, mode);

	if (status < 0)
	{
		WLog_ERR(TAG, "snd_pcm_open failed");
		return FALSE;
	}

	return rdpsnd_alsa_set_format(device, format, latency) && rdpsnd_alsa_open_mixer(alsa);
}

static void rdpsnd_alsa_close(rdpsndDevicePlugin* device)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	if (!alsa)
		return;

	rdpsnd_alsa_close_mixer(alsa);
}

static void rdpsnd_alsa_free(rdpsndDevicePlugin* device)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;
	rdpsnd_alsa_pcm_close(alsa);
	rdpsnd_alsa_close_mixer(alsa);
	free(alsa->device_name);
	free(alsa);
}

static BOOL rdpsnd_alsa_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (format->cbSize == 0 && format->nSamplesPerSec <= 48000 &&
			    (format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
			    (format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}

			break;
	}

	return FALSE;
}

static UINT32 rdpsnd_alsa_get_volume(rdpsndDevicePlugin* device)
{
	long volume_min;
	long volume_max;
	long volume_left;
	long volume_right;
	UINT32 dwVolume;
	UINT16 dwVolumeLeft;
	UINT16 dwVolumeRight;
	snd_mixer_elem_t* elem;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;
	dwVolumeLeft = ((50 * 0xFFFF) / 100);  /* 50% */
	dwVolumeRight = ((50 * 0xFFFF) / 100); /* 50% */

	if (!rdpsnd_alsa_open_mixer(alsa))
		return 0;

	for (elem = snd_mixer_first_elem(alsa->mixer_handle); elem; elem = snd_mixer_elem_next(elem))
	{
		if (snd_mixer_selem_has_playback_volume(elem))
		{
			snd_mixer_selem_get_playback_volume_range(elem, &volume_min, &volume_max);
			snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume_left);
			snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &volume_right);
			dwVolumeLeft =
			    (UINT16)(((volume_left * 0xFFFF) - volume_min) / (volume_max - volume_min));
			dwVolumeRight =
			    (UINT16)(((volume_right * 0xFFFF) - volume_min) / (volume_max - volume_min));
			break;
		}
	}

	dwVolume = (dwVolumeLeft << 16) | dwVolumeRight;
	return dwVolume;
}

static BOOL rdpsnd_alsa_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	long left;
	long right;
	long volume_min;
	long volume_max;
	long volume_left;
	long volume_right;
	snd_mixer_elem_t* elem;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	if (!rdpsnd_alsa_open_mixer(alsa))
		return FALSE;

	left = (value & 0xFFFF);
	right = ((value >> 16) & 0xFFFF);

	for (elem = snd_mixer_first_elem(alsa->mixer_handle); elem; elem = snd_mixer_elem_next(elem))
	{
		if (snd_mixer_selem_has_playback_volume(elem))
		{
			snd_mixer_selem_get_playback_volume_range(elem, &volume_min, &volume_max);
			volume_left = volume_min + (left * (volume_max - volume_min)) / 0xFFFF;
			volume_right = volume_min + (right * (volume_max - volume_min)) / 0xFFFF;

			if ((snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, volume_left) <
			     0) ||
			    (snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT,
			                                         volume_right) < 0))
			{
				WLog_ERR(TAG, "error setting the volume\n");
				return FALSE;
			}
		}
	}

	return TRUE;
}

static UINT rdpsnd_alsa_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	UINT latency;
	size_t offset = 0;
	int frame_size;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;
	WINPR_ASSERT(alsa);
	WINPR_ASSERT(data || (size == 0));
	frame_size = alsa->actual_channels * alsa->aformat.wBitsPerSample / 8;
	if (frame_size <= 0)
		return 0;

	while (offset < size)
	{
		snd_pcm_sframes_t status =
		    snd_pcm_writei(alsa->pcm_handle, &data[offset], (size - offset) / frame_size);

		if (status < 0)
			status = snd_pcm_recover(alsa->pcm_handle, status, 0);

		if (status < 0)
		{
			WLog_ERR(TAG, "status: %d\n", status);
			rdpsnd_alsa_close(device);
			rdpsnd_alsa_open(device, NULL, alsa->latency);
			break;
		}

		offset += status * frame_size;
	}

	{
		snd_pcm_sframes_t available, delay;
		int rc = snd_pcm_avail_delay(alsa->pcm_handle, &available, &delay);

		if (rc != 0)
			latency = 0;
		else if (available == 0) /* Get [ms] from number of samples */
			latency = delay * 1000 / alsa->actual_rate;
		else
			latency = 0;
	}

	return latency + alsa->latency;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_alsa_parse_addin_args(rdpsndDevicePlugin* device, const ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	const COMMAND_LINE_ARGUMENT_A* arg;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;
	COMMAND_LINE_ARGUMENT_A rdpsnd_alsa_args[] = { { "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>",
		                                             NULL, NULL, -1, NULL, "device" },
		                                           { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL } };
	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, args->argv, rdpsnd_alsa_args, flags, alsa, NULL,
	                                    NULL);

	if (status < 0)
	{
		WLog_ERR(TAG, "CommandLineParseArgumentsA failed!");
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	arg = rdpsnd_alsa_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dev")
		{
			alsa->device_name = _strdup(arg->Value);

			if (!alsa->device_name)
				return CHANNEL_RC_NO_MEMORY;
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT alsa_freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	const ADDIN_ARGV* args;
	rdpsndAlsaPlugin* alsa;
	UINT error;
	alsa = (rdpsndAlsaPlugin*)calloc(1, sizeof(rdpsndAlsaPlugin));

	if (!alsa)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	alsa->device.Open = rdpsnd_alsa_open;
	alsa->device.FormatSupported = rdpsnd_alsa_format_supported;
	alsa->device.GetVolume = rdpsnd_alsa_get_volume;
	alsa->device.SetVolume = rdpsnd_alsa_set_volume;
	alsa->device.Play = rdpsnd_alsa_play;
	alsa->device.Close = rdpsnd_alsa_close;
	alsa->device.Free = rdpsnd_alsa_free;
	args = pEntryPoints->args;

	if (args->argc > 1)
	{
		if ((error = rdpsnd_alsa_parse_addin_args(&alsa->device, args)))
		{
			WLog_ERR(TAG, "rdpsnd_alsa_parse_addin_args failed with error %" PRIu32 "", error);
			goto error_parse_args;
		}
	}

	if (!alsa->device_name)
	{
		alsa->device_name = _strdup("default");

		if (!alsa->device_name)
		{
			WLog_ERR(TAG, "_strdup failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_strdup;
		}
	}

	alsa->pcm_handle = 0;
	alsa->actual_rate = 22050;
	alsa->format = SND_PCM_FORMAT_S16_LE;
	alsa->actual_channels = 2;
	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)alsa);
	return CHANNEL_RC_OK;
error_strdup:
	free(alsa->device_name);
error_parse_args:
	free(alsa);
	return error;
}
