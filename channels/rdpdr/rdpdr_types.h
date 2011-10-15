/**
 * FreeRDP: A Remote Desktop Protocol client.
 * File System Virtual Channel
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __RDPDR_TYPES_H
#define __RDPDR_TYPES_H

#include "config.h"
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/svc_plugin.h>

typedef struct _DEVICE DEVICE;
typedef struct _IRP IRP;
typedef struct _DEVMAN DEVMAN;


typedef void (*pcIRPRequest)(DEVICE* device, IRP* irp);
typedef void (*pcFreeDevice)(DEVICE* device);

struct _DEVICE
{
	uint32 id;

	uint32 type;
	char* name;
	STREAM* data;

	pcIRPRequest IRPRequest;
	pcFreeDevice Free;
};

typedef void (*pcIRPResponse)(IRP* irp);

struct _IRP
{
	DEVICE* device;
	DEVMAN* devman;
	uint32 FileId;
	uint32 CompletionId;
	uint32 MajorFunction;
	uint32 MinorFunction;
	STREAM* input;

	uint32 IoStatus;
	STREAM* output;

	pcIRPResponse Complete;
	pcIRPResponse Discard;
};

struct _DEVMAN
{
	rdpSvcPlugin* plugin;
	uint32 id_sequence; /* generate unique device id */
	LIST* devices;
};

typedef void (*pcRegisterDevice)(DEVMAN* devman, DEVICE* device);

struct _DEVICE_SERVICE_ENTRY_POINTS
{
	DEVMAN* devman;

	pcRegisterDevice RegisterDevice;
	pcRegisterDevice UnregisterDevice;
	RDP_PLUGIN_DATA* plugin_data;
};
typedef struct _DEVICE_SERVICE_ENTRY_POINTS DEVICE_SERVICE_ENTRY_POINTS;
typedef DEVICE_SERVICE_ENTRY_POINTS* PDEVICE_SERVICE_ENTRY_POINTS;

typedef int (*PDEVICE_SERVICE_ENTRY)(PDEVICE_SERVICE_ENTRY_POINTS);

#endif /* __RDPDR_TYPES_H */
