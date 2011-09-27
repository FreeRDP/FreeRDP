/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Credential Security Support Provider (CredSSP)
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __CREDSSP_H
#define __CREDSSP_H

typedef struct rdp_credssp rdpCredssp;

#include "tls.h"
#include "ber.h"
#include "crypto.h"
#include "transport.h"
#include <freerdp/settings.h>
#include <freerdp/utils/blob.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/hexdump.h>

#include "ntlmssp.h"

struct rdp_credssp
{
	rdpBlob negoToken;
	rdpBlob pubKeyAuth;
	rdpBlob authInfo;
	int send_seq_num;
	rdpBlob public_key;
	rdpBlob ts_credentials;
	rdpSettings* settings;
	CryptoRc4 rc4_seal_state;
	struct _NTLMSSP *ntlmssp;
	struct rdp_transport* transport;
};

int credssp_authenticate(rdpCredssp* credssp);

void credssp_send(rdpCredssp* credssp, rdpBlob* negoToken, rdpBlob* authInfo, rdpBlob* pubKeyAuth);
int credssp_recv(rdpCredssp* credssp, rdpBlob* negoToken, rdpBlob* authInfo, rdpBlob* pubKeyAuth);

void credssp_encrypt_public_key(rdpCredssp* credssp, rdpBlob* d);
void credssp_encrypt_ts_credentials(rdpCredssp* credssp, rdpBlob* d);
int credssp_verify_public_key(rdpCredssp* credssp, rdpBlob* d);
void credssp_encode_ts_credentials(rdpCredssp* credssp);

void credssp_current_time(uint8* timestamp);
void credssp_rc4k(uint8* key, int length, uint8* plaintext, uint8* ciphertext);

rdpCredssp* credssp_new(rdpTransport* transport);
void credssp_free(rdpCredssp* credssp);

#endif /* __CREDSSP_H */
