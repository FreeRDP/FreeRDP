/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Security
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __SECURITY_H
#define __SECURITY_H

#include "rdp.h"
#include "crypto.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

void security_master_secret(uint8* premaster_secret, uint8* client_random, uint8* server_random, uint8* output);
void security_session_key_blob(uint8* master_secret, uint8* client_random, uint8* server_random, uint8* output);
void security_mac_salt_key(uint8* session_key_blob, uint8* client_random, uint8* server_random, uint8* output);
void security_licensing_encryption_key(uint8* session_key_blob, uint8* client_random, uint8* server_random, uint8* output);
void security_mac_data(uint8* mac_salt_key, uint8* data, uint32 length, uint8* output);

void security_mac_signature(rdpRdp *rdp, uint8* data, uint32 length, uint8* output);
void security_salted_mac_signature(rdpRdp *rdp, uint8* data, uint32 length, boolean encryption, uint8* output);
boolean security_establish_keys(uint8* client_random, rdpRdp* rdp);

boolean security_encrypt(uint8* data, int length, rdpRdp* rdp);
boolean security_decrypt(uint8* data, int length, rdpRdp* rdp);

void security_hmac_signature(uint8* data, int length, uint8* output, rdpRdp* rdp);
boolean security_fips_encrypt(uint8* data, int length, rdpRdp* rdp);
boolean security_fips_decrypt(uint8* data, int length, rdpRdp* rdp);
boolean security_fips_check_signature(uint8* data, int length, uint8* sig, rdpRdp* rdp);

#endif /* __SECURITY_H */
