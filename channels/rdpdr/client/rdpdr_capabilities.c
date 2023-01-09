/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015-2016 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
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
#include "rdpdr_capabilities.h"

#define RDPDR_CAPABILITY_HEADER_LENGTH 8

/* Output device direction general capability set */
static void rdpdr_write_general_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_GENERAL_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH + 36,
		                                     GENERAL_CAPABILITY_VERSION_02 };
	rdpdr_write_capset_header(rdpdr->log, s, &header);
	Stream_Write_UINT32(s, 0);                   /* osType, ignored on receipt */
	Stream_Write_UINT32(s, 0);                   /* osVersion, unused and must be set to zero */
	Stream_Write_UINT16(s, rdpdr->clientVersionMajor); /* protocolMajorVersion, must be set to 1 */
	Stream_Write_UINT16(s, rdpdr->clientVersionMinor); /* protocolMinorVersion */
	Stream_Write_UINT32(s, 0x0000FFFF);          /* ioCode1 */
	Stream_Write_UINT32(s, 0); /* ioCode2, must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, RDPDR_DEVICE_REMOVE_PDUS | RDPDR_CLIENT_DISPLAY_NAME_PDU |
	                           RDPDR_USER_LOGGEDON_PDU); /* extendedPDU */
	Stream_Write_UINT32(s, ENABLE_ASYNCIO);              /* extraFlags1 */
	Stream_Write_UINT32(s, 0); /* extraFlags2, must be set to zero, reserved for future use */
	Stream_Write_UINT32(
	    s, 0); /* SpecialTypeDeviceCap, number of special devices to be redirected before logon */
}

/* Process device direction general capability set */
static UINT rdpdr_process_general_capset(rdpdrPlugin* rdpdr, wStream* s,
                                         const RDPDR_CAPABILITY_HEADER* header)
{

	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

/* Output printer direction capability set */
static void rdpdr_write_printer_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_PRINTER_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     PRINT_CAPABILITY_VERSION_01 };
	rdpdr_write_capset_header(rdpdr->log, s, &header);
}

/* Process printer direction capability set */
static UINT rdpdr_process_printer_capset(rdpdrPlugin* rdpdr, wStream* s,
                                         const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

/* Output port redirection capability set */
static void rdpdr_write_port_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_PORT_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     PORT_CAPABILITY_VERSION_01 };
	rdpdr_write_capset_header(rdpdr->log, s, &header);
}

/* Process port redirection capability set */
static UINT rdpdr_process_port_capset(rdpdrPlugin* rdpdr, wStream* s,
                                      const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

/* Output drive redirection capability set */
static void rdpdr_write_drive_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_DRIVE_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     DRIVE_CAPABILITY_VERSION_02 };
	rdpdr_write_capset_header(rdpdr->log, s, &header);
}

/* Process drive redirection capability set */
static UINT rdpdr_process_drive_capset(rdpdrPlugin* rdpdr, wStream* s,
                                       const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

/* Output smart card redirection capability set */
static void rdpdr_write_smartcard_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_SMARTCARD_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     SMARTCARD_CAPABILITY_VERSION_01 };
	rdpdr_write_capset_header(rdpdr->log, s, &header);
}

/* Process smartcard redirection capability set */
static UINT rdpdr_process_smartcard_capset(rdpdrPlugin* rdpdr, wStream* s,
                                           const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

UINT rdpdr_process_capability_request(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT status = CHANNEL_RC_OK;
	UINT16 i;
	UINT16 numCapabilities;

	if (!rdpdr || !s)
		return CHANNEL_RC_NULL_DATA;

	if (!Stream_CheckAndLogRequiredLengthWLog(rdpdr->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, numCapabilities);
	Stream_Seek(s, 2); /* pad (2 bytes) */

	for (i = 0; i < numCapabilities; i++)
	{
		RDPDR_CAPABILITY_HEADER header = { 0 };
		UINT error = rdpdr_read_capset_header(rdpdr->log, s, &header);
		if (error != CHANNEL_RC_OK)
			return error;

		switch (header.CapabilityType)
		{
			case CAP_GENERAL_TYPE:
				status = rdpdr_process_general_capset(rdpdr, s, &header);
				break;

			case CAP_PRINTER_TYPE:
				status = rdpdr_process_printer_capset(rdpdr, s, &header);
				break;

			case CAP_PORT_TYPE:
				status = rdpdr_process_port_capset(rdpdr, s, &header);
				break;

			case CAP_DRIVE_TYPE:
				status = rdpdr_process_drive_capset(rdpdr, s, &header);
				break;

			case CAP_SMARTCARD_TYPE:
				status = rdpdr_process_smartcard_capset(rdpdr, s, &header);
				break;

			default:

				break;
		}

		if (status != CHANNEL_RC_OK)
			return status;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpdr_send_capability_response(rdpdrPlugin* rdpdr)
{
	wStream* s;

	WINPR_ASSERT(rdpdr);
	s = StreamPool_Take(rdpdr->pool, 256);

	if (!s)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);
	Stream_Write_UINT16(s, PAKID_CORE_CLIENT_CAPABILITY);
	Stream_Write_UINT16(s, 5); /* numCapabilities */
	Stream_Write_UINT16(s, 0); /* pad */
	rdpdr_write_general_capset(rdpdr, s);
	rdpdr_write_printer_capset(rdpdr, s);
	rdpdr_write_port_capset(rdpdr, s);
	rdpdr_write_drive_capset(rdpdr, s);
	rdpdr_write_smartcard_capset(rdpdr, s);
	return rdpdr_send(rdpdr, s);
}
