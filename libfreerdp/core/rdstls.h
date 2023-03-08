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

FREERDP_LOCAL SSIZE_T rdstls_parse_pdu(wLog* log, wStream* s);

FREERDP_LOCAL rdpRdstls* rdstls_new(rdpContext* context, rdpTransport* transport);
FREERDP_LOCAL void rdstls_free(rdpRdstls* rdstls);

FREERDP_LOCAL int rdstls_authenticate(rdpRdstls* rdstls);

#endif /* FREERDP_LIB_CORE_RDSTLS_H */
