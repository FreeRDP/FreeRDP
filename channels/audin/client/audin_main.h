/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/dvc.h>
#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/utils/debug.h>

#ifdef WITH_DEBUG_DVC
#define DEBUG_DVC(fmt, ...) DEBUG_CLASS(DVC, fmt, ## __VA_ARGS__)
#else
#define DEBUG_DVC(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

typedef BOOL (*AudinReceive) (BYTE* data, int size, void* user_data);

typedef struct audin_format audinFormat;
struct audin_format
{
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
	UINT16 cbSize;
	BYTE* data;
};

typedef struct _IAudinDevice IAudinDevice;
struct _IAudinDevice
{
	void (*Open) (IAudinDevice* devplugin, AudinReceive receive, void* user_data);
	BOOL (*FormatSupported) (IAudinDevice* devplugin, audinFormat* format);
	void (*SetFormat) (IAudinDevice* devplugin, audinFormat* format, UINT32 FramesPerPacket);
	void (*Close) (IAudinDevice* devplugin);
	void (*Free) (IAudinDevice* devplugin);
};

#define AUDIN_DEVICE_EXPORT_FUNC_NAME "freerdp_audin_client_subsystem_entry"

typedef void (*PREGISTERAUDINDEVICE)(IWTSPlugin* plugin, IAudinDevice* device);

struct _FREERDP_AUDIN_DEVICE_ENTRY_POINTS
{
	IWTSPlugin* plugin;
	PREGISTERAUDINDEVICE pRegisterAudinDevice;
	ADDIN_ARGV* args;
};
typedef struct _FREERDP_AUDIN_DEVICE_ENTRY_POINTS FREERDP_AUDIN_DEVICE_ENTRY_POINTS;
typedef FREERDP_AUDIN_DEVICE_ENTRY_POINTS* PFREERDP_AUDIN_DEVICE_ENTRY_POINTS;

typedef int (*PFREERDP_AUDIN_DEVICE_ENTRY)(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints);

#endif /* __AUDIN_MAIN_H */

