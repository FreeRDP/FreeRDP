/**
 * WinPR: Windows Portable Runtime
 * Credential Security Support Provider (CredSSP)
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

#ifndef WINPR_SSPI_CREDSSP_H
#define WINPR_SSPI_CREDSSP_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#if defined(_WIN32) && !defined(_UWP)

#include <credssp.h>

#else

typedef enum
{
	CredsspPasswordCreds = 2,
	CredsspSchannelCreds = 4,
	CredsspCertificateCreds = 13,
	CredsspSubmitBufferBoth = 50,
	CredsspSubmitBufferBothOld = 51,
	CredsspCredEx = 100
} CREDSPP_SUBMIT_TYPE;

typedef struct
{
	CREDSPP_SUBMIT_TYPE Type;
	void* pSchannelCred;
	void* pSpnegoCred;
} CREDSSP_CRED, *PCREDSSP_CRED;

#define CREDSSP_CRED_EX_VERSION 0

typedef struct
{
	CREDSPP_SUBMIT_TYPE Type;
	DWORD Version;
	DWORD Flags;
	DWORD Reserved;
	CREDSSP_CRED Cred;
} CREDSSP_CRED_EX, *PCREDSSP_CRED_EX;

#define CREDSSP_FLAG_REDIRECT 0x00000001

#endif /* _WIN32 && !_UWP */

/* TSRequest codec, shared by the FreeRDP NLA client and the WinPR CredSSP package.
 * See [MS-CSSP] 2.2.1. This is WinPR's own codec, available on every platform. */

#include <winpr/sspi.h>
#include <winpr/stream.h>

/** @brief [MS-CSSP] 2.2.1 TSRequest fields the CredSSP exchange carries. */
typedef struct
{
	UINT32 version;
	SecBuffer negoToken;
	SecBuffer authInfo;
	SecBuffer pubKeyAuth;
	INT32 errorCode;
	SecBuffer clientNonce;
} WINPR_TSREQUEST;

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Release the buffers a WINPR_TSREQUEST owns (the struct itself is not freed).
	 *  @since version 3.33.0 */
	WINPR_API void winpr_tsrequest_free_contents(WINPR_TSREQUEST* request);

	/** @brief Encode a TSRequest into a DER buffer.
	 *
	 * A field is emitted when its buffer is non-empty; errorCode only for a peer at version 3 or
	 * above other than 5, and clientNonce only for a client (not a server).
	 *
	 * @param request the fields to encode
	 * @param peerVersion the negotiated peer version (gates errorCode)
	 * @param server \b TRUE to encode as a server (suppresses clientNonce)
	 * @param output receives the DER bytes; the caller frees it with sspi_SecBufferFree
	 * @return \b TRUE on success
	 * @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_tsrequest_encode(const WINPR_TSREQUEST* request, UINT32 peerVersion,
	                                      BOOL server, SecBuffer* output);

	/** @brief Decode a TSRequest from a DER stream into \b request (buffers allocated).
	 *
	 * @return \b TRUE on success; the caller frees the result with winpr_tsrequest_free_contents
	 * @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_tsrequest_decode(wStream* s, WINPR_TSREQUEST* request);

#ifdef __cplusplus
}
#endif

/* TSCredentials codec ([MS-CSSP] 2.2.1.2). credType selects the inner structure. */
#define WINPR_TSCREDS_USER_PASSWD 1
#define WINPR_TSCREDS_SMARTCARD 2
#define WINPR_TSCREDS_REMOTEGUARD 6

#include <winpr/secapi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Serialize a KERB_TICKET_LOGON into the tspkg wire layout (type, flags, lengths,
	 * TGS/TGT offsets, then the two tickets).
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_kerb_ticket_logon_encode(const KERB_TICKET_LOGON* ticket,
	                                              SecBuffer* output);

	/** @brief Parse a KERB_TICKET_LOGON; ServiceTicket and TicketGrantingTicket are allocated
	 * copies the caller frees.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_kerb_ticket_logon_decode(const BYTE* data, size_t length,
	                                              KERB_TICKET_LOGON* ticket);

	/** @brief Encode a credType 1 (TSPasswordCreds) TSCredentials from an identity.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_tscredentials_encode_password(const SEC_WINNT_AUTH_IDENTITY_W* identity,
	                                                   SecBuffer* output);

	/** @brief Encode a credType 6 (TSRemoteGuardCreds) TSCredentials carrying a Kerberos
	 * KERB_TICKET_LOGON as the logon credential.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_tscredentials_encode_remoteguard_kerb(const KERB_TICKET_LOGON* ticket,
	                                                           SecBuffer* output);

	/** @brief Compute the CredSSP version-6 pubKeyAuth hash: SHA256 over the binding magic
	 * (client-to-server or server-to-client, with its trailing NUL), the client nonce, and the
	 * server SubjectPublicKey.
	 *
	 * @param server \b TRUE to use the server-to-client magic (what the server sends and the
	 *               client verifies), \b FALSE for the client-to-server direction
	 * @param nonce the 32-byte client nonce
	 * @param publicKey the server SubjectPublicKey (DER SPKI)
	 * @param hash receives the 32-byte digest; the caller frees it with sspi_SecBufferFree
	 * @return \b TRUE on success
	 * @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_credssp_public_key_hash(BOOL server, const SecBuffer* nonce,
	                                             const SecBuffer* publicKey, SecBuffer* hash);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_SSPI_CREDSSP_H */
