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

#include "rdpdr_main.h"
#include "devman.h"
#include "irp.h"

static void irp_free(IRP* irp)
{
	if (!irp)
		return;

	Stream_Free(irp->input, TRUE);
	Stream_Free(irp->output, TRUE);

	_aligned_free(irp);
}

static void irp_complete(IRP* irp)
{
	int pos;
	rdpdrPlugin* rdpdr;

	rdpdr = (rdpdrPlugin*) irp->devman->plugin;

	pos = (int) Stream_GetPosition(irp->output);
	Stream_SetPosition(irp->output, RDPDR_DEVICE_IO_RESPONSE_LENGTH - 4);
	Stream_Write_UINT32(irp->output, irp->IoStatus); /* IoStatus (4 bytes) */
	Stream_SetPosition(irp->output, pos);

	rdpdr_send(rdpdr, irp->output);
	irp->output = NULL;

	irp_free(irp);
}

IRP* irp_new(DEVMAN* devman, wStream* s)
{
	IRP* irp;
	DEVICE* device;
	UINT32 DeviceId;

	Stream_Read_UINT32(s, DeviceId); /* DeviceId (4 bytes) */
	device = devman_get_device_by_id(devman, DeviceId);

	if (!device)
		return NULL;

	irp = (IRP*) _aligned_malloc(sizeof(IRP), MEMORY_ALLOCATION_ALIGNMENT);
	ZeroMemory(irp, sizeof(IRP));

	irp->input = s;
	irp->device = device;
	irp->devman = devman;

	Stream_Read_UINT32(s, irp->FileId); /* FileId (4 bytes) */
	Stream_Read_UINT32(s, irp->CompletionId); /* CompletionId (4 bytes) */
	Stream_Read_UINT32(s, irp->MajorFunction); /* MajorFunction (4 bytes) */
	Stream_Read_UINT32(s, irp->MinorFunction); /* MinorFunction (4 bytes) */

	irp->output = Stream_New(NULL, 256);
	Stream_Write_UINT16(irp->output, RDPDR_CTYP_CORE); /* Component (2 bytes) */
	Stream_Write_UINT16(irp->output, PAKID_CORE_DEVICE_IOCOMPLETION); /* PacketId (2 bytes) */
	Stream_Write_UINT32(irp->output, DeviceId); /* DeviceId (4 bytes) */
	Stream_Write_UINT32(irp->output, irp->CompletionId); /* CompletionId (4 bytes) */
	Stream_Write_UINT32(irp->output, 0); /* IoStatus (4 bytes) */

	irp->Complete = irp_complete;
	irp->Discard = irp_free;

	irp->thread = NULL;
	irp->cancelled = FALSE;

	return irp;
}
