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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_types.h"
#include "rdpdr_constants.h"
#include "devman.h"
#include "irp.h"

static void irp_free(IRP* irp)
{
	DEBUG_SVC("DeviceId %d FileId %d CompletionId %d", irp->device->id, irp->FileId, irp->CompletionId);

	stream_free(irp->input);
	stream_free(irp->output);
	xfree(irp);
}

static void irp_complete(IRP* irp)
{
	int pos;

	DEBUG_SVC("DeviceId %d FileId %d CompletionId %d", irp->device->id, irp->FileId, irp->CompletionId);

	pos = stream_get_pos(irp->output);
	stream_set_pos(irp->output, 12);
	stream_write_uint32(irp->output, irp->IoStatus);
	stream_set_pos(irp->output, pos);

	svc_plugin_send(irp->devman->plugin, irp->output);
	irp->output = NULL;

	irp_free(irp);
}

IRP* irp_new(DEVMAN* devman, STREAM* data_in)
{
	IRP* irp;
	uint32 DeviceId;
	DEVICE* device;

	stream_read_uint32(data_in, DeviceId);
	device = devman_get_device_by_id(devman, DeviceId);
	if (device == NULL)
	{
		DEBUG_WARN("unknown DeviceId %d", DeviceId);
		return NULL;
	}

	irp = xnew(IRP);
	irp->device = device;
	irp->devman = devman;
	stream_read_uint32(data_in, irp->FileId);
	stream_read_uint32(data_in, irp->CompletionId);
	stream_read_uint32(data_in, irp->MajorFunction);
	stream_read_uint32(data_in, irp->MinorFunction);
	irp->input = data_in;

	irp->output = stream_new(256);
	stream_write_uint16(irp->output, RDPDR_CTYP_CORE);
	stream_write_uint16(irp->output, PAKID_CORE_DEVICE_IOCOMPLETION);
	stream_write_uint32(irp->output, DeviceId);
	stream_write_uint32(irp->output, irp->CompletionId);
	stream_seek_uint32(irp->output); /* IoStatus */

	irp->Complete = irp_complete;
	irp->Discard = irp_free;

	DEBUG_SVC("DeviceId %d FileId %d CompletionId %d MajorFunction 0x%X MinorFunction 0x%x",
		irp->device->id, irp->FileId, irp->CompletionId, irp->MajorFunction, irp->MinorFunction);

	return irp;
}
