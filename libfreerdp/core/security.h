/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_LIB_CORE_SECURITY_H
#define FREERDP_LIB_CORE_SECURITY_H

#include "rdp.h"
#include <freerdp/crypto/crypto.h>

#include <freerdp/freerdp.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

FREERDP_LOCAL BOOL security_master_secret(const BYTE* premaster_secret, const BYTE* client_random,
                                          const BYTE* server_random, BYTE* output);
FREERDP_LOCAL BOOL security_session_key_blob(const BYTE* master_secret, const BYTE* client_random,
                                             const BYTE* server_random, BYTE* output);
FREERDP_LOCAL void security_mac_salt_key(const BYTE* session_key_blob, const BYTE* client_random,
                                         const BYTE* server_random, BYTE* output);
FREERDP_LOCAL BOOL security_licensing_encryption_key(const BYTE* session_key_blob,
                                                     const BYTE* client_random,
                                                     const BYTE* server_random, BYTE* output);
FREERDP_LOCAL BOOL security_mac_data(const BYTE* mac_salt_key, const BYTE* data, UINT32 length,
                                     BYTE* output);
FREERDP_LOCAL BOOL security_mac_signature(rdpRdp* rdp, const BYTE* data, UINT32 length,
                                          BYTE* output);
FREERDP_LOCAL BOOL security_salted_mac_signature(rdpRdp* rdp, const BYTE* data, UINT32 length,
                                                 BOOL encryption, BYTE* output);
FREERDP_LOCAL BOOL security_establish_keys(const BYTE* client_random, rdpRdp* rdp);
FREERDP_LOCAL BOOL security_encrypt(BYTE* data, size_t length, rdpRdp* rdp);
FREERDP_LOCAL BOOL security_decrypt(BYTE* data, size_t length, rdpRdp* rdp);
FREERDP_LOCAL BOOL security_hmac_signature(const BYTE* data, size_t length, BYTE* output,
                                           rdpRdp* rdp);
FREERDP_LOCAL BOOL security_fips_encrypt(BYTE* data, size_t length, rdpRdp* rdp);
FREERDP_LOCAL BOOL security_fips_decrypt(BYTE* data, size_t length, rdpRdp* rdp);
FREERDP_LOCAL BOOL security_fips_check_signature(const BYTE* data, size_t length, const BYTE* sig,
                                                 rdpRdp* rdp);

#endif /* FREERDP_LIB_CORE_SECURITY_H */
