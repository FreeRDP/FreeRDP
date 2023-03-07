/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDSTLS Security protocol
 *
 * Copyright 2023 Joan Torres <joan.torres@suse.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_LIB_CORE_RDSTLS_H
#define FREERDP_LIB_CORE_RDSTLS_H

typedef struct rdp_rdstls rdpRdstls;

#include <freerdp/freerdp.h>

#define RDSTLS_VERSION_1 0x01

#define RDSTLS_TYPE_CAPABILITIES 0x01
#define RDSTLS_TYPE_AUTHREQ 0x02
#define RDSTLS_TYPE_AUTHRSP 0x04

#define RDSTLS_DATA_CAPABILITIES 0x01
#define RDSTLS_DATA_PASSWORD_CREDS 0x01
#define RDSTLS_DATA_AUTORECONNECT_COOKIE 0x02
#define RDSTLS_DATA_RESULT_CODE 0x01

typedef enum
{
	RDSTLS_STATE_CAPABILITIES,
	RDSTLS_STATE_AUTH_REQ,
	RDSTLS_STATE_AUTH_RSP,
} RDSTLS_STATE;

FREERDP_LOCAL int rdstls_authenticate(rdpRdstls* rdstls);

FREERDP_LOCAL rdpRdstls* rdstls_new(rdpContext* context, rdpTransport* transport);
FREERDP_LOCAL void rdstls_free(rdpRdstls* rdstls);

#endif /* FREERDP_LIB_CORE_RDSTLS_H */
