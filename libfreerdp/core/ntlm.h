/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NTLM over HTTP
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CORE_NTLM_H
#define FREERDP_CORE_NTLM_H

typedef struct rdp_ntlm rdpNtlm;
typedef struct rdp_ntlm_http rdpNtlmHttp;

#include "tcp.h"
#include "rts.h"
#include "http.h"
#include "transport.h"

#include <time.h>
#include <winpr/sspi.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/hexdump.h>

struct rdp_ntlm
{
	CtxtHandle context;
	ULONG cbMaxToken;
	ULONG fContextReq;
	ULONG pfContextAttr;
	TimeStamp expiration;
	PSecBuffer pBuffer;
	SecBuffer inputBuffer;
	SecBuffer outputBuffer;
	BOOL haveContext;
	BOOL haveInputBuffer;
	LPTSTR ServicePrincipalName;
	SecBufferDesc inputBufferDesc;
	SecBufferDesc outputBufferDesc;
	CredHandle credentials;
	BOOL confidentiality;
	SecPkgInfo* pPackageInfo;
	SecurityFunctionTable* table;
	SEC_WINNT_AUTH_IDENTITY identity;
	SecPkgContext_Sizes ContextSizes;
};

struct rdp_ntlm_http
{
	rdpNtlm* ntlm;
	HttpContext* context;
};

BOOL ntlm_authenticate(rdpNtlm* ntlm);

BOOL ntlm_client_init(rdpNtlm* ntlm, BOOL confidentiality, char* user, char* domain, char* password);
void ntlm_client_uninit(rdpNtlm* ntlm);

rdpNtlm* ntlm_new();
void ntlm_free(rdpNtlm* ntlm);

#endif /* FREERDP_CORE_NTLM_H */
