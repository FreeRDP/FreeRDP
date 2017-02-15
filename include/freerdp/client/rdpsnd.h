/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012-2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_CLIENT_RDPSND_H
#define FREERDP_CHANNEL_CLIENT_RDPSND_H

#include <freerdp/channels/rdpsnd.h>

/**
 * Subsystem Interface
 */

struct _RDPSND_WAVE
{
	BYTE* data;
	int length;

	BYTE cBlockNo;
	UINT16 wFormatNo;
	UINT16 wTimeStampA;
	UINT16 wTimeStampB;

	UINT16 wAudioLength;

	UINT32 wLocalTimeA;
	UINT32 wLocalTimeB;

	BOOL AutoConfirm;
};
typedef struct _RDPSND_WAVE RDPSND_WAVE;

typedef struct rdpsnd_plugin rdpsndPlugin;

typedef struct rdpsnd_device_plugin rdpsndDevicePlugin;

typedef BOOL (*pcFormatSupported) (rdpsndDevicePlugin* device, AUDIO_FORMAT* format);
typedef BOOL (*pcOpen) (rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency);
typedef BOOL (*pcSetFormat) (rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency);
typedef UINT32 (*pcGetVolume) (rdpsndDevicePlugin* device);
typedef BOOL (*pcSetVolume) (rdpsndDevicePlugin* device, UINT32 value);
typedef void (*pcPlay) (rdpsndDevicePlugin* device, BYTE* data, int size);
typedef void (*pcStart) (rdpsndDevicePlugin* device);
typedef void (*pcClose) (rdpsndDevicePlugin* device);
typedef void (*pcFree) (rdpsndDevicePlugin* device);

typedef BOOL (*pcWaveDecode) (rdpsndDevicePlugin* device, RDPSND_WAVE* wave);
typedef void (*pcWavePlay) (rdpsndDevicePlugin* device, RDPSND_WAVE* wave);
typedef UINT (*pcWaveConfirm) (rdpsndDevicePlugin* device, RDPSND_WAVE* wave);

struct rdpsnd_device_plugin
{
	rdpsndPlugin* rdpsnd;

	pcFormatSupported FormatSupported;
	pcOpen Open;
	pcSetFormat SetFormat;
	pcGetVolume GetVolume;
	pcSetVolume SetVolume;
	pcPlay Play;
	pcStart Start;
	pcClose Close;
	pcFree Free;

	pcWaveDecode WaveDecode;
	pcWavePlay WavePlay;
	pcWaveConfirm WaveConfirm;

	BOOL DisableConfirmThread;
};

#define RDPSND_DEVICE_EXPORT_FUNC_NAME "freerdp_rdpsnd_client_subsystem_entry"

typedef void (*PREGISTERRDPSNDDEVICE)(rdpsndPlugin* rdpsnd, rdpsndDevicePlugin* device);

struct _FREERDP_RDPSND_DEVICE_ENTRY_POINTS
{
	rdpsndPlugin* rdpsnd;
	PREGISTERRDPSNDDEVICE pRegisterRdpsndDevice;
	ADDIN_ARGV* args;
};
typedef struct _FREERDP_RDPSND_DEVICE_ENTRY_POINTS FREERDP_RDPSND_DEVICE_ENTRY_POINTS;
typedef FREERDP_RDPSND_DEVICE_ENTRY_POINTS* PFREERDP_RDPSND_DEVICE_ENTRY_POINTS;

typedef UINT (*PFREERDP_RDPSND_DEVICE_ENTRY)(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints);

#endif /* FREERDP_CHANNEL_CLIENT_RDPSND_H */

