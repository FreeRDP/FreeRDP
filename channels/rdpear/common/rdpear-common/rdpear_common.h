/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2023 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_CHANNEL_RDPEAR_COMMON_H
#define FREERDP_CHANNEL_RDPEAR_COMMON_H

#include <winpr/stream.h>
#include <winpr/asn1.h>
#include <winpr/wlog.h>
#include <winpr/sspi.h>

#include <freerdp/api.h>

#include <rdpear-common/ndr.h>

typedef enum
{
	RDPEAR_PACKAGE_KERBEROS,
	RDPEAR_PACKAGE_NTLM,
	RDPEAR_PACKAGE_UNKNOWN
} RdpEarPackageType;

/* RDPEAR 2.2.1.1 */
typedef enum
{
	// Start Kerberos remote calls
	RemoteCallKerbMinimum = 0x100,
	RemoteCallKerbNegotiateVersion = 0x100,
	RemoteCallKerbBuildAsReqAuthenticator,
	RemoteCallKerbVerifyServiceTicket,
	RemoteCallKerbCreateApReqAuthenticator,
	RemoteCallKerbDecryptApReply,
	RemoteCallKerbUnpackKdcReplyBody,
	RemoteCallKerbComputeTgsChecksum,
	RemoteCallKerbBuildEncryptedAuthData,
	RemoteCallKerbPackApReply,
	RemoteCallKerbHashS4UPreauth,
	RemoteCallKerbSignS4UPreauthData,
	RemoteCallKerbVerifyChecksum,
	RemoteCallKerbReserved1,
	RemoteCallKerbReserved2,
	RemoteCallKerbReserved3,
	RemoteCallKerbReserved4,
	RemoteCallKerbReserved5,
	RemoteCallKerbReserved6,
	RemoteCallKerbReserved7,
	RemoteCallKerbDecryptPacCredentials,
	RemoteCallKerbCreateECDHKeyAgreement,
	RemoteCallKerbCreateDHKeyAgreement,
	RemoteCallKerbDestroyKeyAgreement,
	RemoteCallKerbKeyAgreementGenerateNonce,
	RemoteCallKerbFinalizeKeyAgreement,
	RemoteCallKerbMaximum = 0x1ff,
	// End Kerberos remote calls

	// Start NTLM remote calls
	RemoteCallNtlmMinimum = 0x200,
	RemoteCallNtlmNegotiateVersion = 0x200,
	RemoteCallNtlmLm20GetNtlm3ChallengeResponse,
	RemoteCallNtlmCalculateNtResponse,
	RemoteCallNtlmCalculateUserSessionKeyNt,
	RemoteCallNtlmCompareCredentials,
	RemoteCallNtlmMaximum = 0x2ff,
	// End NTLM remote calls
} RemoteGuardCallId;

FREERDP_LOCAL RdpEarPackageType rdpear_packageType_from_name(WinPrAsn1_OctetString* package);
FREERDP_LOCAL wStream* rdpear_encodePayload(RdpEarPackageType packageType, wStream* payload);

#define RDPEAR_COMMON_MESSAGE_DECL(V)                                                            \
	FREERDP_LOCAL BOOL ndr_read_##V(NdrContext* context, wStream* s, const void* hints, V* obj); \
	FREERDP_LOCAL BOOL ndr_write_##V(NdrContext* context, wStream* s, const void* hints,         \
	                                 const V* obj);                                              \
	FREERDP_LOCAL void ndr_destroy_##V(NdrContext* context, const void* hints, V* obj);          \
	FREERDP_LOCAL void ndr_dump_##V(wLog* logger, UINT32 lvl, size_t indentLevel, const V* obj); \
	FREERDP_LOCAL NdrMessageType ndr_##V##_descr(void)

/** @brief 2.2.1.2.2 KERB_RPC_OCTET_STRING */
typedef struct
{
	UINT32 length;
	BYTE* value;
} KERB_RPC_OCTET_STRING;

RDPEAR_COMMON_MESSAGE_DECL(KERB_RPC_OCTET_STRING);

/** @brief 2.2.1.2.1 KERB_ASN1_DATA */
typedef struct
{
	UINT32 Pdu;
	NdrArrayHints Asn1BufferHints;
	BYTE* Asn1Buffer;
} KERB_ASN1_DATA;

RDPEAR_COMMON_MESSAGE_DECL(KERB_ASN1_DATA);

/** @brief 2.3.10 RPC_UNICODE_STRING (MS-DTYP) */
typedef struct
{
	NdrVaryingArrayHints lenHints;
	UINT32 strLength;
	WCHAR* Buffer;
} RPC_UNICODE_STRING;

RDPEAR_COMMON_MESSAGE_DECL(RPC_UNICODE_STRING);

/** @brief 2.2.1.2.3 KERB_RPC_INTERNAL_NAME */
typedef struct
{
	UINT16 NameType;
	NdrArrayHints nameHints;
	RPC_UNICODE_STRING* Names;
} KERB_RPC_INTERNAL_NAME;

RDPEAR_COMMON_MESSAGE_DECL(KERB_RPC_INTERNAL_NAME);

/** @brief 2.2.1.2.8 KERB_RPC_ENCRYPTION_KEY */
typedef struct
{
	UINT32 reserved1;
	UINT32 reserved2;
	KERB_RPC_OCTET_STRING reserved3;
} KERB_RPC_ENCRYPTION_KEY;

RDPEAR_COMMON_MESSAGE_DECL(KERB_RPC_ENCRYPTION_KEY);

/** @brief 2.2.2.1.8 BuildEncryptedAuthData */
typedef struct
{
	UINT32 KeyUsage;
	KERB_RPC_ENCRYPTION_KEY* Key;
	KERB_ASN1_DATA* PlainAuthData;
} BuildEncryptedAuthDataReq;

RDPEAR_COMMON_MESSAGE_DECL(BuildEncryptedAuthDataReq);

/** @brief 2.2.2.1.7 ComputeTgsChecksum */
typedef struct
{
	KERB_ASN1_DATA* requestBody;
	KERB_RPC_ENCRYPTION_KEY* Key;
	UINT32 ChecksumType;
} ComputeTgsChecksumReq;

RDPEAR_COMMON_MESSAGE_DECL(ComputeTgsChecksumReq);

/** @brief 2.2.2.1.4 CreateApReqAuthenticator */
typedef struct
{
	KERB_RPC_ENCRYPTION_KEY* EncryptionKey;
	ULONG SequenceNumber;
	KERB_RPC_INTERNAL_NAME* ClientName;
	RPC_UNICODE_STRING* ClientRealm;
	PLARGE_INTEGER SkewTime;
	KERB_RPC_ENCRYPTION_KEY* SubKey; // optional
	KERB_ASN1_DATA* AuthData;        // optional
	KERB_ASN1_DATA* GssChecksum;     // optional
	ULONG KeyUsage;
} CreateApReqAuthenticatorReq;

RDPEAR_COMMON_MESSAGE_DECL(CreateApReqAuthenticatorReq);

/** @brief 2.2.2.1.4 CreateApReqAuthenticator */
typedef struct
{
	LARGE_INTEGER AuthenticatorTime;
	KERB_ASN1_DATA Authenticator;
	LONG KerbProtocolError;
} CreateApReqAuthenticatorResp;

RDPEAR_COMMON_MESSAGE_DECL(CreateApReqAuthenticatorResp);

/** @brief 2.2.2.1.6 UnpackKdcReplyBody */
typedef struct
{
	KERB_ASN1_DATA* EncryptedData;
	KERB_RPC_ENCRYPTION_KEY* Key;
	KERB_RPC_ENCRYPTION_KEY* StrengthenKey;
	ULONG Pdu;
	ULONG KeyUsage;
} UnpackKdcReplyBodyReq;

RDPEAR_COMMON_MESSAGE_DECL(UnpackKdcReplyBodyReq);

/** @brief 2.2.2.1.6 UnpackKdcReplyBody */
typedef struct
{
	LONG KerbProtocolError;
	KERB_ASN1_DATA ReplyBody;
} UnpackKdcReplyBodyResp;

RDPEAR_COMMON_MESSAGE_DECL(UnpackKdcReplyBodyResp);

typedef struct
{
	KERB_ASN1_DATA* EncryptedReply;
	KERB_RPC_ENCRYPTION_KEY* Key;
} DecryptApReplyReq;

RDPEAR_COMMON_MESSAGE_DECL(DecryptApReplyReq);

typedef struct
{
	KERB_ASN1_DATA* Reply;
	KERB_ASN1_DATA* ReplyBody;
	KERB_RPC_ENCRYPTION_KEY* SessionKey;
} PackApReplyReq;

RDPEAR_COMMON_MESSAGE_DECL(PackApReplyReq);

typedef struct
{
	NdrArrayHints PackedReplyHints;
	BYTE* PackedReply;
} PackApReplyResp;

RDPEAR_COMMON_MESSAGE_DECL(PackApReplyResp);

#undef RDPEAR_COMMON_MESSAGE_DECL

#endif /* FREERDP_CHANNEL_RDPEAR_COMMON_H */
