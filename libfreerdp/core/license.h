/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Licensing
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_CORE_LICENSE_H
#define FREERDP_LIB_CORE_LICENSE_H

#include "rdp.h"
#include "state.h"

#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/certificate.h>

#include <freerdp/log.h>
#include <freerdp/license.h>

#include <winpr/stream.h>

#define CLIENT_RANDOM_LENGTH 32

typedef struct
{
	UINT32 dwVersion;
	UINT32 cbCompanyName;
	BYTE* pbCompanyName;
	UINT32 cbProductId;
	BYTE* pbProductId;
} LICENSE_PRODUCT_INFO;

typedef struct
{
	UINT16 type;
	UINT16 length;
	BYTE* data;
} LICENSE_BLOB;

typedef struct
{
	UINT32 count;
	LICENSE_BLOB** array;
} SCOPE_LIST;

FREERDP_LOCAL BOOL license_send_valid_client_error_packet(rdpRdp* rdp);

FREERDP_LOCAL state_run_t license_recv(rdpLicense* license, wStream* s);

/* the configuration is applied from settings. Set FreeRDP_ServerLicense* settings */
FREERDP_LOCAL BOOL license_server_configure(rdpLicense* license);
FREERDP_LOCAL BOOL license_server_send_request(rdpLicense* license);

FREERDP_LOCAL rdpLicense* license_new(rdpRdp* rdp);
FREERDP_LOCAL void license_free(rdpLicense* license);

#define LICENSE_TAG FREERDP_TAG("core.license")
#ifdef WITH_DEBUG_LICENSE
#define DEBUG_LICENSE(...) WLog_INFO(LICENSE_TAG, __VA_ARGS__)
#else
#define DEBUG_LICENSE(...) \
	do                     \
	{                      \
	} while (0)
#endif

#endif /* FREERDP_LIB_CORE_LICENSE_H */
