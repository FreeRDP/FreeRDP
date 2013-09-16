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

#include <winpr/stream.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_main.h"
#include "rdpdr_capabilities.h"

/* Output device redirection capability set header */
static void rdpdr_write_capset_header(wStream* s, UINT16 capabilityType, UINT16 capabilityLength, UINT32 version)
{
	Stream_Write_UINT16(s, capabilityType);
	Stream_Write_UINT16(s, capabilityLength);
	Stream_Write_UINT32(s, version);
}

/* Output device direction general capability set */
static void rdpdr_write_general_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	rdpdr_write_capset_header(s, CAP_GENERAL_TYPE, 44, GENERAL_CAPABILITY_VERSION_02);

	Stream_Write_UINT32(s, 0); /* osType, ignored on receipt */
	Stream_Write_UINT32(s, 0); /* osVersion, unused and must be set to zero */
	Stream_Write_UINT16(s, 1); /* protocolMajorVersion, must be set to 1 */
	Stream_Write_UINT16(s, RDPDR_MINOR_RDP_VERSION_5_2); /* protocolMinorVersion */
	Stream_Write_UINT32(s, 0x0000FFFF); /* ioCode1 */
	Stream_Write_UINT32(s, 0); /* ioCode2, must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, RDPDR_DEVICE_REMOVE_PDUS | RDPDR_CLIENT_DISPLAY_NAME_PDU | RDPDR_USER_LOGGEDON_PDU); /* extendedPDU */
	Stream_Write_UINT32(s, ENABLE_ASYNCIO); /* extraFlags1 */
	Stream_Write_UINT32(s, 0); /* extraFlags2, must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, 0); /* SpecialTypeDeviceCap, number of special devices to be redirected before logon */
}

/* Process device direction general capability set */
static void rdpdr_process_general_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 capabilityLength;

	Stream_Read_UINT16(s, capabilityLength);
	Stream_Seek(s, capabilityLength - 4);
}

/* Output printer direction capability set */
static void rdpdr_write_printer_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	rdpdr_write_capset_header(s, CAP_PRINTER_TYPE, 8, PRINT_CAPABILITY_VERSION_01);
}

/* Process printer direction capability set */
static void rdpdr_process_printer_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 capabilityLength;

	Stream_Read_UINT16(s, capabilityLength);
	Stream_Seek(s, capabilityLength - 4);
}

/* Output port redirection capability set */
static void rdpdr_write_port_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	rdpdr_write_capset_header(s, CAP_PORT_TYPE, 8, PORT_CAPABILITY_VERSION_01);
}

/* Process port redirection capability set */
static void rdpdr_process_port_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 capabilityLength;

	Stream_Read_UINT16(s, capabilityLength);
	Stream_Seek(s, capabilityLength - 4);
}

/* Output drive redirection capability set */
static void rdpdr_write_drive_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	rdpdr_write_capset_header(s, CAP_DRIVE_TYPE, 8, DRIVE_CAPABILITY_VERSION_02);
}

/* Process drive redirection capability set */
static void rdpdr_process_drive_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 capabilityLength;

	Stream_Read_UINT16(s, capabilityLength);
	Stream_Seek(s, capabilityLength - 4);
}

/* Output smart card redirection capability set */
static void rdpdr_write_smartcard_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	rdpdr_write_capset_header(s, CAP_SMARTCARD_TYPE, 8, SMARTCARD_CAPABILITY_VERSION_01);
}

/* Process smartcard redirection capability set */
static void rdpdr_process_smartcard_capset(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 capabilityLength;

	Stream_Read_UINT16(s, capabilityLength);
	Stream_Seek(s, capabilityLength - 4);
}

void rdpdr_process_capability_request(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 i;
	UINT16 numCapabilities;
	UINT16 capabilityType;

	Stream_Read_UINT16(s, numCapabilities);
	Stream_Seek(s, 2); /* pad (2 bytes) */

	for (i = 0; i < numCapabilities; i++)
	{
		Stream_Read_UINT16(s, capabilityType);

		switch (capabilityType)
		{
			case CAP_GENERAL_TYPE:
				rdpdr_process_general_capset(rdpdr, s);
				break;

			case CAP_PRINTER_TYPE:
				rdpdr_process_printer_capset(rdpdr, s);
				break;

			case CAP_PORT_TYPE:
				rdpdr_process_port_capset(rdpdr, s);
				break;

			case CAP_DRIVE_TYPE:
				rdpdr_process_drive_capset(rdpdr, s);
				break;

			case CAP_SMARTCARD_TYPE:
				rdpdr_process_smartcard_capset(rdpdr, s);
				break;

			default:
				DEBUG_WARN("Unknown capabilityType %d", capabilityType);
				break;
		}
	}
}

void rdpdr_send_capability_response(rdpdrPlugin* rdpdr)
{
	wStream* s;

	s = Stream_New(NULL, 256);

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);
	Stream_Write_UINT16(s, PAKID_CORE_CLIENT_CAPABILITY);

	Stream_Write_UINT16(s, 5); /* numCapabilities */
	Stream_Write_UINT16(s, 0); /* pad */

	rdpdr_write_general_capset(rdpdr, s);
	rdpdr_write_printer_capset(rdpdr, s);
	rdpdr_write_port_capset(rdpdr, s);
	rdpdr_write_drive_capset(rdpdr, s);
	rdpdr_write_smartcard_capset(rdpdr, s);

	svc_plugin_send((rdpSvcPlugin*)rdpdr, s);
}
