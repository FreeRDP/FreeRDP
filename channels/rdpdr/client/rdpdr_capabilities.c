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

#include <freerdp/freerdp.h>
#include <freerdp/utils/rdpdr_utils.h>

#include "rdpdr_main.h"
#include "rdpdr_capabilities.h"

#define RDPDR_CAPABILITY_HEADER_LENGTH 8

/* Output device direction general capability set */
static BOOL rdpdr_write_general_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_ASSERT(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_GENERAL_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH + 36,
		                                     GENERAL_CAPABILITY_VERSION_02 };

	const UINT32 ioCode1 = rdpdr->clientIOCode1 & rdpdr->serverIOCode1;
	const UINT32 ioCode2 = rdpdr->clientIOCode2 & rdpdr->serverIOCode2;

	if (rdpdr_write_capset_header(rdpdr->log, s, &header) != CHANNEL_RC_OK)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 36))
		return FALSE;

	Stream_Write_UINT32(s, rdpdr->clientOsType);    /* osType, ignored on receipt */
	Stream_Write_UINT32(s, rdpdr->clientOsVersion); /* osVersion, unused and must be set to zero */
	Stream_Write_UINT16(s, rdpdr->clientVersionMajor); /* protocolMajorVersion, must be set to 1 */
	Stream_Write_UINT16(s, rdpdr->clientVersionMinor); /* protocolMinorVersion */
	Stream_Write_UINT32(s, ioCode1);                   /* ioCode1 */
	Stream_Write_UINT32(s, ioCode2); /* ioCode2, must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, rdpdr->clientExtendedPDU); /* extendedPDU */
	Stream_Write_UINT32(s, rdpdr->clientExtraFlags1); /* extraFlags1 */
	Stream_Write_UINT32(
	    s,
	    rdpdr->clientExtraFlags2); /* extraFlags2, must be set to zero, reserved for future use */
	Stream_Write_UINT32(
	    s, rdpdr->clientSpecialTypeDeviceCap); /* SpecialTypeDeviceCap, number of special devices to
	                                              be redirected before logon */
	return TRUE;
}

/* Process device direction general capability set */
static UINT rdpdr_process_general_capset(rdpdrPlugin* rdpdr, wStream* s,
                                         const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);

	const BOOL gotV1 = header->Version == GENERAL_CAPABILITY_VERSION_01;
	const size_t expect = gotV1 ? 32 : 36;
	if (header->CapabilityLength != expect)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR,
		           "CAP_GENERAL_TYPE::CapabilityLength expected %" PRIuz ", got %" PRIu32, expect,
		           header->CapabilityLength);
		return ERROR_INVALID_DATA;
	}
	if (!Stream_CheckAndLogRequiredLengthWLog(rdpdr->log, s, expect))
		return ERROR_INVALID_DATA;

	rdpdr->serverOsType = Stream_Get_UINT32(s);    /* osType, ignored on receipt */
	rdpdr->serverOsVersion = Stream_Get_UINT32(s); /* osVersion, unused and must be set to zero */
	rdpdr->serverVersionMajor = Stream_Get_UINT16(s); /* protocolMajorVersion, must be set to 1 */
	rdpdr->serverVersionMinor = Stream_Get_UINT16(s); /* protocolMinorVersion */
	rdpdr->serverIOCode1 = Stream_Get_UINT32(s);      /* ioCode1 */
	rdpdr->serverIOCode2 =
	    Stream_Get_UINT32(s); /* ioCode2, must be set to zero, reserved for future use */
	rdpdr->serverExtendedPDU = Stream_Get_UINT32(s); /* extendedPDU */
	rdpdr->serverExtraFlags1 = Stream_Get_UINT32(s); /* extraFlags1 */
	rdpdr->serverExtraFlags2 =
	    Stream_Get_UINT32(s); /* extraFlags2, must be set to zero, reserved for future use */
	if (gotV1)
		rdpdr->serverSpecialTypeDeviceCap = 0;
	else
		rdpdr->serverSpecialTypeDeviceCap = Stream_Get_UINT32(
		    s); /* SpecialTypeDeviceCap, number of special devices to
		                                              be redirected before logon */
	return CHANNEL_RC_OK;
}

/* Output printer direction capability set */
static BOOL rdpdr_write_printer_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_PRINTER_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     PRINT_CAPABILITY_VERSION_01 };
	return rdpdr_write_capset_header(rdpdr->log, s, &header) == CHANNEL_RC_OK;
}

/* Process printer direction capability set */
static UINT rdpdr_process_printer_capset(WINPR_ATTR_UNUSED rdpdrPlugin* rdpdr, wStream* s,
                                         const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

/* Output port redirection capability set */
static BOOL rdpdr_write_port_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_PORT_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     PORT_CAPABILITY_VERSION_01 };
	return rdpdr_write_capset_header(rdpdr->log, s, &header) == CHANNEL_RC_OK;
}

/* Process port redirection capability set */
static UINT rdpdr_process_port_capset(WINPR_ATTR_UNUSED rdpdrPlugin* rdpdr, wStream* s,
                                      const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

/* Output drive redirection capability set */
static BOOL rdpdr_write_drive_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_DRIVE_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     DRIVE_CAPABILITY_VERSION_02 };
	return rdpdr_write_capset_header(rdpdr->log, s, &header) == CHANNEL_RC_OK;
}

/* Process drive redirection capability set */
static UINT rdpdr_process_drive_capset(WINPR_ATTR_UNUSED rdpdrPlugin* rdpdr, wStream* s,
                                       const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

/* Output smart card redirection capability set */
static BOOL rdpdr_write_smartcard_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_UNUSED(rdpdr);
	const RDPDR_CAPABILITY_HEADER header = { CAP_SMARTCARD_TYPE, RDPDR_CAPABILITY_HEADER_LENGTH,
		                                     SMARTCARD_CAPABILITY_VERSION_01 };
	return rdpdr_write_capset_header(rdpdr->log, s, &header) == CHANNEL_RC_OK;
}

/* Process smartcard redirection capability set */
static UINT rdpdr_process_smartcard_capset(WINPR_ATTR_UNUSED rdpdrPlugin* rdpdr, wStream* s,
                                           const RDPDR_CAPABILITY_HEADER* header)
{
	WINPR_ASSERT(header);
	Stream_Seek(s, header->CapabilityLength);
	return CHANNEL_RC_OK;
}

UINT rdpdr_process_capability_request(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT status = CHANNEL_RC_OK;
	UINT16 numCapabilities = 0;

	if (!rdpdr || !s)
		return CHANNEL_RC_NULL_DATA;

	rdpdr_state_advance(rdpdr, RDPDR_CHANNEL_STATE_CLIENT_CAPS);

	if (!Stream_CheckAndLogRequiredLengthWLog(rdpdr->log, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, numCapabilities);
	Stream_Seek(s, 2); /* pad (2 bytes) */

	memset(rdpdr->capabilities, 0, sizeof(rdpdr->capabilities));
	for (UINT16 i = 0; i < numCapabilities; i++)
	{
		RDPDR_CAPABILITY_HEADER header = { 0 };
		UINT error = rdpdr_read_capset_header(rdpdr->log, s, &header);
		if (error != CHANNEL_RC_OK)
			return error;

		switch (header.CapabilityType)
		{
			case CAP_GENERAL_TYPE:
				rdpdr->capabilities[header.CapabilityType] = TRUE;
				status = rdpdr_process_general_capset(rdpdr, s, &header);
				break;

			case CAP_PRINTER_TYPE:
				rdpdr->capabilities[header.CapabilityType] = TRUE;
				status = rdpdr_process_printer_capset(rdpdr, s, &header);
				break;

			case CAP_PORT_TYPE:
				rdpdr->capabilities[header.CapabilityType] = TRUE;
				status = rdpdr_process_port_capset(rdpdr, s, &header);
				break;

			case CAP_DRIVE_TYPE:
				rdpdr->capabilities[header.CapabilityType] = TRUE;
				status = rdpdr_process_drive_capset(rdpdr, s, &header);
				break;

			case CAP_SMARTCARD_TYPE:
				rdpdr->capabilities[header.CapabilityType] = TRUE;
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
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(rdpdr->rdpcontext);

	rdpSettings* settings = rdpdr->rdpcontext->settings;
	WINPR_ASSERT(settings);

	wStream* s = StreamPool_Take(rdpdr->pool, 256);

	if (!s)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	const RDPDR_DEVICE* cdrives =
	    freerdp_device_collection_find_type(settings, RDPDR_DTYP_FILESYSTEM);
	const RDPDR_DEVICE* cserial = freerdp_device_collection_find_type(settings, RDPDR_DTYP_SERIAL);
	const RDPDR_DEVICE* cparallel =
	    freerdp_device_collection_find_type(settings, RDPDR_DTYP_PARALLEL);
	const RDPDR_DEVICE* csmart =
	    freerdp_device_collection_find_type(settings, RDPDR_DTYP_SMARTCARD);
	const RDPDR_DEVICE* cprinter = freerdp_device_collection_find_type(settings, RDPDR_DTYP_PRINT);

	/* Only send capabilities the server announced */
	const BOOL drives = cdrives && rdpdr->capabilities[CAP_DRIVE_TYPE];
	const BOOL serial = cserial && rdpdr->capabilities[CAP_PORT_TYPE];
	const BOOL parallel = cparallel && rdpdr->capabilities[CAP_PORT_TYPE];
	const BOOL smart = csmart && rdpdr->capabilities[CAP_SMARTCARD_TYPE];
	const BOOL printer = cprinter && rdpdr->capabilities[CAP_PRINTER_TYPE];
	UINT16 count = 1;
	if (drives)
		count++;
	if (serial || parallel)
		count++;
	if (smart)
		count++;
	if (printer)
		count++;

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);
	Stream_Write_UINT16(s, PAKID_CORE_CLIENT_CAPABILITY);
	Stream_Write_UINT16(s, count); /* numCapabilities */
	Stream_Write_UINT16(s, 0); /* pad */

	if (!rdpdr_write_general_capset(rdpdr, s))
		goto fail;

	if (printer)
	{
		if (!rdpdr_write_printer_capset(rdpdr, s))
			goto fail;
	}
	if (serial || parallel)
	{
		if (!rdpdr_write_port_capset(rdpdr, s))
			goto fail;
	}
	if (drives)
	{
		if (!rdpdr_write_drive_capset(rdpdr, s))
			goto fail;
	}
	if (smart)
	{
		if (!rdpdr_write_smartcard_capset(rdpdr, s))
			goto fail;
	}
	return rdpdr_send(rdpdr, s);

fail:
	Stream_Release(s);
	return ERROR_OUTOFMEMORY;
}
