/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel: Kerberos redirected-call servicing
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

#ifndef FREERDP_CHANNELS_RDPEAR_COMMON_KERB_H
#define FREERDP_CHANNELS_RDPEAR_COMMON_KERB_H

#include <krb5.h>

#include <winpr/wtypes.h>
#include <winpr/stream.h>

/**
 * Service one decrypted [MS-RDPEAR] packet payload (the TSRemoteGuardInnerPacket, an ASN.1
 * SEQUENCE { [1] packageName, [2] payload }) and produce the decrypted response payload in the
 * same envelope, ready to be encrypted for the wire. The handlers run krb5 crypto on the keys
 * carried in the request and never touch the ticket cache, so any krb5_context suffices.
 *
 * @param krb a krb5 context for the crypto operations
 * @param plaintext the decrypted request payload
 * @param plaintextLen its length in bytes
 * @return a newly allocated stream with the decrypted response payload (caller frees with
 *         Stream_Free(.., TRUE)), or NULL on error
 */
WINPR_ATTR_MALLOC(Stream_Free, 1)
wStream* rdpear_kerb_service(krb5_context krb, const BYTE* plaintext, size_t plaintextLen);

#endif /* FREERDP_CHANNELS_RDPEAR_COMMON_KERB_H */
