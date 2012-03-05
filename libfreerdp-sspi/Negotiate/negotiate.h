/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Negotiate Security Package
 *
 * Copyright 2011-2012 Jiten Pathy
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

#ifndef FREERDP_SSPI_NEGOTIATE_PRIVATE_H
#define FREERDP_SSPI_NEGOTIATE_PRIVATE_H

#include <freerdp/sspi/sspi.h>
#include <freerdp/utils/unicode.h>

#include "../Kerberos/kerberos.h"

#include "../sspi.h"

#define NTLM_OID            "1.3.6.1.4.1.311.2.2.10"

enum _NEGOTIATE_STATE
{
	NEGOTIATE_STATE_INITIAL,
	NEGOTIATE_STATE_NEGOINIT,
	NEGOTIATE_STATE_NEGORESP,
	NEGOTIATE_STATE_FINAL
};
typedef enum _NEGOTIATE_STATE NEGOTIATE_STATE;

struct _NEGOTIATE_CONTEXT
{
	NEGOTIATE_STATE state;
	UNICONV* uniconv;
	uint32 NegotiateFlags;
	CTXT_HANDLE* auth_ctx;
	SEC_AUTH_IDENTITY identity;
	SEC_BUFFER NegoInitMessage;
};
typedef struct _NEGOTIATE_CONTEXT NEGOTIATE_CONTEXT;

NEGOTIATE_CONTEXT* negotiate_ContextNew();
void negotiate_ContextFree(NEGOTIATE_CONTEXT* context);

#endif /* FREERDP_SSPI_NEGOTIATE_PRIVATE_H */

