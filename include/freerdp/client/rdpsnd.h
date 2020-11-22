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

#ifndef FREERDP_CHANNEL_RDPSND_CLIENT_RDPSND_H
#define FREERDP_CHANNEL_RDPSND_CLIENT_RDPSND_H

#include <freerdp/channels/rdpsnd.h>

#define RDPSND_DVC_CHANNEL_NAME "AUDIO_PLAYBACK_DVC"

/**
 * Subsystem Interface
 */
typedef struct rdpsnd_plugin rdpsndPlugin;

typedef struct rdpsnd_device_plugin rdpsndDevicePlugin;

typedef BOOL (*pcFormatSupported)(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format);
typedef BOOL (*pcOpen)(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format, UINT32 latency);
typedef UINT32 (*pcGetVolume)(rdpsndDevicePlugin* device);
typedef BOOL (*pcSetVolume)(rdpsndDevicePlugin* device, UINT32 value);
typedef UINT (*pcPlay)(rdpsndDevicePlugin* device, const BYTE* data, size_t size);
typedef void (*pcStart)(rdpsndDevicePlugin* device);
typedef void (*pcClose)(rdpsndDevicePlugin* device);
typedef void (*pcFree)(rdpsndDevicePlugin* device);
typedef BOOL (*pcDefaultFormat)(rdpsndDevicePlugin* device, const AUDIO_FORMAT* desired,
                                AUDIO_FORMAT* defaultFormat);

struct rdpsnd_device_plugin
{
	rdpsndPlugin* rdpsnd;

	pcFormatSupported FormatSupported;
	pcOpen Open;
	pcGetVolume GetVolume;
	pcSetVolume SetVolume;
	pcPlay Play;
	pcStart Start; /* Deprecated, unused. */
	pcClose Close;
	pcFree Free;
	pcDefaultFormat DefaultFormat;
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

#endif /* FREERDP_CHANNEL_RDPSND_CLIENT_RDPSND_H */
