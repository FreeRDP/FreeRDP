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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/utils/rdpdr_utils.h>

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

	if (irp->input)
		Stream_Release(irp->input);
	if (irp->output)
		Stream_Release(irp->output);

	winpr_aligned_free(irp);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT irp_complete(IRP* irp)
{
	WINPR_ASSERT(irp);
	WINPR_ASSERT(irp->output);
	WINPR_ASSERT(irp->devman);

	rdpdrPlugin* rdpdr = (rdpdrPlugin*)irp->devman->plugin;
	WINPR_ASSERT(rdpdr);

	const size_t pos = Stream_GetPosition(irp->output);
	Stream_SetPosition(irp->output, RDPDR_DEVICE_IO_RESPONSE_LENGTH - 4);
	Stream_Write_INT32(irp->output, irp->IoStatus); /* IoStatus (4 bytes) */
	Stream_SetPosition(irp->output, pos);

	const UINT error = rdpdr_send(rdpdr, irp->output);
	irp->output = nullptr;

	irp_free(irp);
	return error;
}

IRP* irp_new(DEVMAN* devman, wStreamPool* pool, wStream* s, wLog* log, UINT* error)
{
	IRP* irp = nullptr;
	DEVICE* device = nullptr;
	UINT32 DeviceId = 0;

	WINPR_ASSERT(devman);
	WINPR_ASSERT(pool);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 20))
	{
		if (error)
			*error = ERROR_INVALID_DATA;
		return nullptr;
	}

	Stream_Read_UINT32(s, DeviceId); /* DeviceId (4 bytes) */
	device = devman_get_device_by_id(devman, DeviceId);

	if (!device)
	{
		if (error)
			*error = ERROR_DEV_NOT_EXIST;

		return nullptr;
	}

	irp = (IRP*)winpr_aligned_calloc(1, sizeof(IRP), MEMORY_ALLOCATION_ALIGNMENT);

	if (!irp)
	{
		WLog_Print(log, WLOG_ERROR, "_aligned_malloc failed!");
		if (error)
			*error = CHANNEL_RC_NO_MEMORY;
		return nullptr;
	}

	Stream_Read_UINT32(s, irp->FileId);        /* FileId (4 bytes) */
	Stream_Read_UINT32(s, irp->CompletionId);  /* CompletionId (4 bytes) */
	Stream_Read_UINT32(s, irp->MajorFunction); /* MajorFunction (4 bytes) */
	Stream_Read_UINT32(s, irp->MinorFunction); /* MinorFunction (4 bytes) */

	Stream_AddRef(s);
	irp->input = s;
	irp->device = device;
	irp->devman = devman;

	irp->output = StreamPool_Take(pool, 256);
	if (!irp->output)
	{
		WLog_Print(log, WLOG_ERROR, "Stream_New failed!");
		irp_free(irp);
		if (error)
			*error = CHANNEL_RC_NO_MEMORY;
		return nullptr;
	}

	if (!rdpdr_write_iocompletion_header(irp->output, DeviceId, irp->CompletionId, 0))
	{
		irp_free(irp);
		if (error)
			*error = CHANNEL_RC_NO_MEMORY;
		return nullptr;
	}

	irp->Complete = irp_complete;
	irp->Discard = irp_free;

	irp->thread = nullptr;
	irp->cancelled = FALSE;

	if (error)
		*error = CHANNEL_RC_OK;

	return irp;
}
