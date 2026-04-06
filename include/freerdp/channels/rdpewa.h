/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * WebAuthn Virtual Channel Extension [MS-RDPEWA]
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#ifndef FREERDP_CHANNEL_RDPEWA_H
#define FREERDP_CHANNEL_RDPEWA_H

#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

/** The command line name of the channel
 *
 *  \since version 3.0.0
 */
#define RDPEWA_CHANNEL_NAME "rdpewa"
#define RDPEWA_DVC_CHANNEL_NAME "WebAuthN_Channel"

/** @brief RPC command types sent in the "command" field of request messages */
#define CYCAPCBOR_RPC_COMMAND_WEB_AUTHN 5
#define CTAPCBOR_RPC_COMMAND_IUVPAA 6
#define CTAPCBOR_RPC_COMMAND_CANCEL_CUR_OP 7
#define CTAPCBOR_RPC_COMMAND_API_VERSION 8
#define CTAPCBOR_RPC_COMMAND_GET_CREDENTIALS 9
#define CTAPCBOR_RPC_COMMAND_GET_AUTHENTICATOR_LIST 12

/** @brief WebAuthn sub-command types (first byte of "request" field for command 5) */
#define CTAPCBOR_CMD_MAKE_CREDENTIAL 0x01
#define CTAPCBOR_CMD_GET_ASSERTION 0x02

/** @brief CTAP2 authenticatorMakeCredential parameter keys (section 6.1) */
#define CTAP_MAKECRED_CLIENT_DATA_HASH 1
#define CTAP_MAKECRED_RP 2
#define CTAP_MAKECRED_USER 3
#define CTAP_MAKECRED_PUB_KEY_CRED_PARAMS 4
#define CTAP_MAKECRED_EXCLUDE_LIST 5
#define CTAP_MAKECRED_OPTIONS 7

/** @brief CTAP2 authenticatorGetAssertion parameter keys (section 6.2) */
#define CTAP_GETASSERT_RP_ID 1
#define CTAP_GETASSERT_CLIENT_DATA_HASH 2
#define CTAP_GETASSERT_ALLOW_LIST 3
#define CTAP_GETASSERT_OPTIONS 5

/** @brief Flags bitfield values (XOR'd in "flags" field) */
#define CTAPCLT_U2F_FLAG 0x00020000
#define CTAPCLT_DUAL_FLAG 0x00040000
#define CTAPCLT_SELECT_CREDENTIAL_ALLOW_UV_FLAG 0x00008000
#define CTAPCLT_CLIENT_PIN_REQUIRED_FLAG 0x00100000
#define CTAPCLT_UV_REQUIRED_FLAG 0x00400000
#define CTAPCLT_UV_PREFERRED_FLAG 0x00800000
#define CTAPCLT_UV_NOT_REQUIRED_FLAG 0x01000000
#define CTAPCLT_HMAC_SECRET_EXTENSION_FLAG 0x04000000
#define CTAPCLT_FORCE_U2F_V2_FLAG 0x08000000

/** @brief Authenticator attachment */
#define WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY 0
#define WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM 1
#define WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM 2

/** @brief User verification requirement */
#define WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY 0
#define WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED 1
#define WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED 2
#define WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED 3

/** @brief Attestation conveyance preference */
#define WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY 0
#define WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE 1
#define WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT 2
#define WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT 3

/** @brief Enterprise attestation */
#define WEBAUTHN_ENTERPRISE_ATTESTATION_NONE 0
#define WEBAUTHN_ENTERPRISE_ATTESTATION_VENDOR_FACILITATED 1
#define WEBAUTHN_ENTERPRISE_ATTESTATION_PLATFORM_MANAGED 2

/** @brief Credential large blob operation */
#define WEBAUTHN_CRED_LARGE_BLOB_OPERATION_NONE 0
#define WEBAUTHN_CRED_LARGE_BLOB_OPERATION_GET 1
#define WEBAUTHN_CRED_LARGE_BLOB_OPERATION_SET 2
#define WEBAUTHN_CRED_LARGE_BLOB_OPERATION_DELETE 3

/** @brief Large blob support */
#define WEBAUTHN_LARGE_BLOB_SUPPORT_NONE 0
#define WEBAUTHN_LARGE_BLOB_SUPPORT_REQUIRED 1
#define WEBAUTHN_LARGE_BLOB_SUPPORT_PREFERRED 2

/** @brief Authenticator logo request type */
#define WEBAUTHN_AUTHENTICATOR_LOGO_REQUEST_TYPE_NONE 0
#define WEBAUTHN_AUTHENTICATOR_LOGO_REQUEST_TYPE_LIGHT 1
#define WEBAUTHN_AUTHENTICATOR_LOGO_REQUEST_TYPE_DARK 2
#define WEBAUTHN_AUTHENTICATOR_LOGO_REQUEST_TYPE_ALL 3

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif
#endif /* FREERDP_CHANNEL_RDPEWA_H */
