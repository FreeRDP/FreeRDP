/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MULTITRANSPORT PDUs
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
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

#include "multitransport.h"

int rdp_recv_multitransport_packet(rdpRdp* rdp, wStream* s)
{
	UINT32 requestId;
	UINT16 requestedProtocol;
	UINT16 reserved;
	BYTE securityCookie[16];

	if (Stream_GetRemainingLength(s) < 24)
		return -1;

	Stream_Read_UINT32(s, requestId);         /* requestId (4 bytes) */
	Stream_Read_UINT16(s, requestedProtocol); /* requestedProtocol (2 bytes) */
	Stream_Read_UINT16(s, reserved);          /* reserved (2 bytes) */
	Stream_Read(s, securityCookie, 16);       /* securityCookie (16 bytes) */

	return 0;
}

rdpMultitransport* multitransport_new(void)
{
	return (rdpMultitransport*)calloc(1, sizeof(rdpMultitransport));
}

void multitransport_free(rdpMultitransport* multitransport)
{
	free(multitransport);
}
