/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>

#include <winpr/stream.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_main.h"
#include "devman.h"
#include "irp.h"

static void irp_free(IRP* irp)
{
	DEBUG_SVC("DeviceId %d FileId %d CompletionId %d", irp->device->id, irp->FileId, irp->CompletionId);

	Stream_Free(irp->input, TRUE);
	Stream_Free(irp->output, TRUE);

	_aligned_free(irp);
}

static void irp_complete(IRP* irp)
{
	int pos;

	DEBUG_SVC("DeviceId %d FileId %d CompletionId %d", irp->device->id, irp->FileId, irp->CompletionId);

	pos = Stream_GetPosition(irp->output);
	Stream_SetPosition(irp->output, 12);
	Stream_Write_UINT32(irp->output, irp->IoStatus);
	Stream_SetPosition(irp->output, pos);

	svc_plugin_send(irp->devman->plugin, irp->output);
	irp->output = NULL;

	irp_free(irp);
}

IRP* irp_new(DEVMAN* devman, wStream* data_in)
{
	IRP* irp;
	UINT32 DeviceId;
	DEVICE* device;

	Stream_Read_UINT32(data_in, DeviceId);
	device = devman_get_device_by_id(devman, DeviceId);

	if (device == NULL)
	{
		DEBUG_WARN("unknown DeviceId %d", DeviceId);
		return NULL;
	}

	irp = (IRP*) _aligned_malloc(sizeof(IRP), MEMORY_ALLOCATION_ALIGNMENT);
	ZeroMemory(irp, sizeof(IRP));

	irp->device = device;
	irp->devman = devman;
	Stream_Read_UINT32(data_in, irp->FileId);
	Stream_Read_UINT32(data_in, irp->CompletionId);
	Stream_Read_UINT32(data_in, irp->MajorFunction);
	Stream_Read_UINT32(data_in, irp->MinorFunction);
	irp->input = data_in;

	irp->output = Stream_New(NULL, 256);
	Stream_Write_UINT16(irp->output, RDPDR_CTYP_CORE);
	Stream_Write_UINT16(irp->output, PAKID_CORE_DEVICE_IOCOMPLETION);
	Stream_Write_UINT32(irp->output, DeviceId);
	Stream_Write_UINT32(irp->output, irp->CompletionId);
	Stream_Seek_UINT32(irp->output); /* IoStatus */

	irp->Complete = irp_complete;
	irp->Discard = irp_free;

	DEBUG_SVC("DeviceId %d FileId %d CompletionId %d MajorFunction 0x%X MinorFunction 0x%x",
		irp->device->id, irp->FileId, irp->CompletionId, irp->MajorFunction, irp->MinorFunction);

	return irp;
}
