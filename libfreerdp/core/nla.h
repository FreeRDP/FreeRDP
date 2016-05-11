/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Credential Security Support Provider (CredSSP)
 *
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

#ifndef FREERDP_CORE_NLA_H
#define FREERDP_CORE_NLA_H

typedef struct rdp_nla rdpNla;

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#include <winpr/sspi.h>
#include <winpr/stream.h>
#include <winpr/crypto.h>

#include <freerdp/crypto/tls.h>
#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/der.h>
#include <freerdp/crypto/crypto.h>

#include "transport.h"

enum _NLA_STATE
{
	NLA_STATE_INITIAL,
	NLA_STATE_NEGO_TOKEN,
	NLA_STATE_PUB_KEY_AUTH,
	NLA_STATE_AUTH_INFO,
	NLA_STATE_POST_NEGO,
	NLA_STATE_FINAL
};
typedef enum _NLA_STATE NLA_STATE;

struct rdp_nla
{
	BOOL server;
	NLA_STATE state;
	int sendSeqNum;
	int recvSeqNum;
	freerdp* instance;
	CtxtHandle context;
	LPTSTR SspiModule;
	rdpSettings* settings;
	rdpTransport* transport;
	UINT32 cbMaxToken;
	UINT32 version;
	UINT32 errorCode;
	ULONG fContextReq;
	ULONG pfContextAttr;
	BOOL haveContext;
	BOOL haveInputBuffer;
	BOOL havePubKeyAuth;
	SECURITY_STATUS status;
	CredHandle credentials;
	TimeStamp expiration;
	PSecPkgInfo pPackageInfo;
	SecBuffer inputBuffer;
	SecBuffer outputBuffer;
	SecBufferDesc inputBufferDesc;
	SecBufferDesc outputBufferDesc;
	SecBuffer negoToken;
	SecBuffer pubKeyAuth;
	SecBuffer authInfo;
	SecBuffer PublicKey;
	SecBuffer tsCredentials;
	WINPR_RC4_CTX rc4SealState;
	LPTSTR ServicePrincipalName;
	SEC_WINNT_AUTH_IDENTITY* identity;
	PSecurityFunctionTable table;
	SecPkgContext_Sizes ContextSizes;
};

int nla_authenticate(rdpNla* nla);
LPTSTR nla_make_spn(const char* ServiceClass, const char* hostname);

int nla_client_begin(rdpNla* nla);
int nla_recv_pdu(rdpNla* nla, wStream* s);

rdpNla* nla_new(freerdp* instance, rdpTransport* transport, rdpSettings* settings);
void nla_free(rdpNla* nla);

#endif /* FREERDP_CORE_NLA_H */
