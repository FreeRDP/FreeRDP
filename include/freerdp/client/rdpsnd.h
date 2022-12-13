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
#ifdef __cplusplus
extern "C"
{
#endif

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
	typedef UINT (*pcPlayEx)(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
	                         const BYTE* data, size_t size);
	typedef void (*pcStart)(rdpsndDevicePlugin* device);
	typedef void (*pcClose)(rdpsndDevicePlugin* device);
	typedef void (*pcFree)(rdpsndDevicePlugin* device);
	typedef BOOL (*pcDefaultFormat)(rdpsndDevicePlugin* device, const AUDIO_FORMAT* desired,
	                                AUDIO_FORMAT* defaultFormat);
	typedef UINT (*pcServerFormatAnnounce)(rdpsndDevicePlugin* device, const AUDIO_FORMAT* formats,
	                                       size_t count);

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
		pcServerFormatAnnounce ServerFormatAnnounce;
		pcPlayEx PlayEx;
	};

#define RDPSND_DEVICE_EXPORT_FUNC_NAME "freerdp_rdpsnd_client_subsystem_entry"

typedef void (*PREGISTERRDPSNDDEVICE)(rdpsndPlugin* rdpsnd, rdpsndDevicePlugin* device);

typedef struct
{
	rdpsndPlugin* rdpsnd;
	PREGISTERRDPSNDDEVICE pRegisterRdpsndDevice;
	const ADDIN_ARGV* args;
} FREERDP_RDPSND_DEVICE_ENTRY_POINTS;
typedef FREERDP_RDPSND_DEVICE_ENTRY_POINTS* PFREERDP_RDPSND_DEVICE_ENTRY_POINTS;

typedef UINT (*PFREERDP_RDPSND_DEVICE_ENTRY)(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints);

FREERDP_API rdpContext* freerdp_rdpsnd_get_context(rdpsndPlugin* plugin);

#ifdef __cplusplus
}
#endif
#endif /* FREERDP_CHANNEL_RDPSND_CLIENT_RDPSND_H */
