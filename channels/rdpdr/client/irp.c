/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT irp_free(IRP* irp)
{
	if (!irp)
		return CHANNEL_RC_OK;

	Stream_Free(irp->input, TRUE);
	Stream_Free(irp->output, TRUE);

	_aligned_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT irp_complete(IRP* irp)
{
	size_t pos;
	rdpdrPlugin* rdpdr;
	UINT error;

	rdpdr = (rdpdrPlugin*) irp->devman->plugin;

	pos = Stream_GetPosition(irp->output);
	Stream_SetPosition(irp->output, RDPDR_DEVICE_IO_RESPONSE_LENGTH - 4);
	Stream_Write_UINT32(irp->output, irp->IoStatus); /* IoStatus (4 bytes) */
	Stream_SetPosition(irp->output, pos);

	error = rdpdr_send(rdpdr, irp->output);
	irp->output = NULL;

	irp_free(irp);
	return error;
}

IRP* irp_new(DEVMAN* devman, wStream* s, UINT* error)
{
	IRP* irp;
	DEVICE* device;
	UINT32 DeviceId;

	if (Stream_GetRemainingLength(s) < 20)
	{
		if (error)
			*error = ERROR_INVALID_DATA;
		return NULL;
	}

	Stream_Read_UINT32(s, DeviceId); /* DeviceId (4 bytes) */
	device = devman_get_device_by_id(devman, DeviceId);

	if (!device)
	{
		WLog_WARN(TAG, "devman_get_device_by_id failed!");
		if (error)
			*error = CHANNEL_RC_OK;

		return NULL;
	};

	irp = (IRP*) _aligned_malloc(sizeof(IRP), MEMORY_ALLOCATION_ALIGNMENT);

	if (!irp)
	{
		WLog_ERR(TAG, "_aligned_malloc failed!");
		if (error)
			*error = CHANNEL_RC_NO_MEMORY;
		return NULL;
	}


	ZeroMemory(irp, sizeof(IRP));

	irp->input = s;
	irp->device = device;
	irp->devman = devman;

	Stream_Read_UINT32(s, irp->FileId); /* FileId (4 bytes) */
	Stream_Read_UINT32(s, irp->CompletionId); /* CompletionId (4 bytes) */
	Stream_Read_UINT32(s, irp->MajorFunction); /* MajorFunction (4 bytes) */
	Stream_Read_UINT32(s, irp->MinorFunction); /* MinorFunction (4 bytes) */

	irp->output = Stream_New(NULL, 256);
	if (!irp->output)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		_aligned_free(irp);
		if (error)
			*error = CHANNEL_RC_NO_MEMORY;
		return NULL;
	}
	Stream_Write_UINT16(irp->output, RDPDR_CTYP_CORE); /* Component (2 bytes) */
	Stream_Write_UINT16(irp->output, PAKID_CORE_DEVICE_IOCOMPLETION); /* PacketId (2 bytes) */
	Stream_Write_UINT32(irp->output, DeviceId); /* DeviceId (4 bytes) */
	Stream_Write_UINT32(irp->output, irp->CompletionId); /* CompletionId (4 bytes) */
	Stream_Write_UINT32(irp->output, 0); /* IoStatus (4 bytes) */

	irp->Complete = irp_complete;
	irp->Discard = irp_free;

	irp->thread = NULL;
	irp->cancelled = FALSE;

	if (error)
		*error = CHANNEL_RC_OK;

	return irp;
}
