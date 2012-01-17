/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Audio Input Redirection Virtual Channel
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

#ifndef __AUDIN_MAIN_H
#define __AUDIN_MAIN_H

#include "drdynvc_types.h"

typedef boolean (*AudinReceive) (uint8* data, int size, void* user_data);

typedef struct audin_format audinFormat;
struct audin_format
{
	uint16 wFormatTag;
	uint16 nChannels;
	uint32 nSamplesPerSec;
	uint16 nBlockAlign;
	uint16 wBitsPerSample;
	uint16 cbSize;
	uint8* data;
};

typedef struct _IAudinDevice IAudinDevice;
struct _IAudinDevice
{
	void (*Open) (IAudinDevice* devplugin, AudinReceive receive, void* user_data);
	boolean (*FormatSupported) (IAudinDevice* devplugin, audinFormat* format);
	void (*SetFormat) (IAudinDevice* devplugin, audinFormat* format, uint32 FramesPerPacket);
	void (*Close) (IAudinDevice* devplugin);
	void (*Free) (IAudinDevice* devplugin);
};

#define AUDIN_DEVICE_EXPORT_FUNC_NAME "FreeRDPAudinDeviceEntry"

typedef void (*PREGISTERAUDINDEVICE)(IWTSPlugin* plugin, IAudinDevice* device);

struct _FREERDP_AUDIN_DEVICE_ENTRY_POINTS
{
	IWTSPlugin* plugin;
	PREGISTERAUDINDEVICE pRegisterAudinDevice;
	RDP_PLUGIN_DATA* plugin_data;
};
typedef struct _FREERDP_AUDIN_DEVICE_ENTRY_POINTS FREERDP_AUDIN_DEVICE_ENTRY_POINTS;
typedef FREERDP_AUDIN_DEVICE_ENTRY_POINTS* PFREERDP_AUDIN_DEVICE_ENTRY_POINTS;

typedef int (*PFREERDP_AUDIN_DEVICE_ENTRY)(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints);

#endif /* __AUDIN_MAIN_H */

