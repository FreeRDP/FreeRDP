/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
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

#ifndef FREERDP_LIB_CORE_NLA_H
#define FREERDP_LIB_CORE_NLA_H

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

FREERDP_LOCAL int nla_authenticate(rdpNla* nla);
FREERDP_LOCAL LPTSTR nla_make_spn(const char* ServiceClass, const char* hostname);

FREERDP_LOCAL int nla_client_begin(rdpNla* nla);
FREERDP_LOCAL int nla_recv_pdu(rdpNla* nla, wStream* s);

FREERDP_LOCAL SEC_WINNT_AUTH_IDENTITY* nla_get_identity(rdpNla* nla);

FREERDP_LOCAL NLA_STATE nla_get_state(rdpNla* nla);
FREERDP_LOCAL BOOL nla_set_state(rdpNla* nla, NLA_STATE state);

FREERDP_LOCAL BOOL nla_set_service_principal(rdpNla* nla, LPSTR principal);

FREERDP_LOCAL BOOL nla_impersonate(rdpNla* nla);
FREERDP_LOCAL BOOL nla_revert_to_self(rdpNla* nla);

FREERDP_LOCAL rdpNla* nla_new(freerdp* instance, rdpTransport* transport, rdpSettings* settings);
FREERDP_LOCAL void nla_free(rdpNla* nla);

#endif /* FREERDP_LIB_CORE_NLA_H */
