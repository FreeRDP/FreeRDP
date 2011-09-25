/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Audio Output Virtual Channel
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

#ifndef __RDPSND_MAIN_H
#define __RDPSND_MAIN_H

typedef struct rdpsnd_plugin rdpsndPlugin;

typedef struct rdpsnd_format rdpsndFormat;
struct rdpsnd_format
{
	uint16 wFormatTag;
	uint16 nChannels;
	uint32 nSamplesPerSec;
	uint16 nBlockAlign;
	uint16 wBitsPerSample;
	uint16 cbSize;
	uint8* data;
};

typedef struct rdpsnd_device_plugin rdpsndDevicePlugin;

typedef boolean (*pcFormatSupported) (rdpsndDevicePlugin* device, rdpsndFormat* format);
typedef void (*pcOpen) (rdpsndDevicePlugin* device, rdpsndFormat* format, int latency);
typedef void (*pcSetFormat) (rdpsndDevicePlugin* device, rdpsndFormat* format, int latency);
typedef void (*pcSetVolume) (rdpsndDevicePlugin* device, uint32 value);
typedef void (*pcPlay) (rdpsndDevicePlugin* device, uint8* data, int size);
typedef void (*pcStart) (rdpsndDevicePlugin* device);
typedef void (*pcClose) (rdpsndDevicePlugin* device);
typedef void (*pcFree) (rdpsndDevicePlugin* device);

struct rdpsnd_device_plugin
{
	pcFormatSupported FormatSupported;
	pcOpen Open;
	pcSetFormat SetFormat;
	pcSetVolume SetVolume;
	pcPlay Play;
	pcStart Start;
	pcClose Close;
	pcFree Free;
};

#define RDPSND_DEVICE_EXPORT_FUNC_NAME "FreeRDPRdpsndDeviceEntry"

typedef void (*PREGISTERRDPSNDDEVICE)(rdpsndPlugin* rdpsnd, rdpsndDevicePlugin* device);

struct _FREERDP_RDPSND_DEVICE_ENTRY_POINTS
{
	rdpsndPlugin* rdpsnd;
	PREGISTERRDPSNDDEVICE pRegisterRdpsndDevice;
	RDP_PLUGIN_DATA* plugin_data;
};
typedef struct _FREERDP_RDPSND_DEVICE_ENTRY_POINTS FREERDP_RDPSND_DEVICE_ENTRY_POINTS;
typedef FREERDP_RDPSND_DEVICE_ENTRY_POINTS* PFREERDP_RDPSND_DEVICE_ENTRY_POINTS;

typedef int (*PFREERDP_RDPSND_DEVICE_ENTRY)(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints);

#endif /* __RDPSND_MAIN_H */

