/**
 * WinPR: Windows Portable Runtime
 * Security Support Provider Interface (SSPI)
 *
 * Copyright 2012-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_SSPI_H
#define WINPR_SSPI_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/windows.h>
#include <winpr/security.h>

#ifdef _WIN32

#include <tchar.h>
#include <winerror.h>

#define SECURITY_WIN32
#include <sspi.h>
#include <security.h>

#endif

#if !defined(_WIN32) || defined(_UWP)

#ifndef SEC_ENTRY
#define SEC_ENTRY
#endif

typedef CHAR SEC_CHAR;
typedef WCHAR SEC_WCHAR;

struct _SECURITY_INTEGER
{
	UINT32 LowPart;
	INT32 HighPart;
};
typedef struct _SECURITY_INTEGER SECURITY_INTEGER;

typedef SECURITY_INTEGER TimeStamp;
typedef SECURITY_INTEGER* PTimeStamp;

#ifndef __SECSTATUS_DEFINED__
typedef LONG SECURITY_STATUS;
#define __SECSTATUS_DEFINED__
#endif

struct _SecPkgInfoA
{
	UINT32 fCapabilities;
	UINT16 wVersion;
	UINT16 wRPCID;
	UINT32 cbMaxToken;
	SEC_CHAR* Name;
	SEC_CHAR* Comment;
};
typedef struct _SecPkgInfoA SecPkgInfoA;
typedef SecPkgInfoA* PSecPkgInfoA;

struct _SecPkgInfoW
{
	UINT32 fCapabilities;
	UINT16 wVersion;
	UINT16 wRPCID;
	UINT32 cbMaxToken;
	SEC_WCHAR* Name;
	SEC_WCHAR* Comment;
};
typedef struct _SecPkgInfoW SecPkgInfoW;
typedef SecPkgInfoW* PSecPkgInfoW;

#ifdef UNICODE
#define SecPkgInfo SecPkgInfoW
#define PSecPkgInfo PSecPkgInfoW
#else
#define SecPkgInfo SecPkgInfoA
#define PSecPkgInfo PSecPkgInfoA
#endif

#endif

#define NTLM_SSP_NAME	_T("NTLM")
#define KERBEROS_SSP_NAME	_T("Kerberos")
#define NEGO_SSP_NAME	_T("Negotiate")

#define SECPKG_ID_NONE				0xFFFF

#define SECPKG_FLAG_INTEGRITY			0x00000001
#define SECPKG_FLAG_PRIVACY			0x00000002
#define SECPKG_FLAG_TOKEN_ONLY			0x00000004
#define SECPKG_FLAG_DATAGRAM			0x00000008
#define SECPKG_FLAG_CONNECTION			0x00000010
#define SECPKG_FLAG_MULTI_REQUIRED		0x00000020
#define SECPKG_FLAG_CLIENT_ONLY			0x00000040
#define SECPKG_FLAG_EXTENDED_ERROR		0x00000080
#define SECPKG_FLAG_IMPERSONATION		0x00000100
#define SECPKG_FLAG_ACCEPT_WIN32_NAME		0x00000200
#define SECPKG_FLAG_STREAM			0x00000400
#define SECPKG_FLAG_NEGOTIABLE			0x00000800
#define SECPKG_FLAG_GSS_COMPATIBLE		0x00001000
#define SECPKG_FLAG_LOGON			0x00002000
#define SECPKG_FLAG_ASCII_BUFFERS		0x00004000
#define SECPKG_FLAG_FRAGMENT			0x00008000
#define SECPKG_FLAG_MUTUAL_AUTH			0x00010000
#define SECPKG_FLAG_DELEGATION			0x00020000
#define SECPKG_FLAG_READONLY_WITH_CHECKSUM	0x00040000
#define SECPKG_FLAG_RESTRICTED_TOKENS		0x00080000
#define SECPKG_FLAG_NEGO_EXTENDER		0x00100000
#define SECPKG_FLAG_NEGOTIABLE2			0x00200000

#ifndef _WINERROR_

#define SEC_E_OK				0x00000000
#define SEC_E_INSUFFICIENT_MEMORY		0x80090300
#define SEC_E_INVALID_HANDLE			0x80090301
#define SEC_E_UNSUPPORTED_FUNCTION		0x80090302
#define SEC_E_TARGET_UNKNOWN			0x80090303
#define SEC_E_INTERNAL_ERROR			0x80090304
#define SEC_E_SECPKG_NOT_FOUND			0x80090305
#define SEC_E_NOT_OWNER				0x80090306
#define SEC_E_CANNOT_INSTALL			0x80090307
#define SEC_E_INVALID_TOKEN			0x80090308
#define SEC_E_CANNOT_PACK			0x80090309
#define SEC_E_QOP_NOT_SUPPORTED			0x8009030A
#define SEC_E_NO_IMPERSONATION			0x8009030B
#define SEC_E_LOGON_DENIED			0x8009030C
#define SEC_E_UNKNOWN_CREDENTIALS		0x8009030D
#define SEC_E_NO_CREDENTIALS			0x8009030E
#define SEC_E_MESSAGE_ALTERED			0x8009030F
#define SEC_E_OUT_OF_SEQUENCE			0x80090310
#define SEC_E_NO_AUTHENTICATING_AUTHORITY	0x80090311
#define SEC_E_BAD_PKGID				0x80090316
#define SEC_E_CONTEXT_EXPIRED			0x80090317
#define SEC_E_INCOMPLETE_MESSAGE		0x80090318
#define SEC_E_INCOMPLETE_CREDENTIALS		0x80090320
#define SEC_E_BUFFER_TOO_SMALL			0x80090321
#define SEC_E_WRONG_PRINCIPAL			0x80090322
#define SEC_E_TIME_SKEW				0x80090324
#define SEC_E_UNTRUSTED_ROOT			0x80090325
#define SEC_E_ILLEGAL_MESSAGE			0x80090326
#define SEC_E_CERT_UNKNOWN			0x80090327
#define SEC_E_CERT_EXPIRED			0x80090328
#define SEC_E_ENCRYPT_FAILURE			0x80090329
#define SEC_E_DECRYPT_FAILURE			0x80090330
#define SEC_E_ALGORITHM_MISMATCH		0x80090331
#define SEC_E_SECURITY_QOS_FAILED		0x80090332
#define SEC_E_UNFINISHED_CONTEXT_DELETED	0x80090333
#define SEC_E_NO_TGT_REPLY			0x80090334
#define SEC_E_NO_IP_ADDRESSES			0x80090335
#define SEC_E_WRONG_CREDENTIAL_HANDLE		0x80090336
#define SEC_E_CRYPTO_SYSTEM_INVALID		0x80090337
#define SEC_E_MAX_REFERRALS_EXCEEDED		0x80090338
#define SEC_E_MUST_BE_KDC			0x80090339
#define SEC_E_STRONG_CRYPTO_NOT_SUPPORTED	0x8009033A
#define SEC_E_TOO_MANY_PRINCIPALS		0x8009033B
#define SEC_E_NO_PA_DATA			0x8009033C
#define SEC_E_PKINIT_NAME_MISMATCH		0x8009033D
#define SEC_E_SMARTCARD_LOGON_REQUIRED		0x8009033E
#define SEC_E_SHUTDOWN_IN_PROGRESS		0x8009033F
#define SEC_E_KDC_INVALID_REQUEST		0x80090340
#define SEC_E_KDC_UNABLE_TO_REFER		0x80090341
#define SEC_E_KDC_UNKNOWN_ETYPE			0x80090342
#define SEC_E_UNSUPPORTED_PREAUTH		0x80090343
#define SEC_E_DELEGATION_REQUIRED		0x80090345
#define SEC_E_BAD_BINDINGS			0x80090346
#define SEC_E_MULTIPLE_ACCOUNTS			0x80090347
#define SEC_E_NO_KERB_KEY			0x80090348
#define SEC_E_CERT_WRONG_USAGE			0x80090349
#define SEC_E_DOWNGRADE_DETECTED		0x80090350
#define SEC_E_SMARTCARD_CERT_REVOKED		0x80090351
#define SEC_E_ISSUING_CA_UNTRUSTED		0x80090352
#define SEC_E_REVOCATION_OFFLINE_C		0x80090353
#define SEC_E_PKINIT_CLIENT_FAILURE		0x80090354
#define SEC_E_SMARTCARD_CERT_EXPIRED		0x80090355
#define SEC_E_NO_S4U_PROT_SUPPORT		0x80090356
#define SEC_E_CROSSREALM_DELEGATION_FAILURE	0x80090357
#define SEC_E_REVOCATION_OFFLINE_KDC		0x80090358
#define SEC_E_ISSUING_CA_UNTRUSTED_KDC		0x80090359
#define SEC_E_KDC_CERT_EXPIRED			0x8009035A
#define SEC_E_KDC_CERT_REVOKED			0x8009035B
#define SEC_E_INVALID_PARAMETER			0x8009035D
#define SEC_E_DELEGATION_POLICY			0x8009035E
#define SEC_E_POLICY_NLTM_ONLY			0x8009035F
#define SEC_E_NO_CONTEXT			0x80090361
#define SEC_E_PKU2U_CERT_FAILURE		0x80090362
#define SEC_E_MUTUAL_AUTH_FAILED		0x80090363

#define SEC_I_CONTINUE_NEEDED			0x00090312
#define SEC_I_COMPLETE_NEEDED			0x00090313
#define SEC_I_COMPLETE_AND_CONTINUE		0x00090314
#define SEC_I_LOCAL_LOGON			0x00090315
#define SEC_I_CONTEXT_EXPIRED			0x00090317
#define SEC_I_INCOMPLETE_CREDENTIALS		0x00090320
#define SEC_I_RENEGOTIATE			0x00090321
#define SEC_I_NO_LSA_CONTEXT			0x00090323
#define SEC_I_SIGNATURE_NEEDED			0x0009035C
#define SEC_I_NO_RENEGOTIATION			0x00090360

#endif

#define SECURITY_NATIVE_DREP			0x00000010
#define SECURITY_NETWORK_DREP			0x00000000

#define SECPKG_CRED_INBOUND			0x00000001
#define SECPKG_CRED_OUTBOUND			0x00000002
#define SECPKG_CRED_BOTH			0x00000003
#define SECPKG_CRED_AUTOLOGON_RESTRICTED	0x00000010
#define SECPKG_CRED_PROCESS_POLICY_ONLY		0x00000020

/* Security Context Attributes */

#define SECPKG_ATTR_SIZES				0
#define SECPKG_ATTR_NAMES				1
#define SECPKG_ATTR_LIFESPAN				2
#define SECPKG_ATTR_DCE_INFO				3
#define SECPKG_ATTR_STREAM_SIZES			4
#define SECPKG_ATTR_KEY_INFO				5
#define SECPKG_ATTR_AUTHORITY				6
#define SECPKG_ATTR_PROTO_INFO				7
#define SECPKG_ATTR_PASSWORD_EXPIRY			8
#define SECPKG_ATTR_SESSION_KEY				9
#define SECPKG_ATTR_PACKAGE_INFO			10
#define SECPKG_ATTR_USER_FLAGS				11
#define SECPKG_ATTR_NEGOTIATION_INFO			12
#define SECPKG_ATTR_NATIVE_NAMES			13
#define SECPKG_ATTR_FLAGS				14
#define SECPKG_ATTR_USE_VALIDATED			15
#define SECPKG_ATTR_CREDENTIAL_NAME			16
#define SECPKG_ATTR_TARGET_INFORMATION			17
#define SECPKG_ATTR_ACCESS_TOKEN			18
#define SECPKG_ATTR_TARGET				19
#define SECPKG_ATTR_AUTHENTICATION_ID			20
#define SECPKG_ATTR_LOGOFF_TIME				21
#define SECPKG_ATTR_NEGO_KEYS				22
#define SECPKG_ATTR_PROMPTING_NEEDED			24
#define SECPKG_ATTR_UNIQUE_BINDINGS			25
#define SECPKG_ATTR_ENDPOINT_BINDINGS			26
#define SECPKG_ATTR_CLIENT_SPECIFIED_TARGET		27
#define SECPKG_ATTR_LAST_CLIENT_TOKEN_STATUS		30
#define SECPKG_ATTR_NEGO_PKG_INFO			31
#define SECPKG_ATTR_NEGO_STATUS				32
#define SECPKG_ATTR_CONTEXT_DELETED			33

#if !defined(_WIN32) || defined(_UWP)

struct _SecPkgContext_AccessToken
{
	void* AccessToken;
};
typedef struct _SecPkgContext_AccessToken SecPkgContext_AccessToken;

struct _SecPkgContext_SessionAppData
{
	UINT32 dwFlags;
	UINT32 cbAppData;
	BYTE* pbAppData;
};
typedef struct _SecPkgContext_SessionAppData SecPkgContext_SessionAppData;

struct _SecPkgContext_Authority
{
	char* sAuthorityName;
};
typedef struct _SecPkgContext_Authority SecPkgContext_Authority;

struct _SecPkgContext_ClientSpecifiedTarget
{
	char* sTargetName;
};
typedef struct _SecPkgContext_ClientSpecifiedTarget SecPkgContext_ClientSpecifiedTarget;

typedef UINT32 ALG_ID;

struct _SecPkgContext_ConnectionInfo
{
	UINT32 dwProtocol;
	ALG_ID aiCipher;
	UINT32 dwCipherStrength;
	ALG_ID aiHash;
	UINT32 dwHashStrength;
	ALG_ID aiExch;
	UINT32 dwExchStrength;
};
typedef struct _SecPkgContext_ConnectionInfo SecPkgContext_ConnectionInfo;

struct _SecPkgContext_ClientCreds
{
	UINT32 AuthBufferLen;
	BYTE* AuthBuffer;
};
typedef struct _SecPkgContext_ClientCreds SecPkgContext_ClientCreds;

struct _SecPkgContex_DceInfo
{
	UINT32 AuthzSvc;
	void* pPac;
};
typedef struct _SecPkgContex_DceInfo SecPkgContex_DceInfo;

struct _SEC_CHANNEL_BINDINGS
{
	UINT32 dwInitiatorAddrType;
	UINT32 cbInitiatorLength;
	UINT32 dwInitiatorOffset;
	UINT32 dwAcceptorAddrType;
	UINT32 cbAcceptorLength;
	UINT32 dwAcceptorOffset;
	UINT32 cbApplicationDataLength;
	UINT32 dwApplicationDataOffset;
};
typedef struct _SEC_CHANNEL_BINDINGS SEC_CHANNEL_BINDINGS;

struct _SecPkgContext_Bindings
{
	UINT32 BindingsLength;
	SEC_CHANNEL_BINDINGS* Bindings;
};
typedef struct _SecPkgContext_Bindings SecPkgContext_Bindings;

struct _SecPkgContext_EapKeyBlock
{
	BYTE rgbKeys[128];
	BYTE rgbIVs[64];
};
typedef struct _SecPkgContext_EapKeyBlock SecPkgContext_EapKeyBlock;

struct _SecPkgContext_Flags
{
	UINT32 Flags;
};
typedef struct _SecPkgContext_Flags SecPkgContext_Flags;

struct _SecPkgContext_KeyInfo
{
	char* sSignatureAlgorithmName;
	char* sEncryptAlgorithmName;
	UINT32 KeySize;
	UINT32 SignatureAlgorithm;
	UINT32 EncryptAlgorithm;
};
typedef struct _SecPkgContext_KeyInfo SecPkgContext_KeyInfo;

struct _SecPkgContext_Lifespan
{
	TimeStamp tsStart;
	TimeStamp tsExpiry;
};
typedef struct _SecPkgContext_Lifespan SecPkgContext_Lifespan;

struct _SecPkgContext_Names
{
	char* sUserName;
};
typedef struct _SecPkgContext_Names SecPkgContext_Names;

struct _SecPkgContext_NativeNames
{
	char* sClientName;
	char* sServerName;
};
typedef struct _SecPkgContext_NativeNames SecPkgContext_NativeNames;

struct _SecPkgContext_NegotiationInfo
{
	SecPkgInfo* PackageInfo;
	UINT32 NegotiationState;
};
typedef struct _SecPkgContext_NegotiationInfo SecPkgContext_NegotiationInfo;

struct _SecPkgContext_PackageInfo
{
	SecPkgInfo* PackageInfo;
};
typedef struct _SecPkgContext_PackageInfo SecPkgContext_PackageInfo;

struct _SecPkgContext_PasswordExpiry
{
	TimeStamp tsPasswordExpires;
};
typedef struct _SecPkgContext_PasswordExpiry SecPkgContext_PasswordExpiry;

struct _SecPkgContext_SessionKey
{
	UINT32 SessionKeyLength;
	BYTE* SessionKey;
};
typedef struct _SecPkgContext_SessionKey SecPkgContext_SessionKey;

struct _SecPkgContext_SessionInfo
{
	UINT32 dwFlags;
	UINT32 cbSessionId;
	BYTE rgbSessionId[32];
};
typedef struct _SecPkgContext_SessionInfo SecPkgContext_SessionInfo;

struct _SecPkgContext_Sizes
{
	UINT32 cbMaxToken;
	UINT32 cbMaxSignature;
	UINT32 cbBlockSize;
	UINT32 cbSecurityTrailer;
};
typedef struct _SecPkgContext_Sizes SecPkgContext_Sizes;

struct _SecPkgContext_StreamSizes
{
	UINT32 cbHeader;
	UINT32 cbTrailer;
	UINT32 cbMaximumMessage;
	UINT32 cBuffers;
	UINT32 cbBlockSize;
};
typedef struct _SecPkgContext_StreamSizes SecPkgContext_StreamSizes;

struct _SecPkgContext_SubjectAttributes
{
	void* AttributeInfo;
};
typedef struct _SecPkgContext_SubjectAttributes SecPkgContext_SubjectAttributes;

struct _SecPkgContext_SupportedSignatures
{
	UINT16 cSignatureAndHashAlgorithms;
	UINT16* pSignatureAndHashAlgorithms;
};
typedef struct _SecPkgContext_SupportedSignatures SecPkgContext_SupportedSignatures;

struct _SecPkgContext_TargetInformation
{
	UINT32 MarshalledTargetInfoLength;
	BYTE* MarshalledTargetInfo;
};
typedef struct _SecPkgContext_TargetInformation SecPkgContext_TargetInformation;

/* Security Credentials Attributes */

#define SECPKG_CRED_ATTR_NAMES				1

struct _SecPkgCredentials_NamesA
{
	SEC_CHAR* sUserName;
};
typedef struct _SecPkgCredentials_NamesA SecPkgCredentials_NamesA;
typedef SecPkgCredentials_NamesA* PSecPkgCredentials_NamesA;

struct _SecPkgCredentials_NamesW
{
	SEC_WCHAR* sUserName;
};
typedef struct _SecPkgCredentials_NamesW SecPkgCredentials_NamesW;
typedef SecPkgCredentials_NamesW* PSecPkgCredentials_NamesW;

#ifdef UNICODE
#define SecPkgCredentials_Names SecPkgCredentials_NamesW
#define PSecPkgCredentials_Names PSecPkgCredentials_NamesW
#else
#define SecPkgCredentials_Names SecPkgCredentials_NamesA
#define PSecPkgCredentials_Names PSecPkgCredentials_NamesA
#endif

#endif

/* InitializeSecurityContext Flags */

#define ISC_REQ_DELEGATE			0x00000001
#define ISC_REQ_MUTUAL_AUTH			0x00000002
#define ISC_REQ_REPLAY_DETECT			0x00000004
#define ISC_REQ_SEQUENCE_DETECT			0x00000008
#define ISC_REQ_CONFIDENTIALITY			0x00000010
#define ISC_REQ_USE_SESSION_KEY			0x00000020
#define ISC_REQ_PROMPT_FOR_CREDS		0x00000040
#define ISC_REQ_USE_SUPPLIED_CREDS		0x00000080
#define ISC_REQ_ALLOCATE_MEMORY			0x00000100
#define ISC_REQ_USE_DCE_STYLE			0x00000200
#define ISC_REQ_DATAGRAM			0x00000400
#define ISC_REQ_CONNECTION			0x00000800
#define ISC_REQ_CALL_LEVEL			0x00001000
#define ISC_REQ_FRAGMENT_SUPPLIED		0x00002000
#define ISC_REQ_EXTENDED_ERROR			0x00004000
#define ISC_REQ_STREAM				0x00008000
#define ISC_REQ_INTEGRITY			0x00010000
#define ISC_REQ_IDENTIFY			0x00020000
#define ISC_REQ_NULL_SESSION			0x00040000
#define ISC_REQ_MANUAL_CRED_VALIDATION		0x00080000
#define ISC_REQ_RESERVED1			0x00100000
#define ISC_REQ_FRAGMENT_TO_FIT			0x00200000
#define ISC_REQ_FORWARD_CREDENTIALS		0x00400000
#define ISC_REQ_NO_INTEGRITY			0x00800000
#define ISC_REQ_USE_HTTP_STYLE			0x01000000

#define ISC_RET_DELEGATE			0x00000001
#define ISC_RET_MUTUAL_AUTH			0x00000002
#define ISC_RET_REPLAY_DETECT			0x00000004
#define ISC_RET_SEQUENCE_DETECT			0x00000008
#define ISC_RET_CONFIDENTIALITY			0x00000010
#define ISC_RET_USE_SESSION_KEY			0x00000020
#define ISC_RET_USED_COLLECTED_CREDS		0x00000040
#define ISC_RET_USED_SUPPLIED_CREDS		0x00000080
#define ISC_RET_ALLOCATED_MEMORY		0x00000100
#define ISC_RET_USED_DCE_STYLE			0x00000200
#define ISC_RET_DATAGRAM			0x00000400
#define ISC_RET_CONNECTION			0x00000800
#define ISC_RET_INTERMEDIATE_RETURN		0x00001000
#define ISC_RET_CALL_LEVEL			0x00002000
#define ISC_RET_EXTENDED_ERROR			0x00004000
#define ISC_RET_STREAM				0x00008000
#define ISC_RET_INTEGRITY			0x00010000
#define ISC_RET_IDENTIFY			0x00020000
#define ISC_RET_NULL_SESSION			0x00040000
#define ISC_RET_MANUAL_CRED_VALIDATION		0x00080000
#define ISC_RET_RESERVED1			0x00100000
#define ISC_RET_FRAGMENT_ONLY			0x00200000
#define ISC_RET_FORWARD_CREDENTIALS		0x00400000
#define ISC_RET_USED_HTTP_STYLE			0x01000000

/* AcceptSecurityContext Flags */

#define ASC_REQ_DELEGATE			0x00000001
#define ASC_REQ_MUTUAL_AUTH			0x00000002
#define ASC_REQ_REPLAY_DETECT			0x00000004
#define ASC_REQ_SEQUENCE_DETECT			0x00000008
#define ASC_REQ_CONFIDENTIALITY			0x00000010
#define ASC_REQ_USE_SESSION_KEY			0x00000020
#define ASC_REQ_ALLOCATE_MEMORY			0x00000100
#define ASC_REQ_USE_DCE_STYLE			0x00000200
#define ASC_REQ_DATAGRAM			0x00000400
#define ASC_REQ_CONNECTION			0x00000800
#define ASC_REQ_CALL_LEVEL			0x00001000
#define ASC_REQ_EXTENDED_ERROR			0x00008000
#define ASC_REQ_STREAM				0x00010000
#define ASC_REQ_INTEGRITY			0x00020000
#define ASC_REQ_LICENSING			0x00040000
#define ASC_REQ_IDENTIFY			0x00080000
#define ASC_REQ_ALLOW_NULL_SESSION		0x00100000
#define ASC_REQ_ALLOW_NON_USER_LOGONS		0x00200000
#define ASC_REQ_ALLOW_CONTEXT_REPLAY		0x00400000
#define ASC_REQ_FRAGMENT_TO_FIT			0x00800000
#define ASC_REQ_FRAGMENT_SUPPLIED		0x00002000
#define ASC_REQ_NO_TOKEN			0x01000000
#define ASC_REQ_PROXY_BINDINGS			0x04000000
#define ASC_REQ_ALLOW_MISSING_BINDINGS		0x10000000

#define ASC_RET_DELEGATE			0x00000001
#define ASC_RET_MUTUAL_AUTH			0x00000002
#define ASC_RET_REPLAY_DETECT			0x00000004
#define ASC_RET_SEQUENCE_DETECT			0x00000008
#define ASC_RET_CONFIDENTIALITY			0x00000010
#define ASC_RET_USE_SESSION_KEY			0x00000020
#define ASC_RET_ALLOCATED_MEMORY		0x00000100
#define ASC_RET_USED_DCE_STYLE			0x00000200
#define ASC_RET_DATAGRAM			0x00000400
#define ASC_RET_CONNECTION			0x00000800
#define ASC_RET_CALL_LEVEL			0x00002000
#define ASC_RET_THIRD_LEG_FAILED		0x00004000
#define ASC_RET_EXTENDED_ERROR			0x00008000
#define ASC_RET_STREAM				0x00010000
#define ASC_RET_INTEGRITY			0x00020000
#define ASC_RET_LICENSING			0x00040000
#define ASC_RET_IDENTIFY			0x00080000
#define ASC_RET_NULL_SESSION			0x00100000
#define ASC_RET_ALLOW_NON_USER_LOGONS		0x00200000
#define ASC_RET_FRAGMENT_ONLY			0x00800000
#define ASC_RET_NO_TOKEN			0x01000000
#define ASC_RET_NO_PROXY_BINDINGS		0x04000000
#define ASC_RET_MISSING_BINDINGS		0x10000000

#define SEC_WINNT_AUTH_IDENTITY_ANSI		0x1
#define SEC_WINNT_AUTH_IDENTITY_UNICODE		0x2

#if !defined(_WIN32) || defined(_UWP)

#ifndef _AUTH_IDENTITY_DEFINED
#define _AUTH_IDENTITY_DEFINED

typedef struct _SEC_WINNT_AUTH_IDENTITY_W
{
	/* TSPasswordCreds */
	UINT16* User;
	UINT32 UserLength;
	UINT16* Domain;
	UINT32 DomainLength;
	UINT16* Password;
	UINT32 PasswordLength;
	UINT32 Flags;
} SEC_WINNT_AUTH_IDENTITY_W, *PSEC_WINNT_AUTH_IDENTITY_W;

typedef struct _SEC_WINNT_AUTH_IDENTITY_A
{
	/* TSPasswordCreds */
	BYTE* User;
	UINT32 UserLength;
	BYTE* Domain;
	UINT32 DomainLength;
	BYTE* Password;
	UINT32 PasswordLength;
	UINT32 Flags;
} SEC_WINNT_AUTH_IDENTITY_A, *PSEC_WINNT_AUTH_IDENTITY_A;

struct _SEC_WINNT_AUTH_IDENTITY
{
	/* TSPasswordCreds */
	UINT16* User;
	UINT32 UserLength;
	UINT16* Domain;
	UINT32 DomainLength;
	UINT16* Password;
	UINT32 PasswordLength;
	UINT32 Flags;
};
typedef struct _SEC_WINNT_AUTH_IDENTITY SEC_WINNT_AUTH_IDENTITY;

#endif /* _AUTH_IDENTITY_DEFINED */

struct _SecHandle
{
	ULONG_PTR dwLower;
	ULONG_PTR dwUpper;
};
typedef struct _SecHandle SecHandle;
typedef SecHandle* PSecHandle;

typedef SecHandle CredHandle;
typedef CredHandle* PCredHandle;
typedef SecHandle CtxtHandle;
typedef CtxtHandle* PCtxtHandle;

#define SecInvalidateHandle(x) \
	((PSecHandle)(x))->dwLower = ((PSecHandle)(x))->dwUpper = ((ULONG_PTR)((INT_PTR) - 1));

#define SecIsValidHandle(x) \
	((((PSecHandle)(x))->dwLower != ((ULONG_PTR)((INT_PTR) - 1))) && \
	 (((PSecHandle) (x))->dwUpper != ((ULONG_PTR)((INT_PTR) - 1))))

#endif

#define SECBUFFER_VERSION			0

/* Buffer Types */
#define SECBUFFER_EMPTY				0
#define SECBUFFER_DATA				1
#define SECBUFFER_TOKEN				2
#define SECBUFFER_PKG_PARAMS			3
#define SECBUFFER_MISSING			4
#define SECBUFFER_EXTRA				5
#define SECBUFFER_STREAM_TRAILER		6
#define SECBUFFER_STREAM_HEADER			7
#define SECBUFFER_NEGOTIATION_INFO		8
#define SECBUFFER_PADDING			9
#define SECBUFFER_STREAM			10
#define SECBUFFER_MECHLIST			11
#define SECBUFFER_MECHLIST_SIGNATURE		12
#define SECBUFFER_TARGET			13
#define SECBUFFER_CHANNEL_BINDINGS		14
#define SECBUFFER_CHANGE_PASS_RESPONSE		15
#define SECBUFFER_TARGET_HOST			16
#define SECBUFFER_ALERT				17

/* Security Buffer Flags */
#define SECBUFFER_ATTRMASK			0xF0000000
#define SECBUFFER_READONLY			0x80000000
#define SECBUFFER_READONLY_WITH_CHECKSUM	0x10000000
#define SECBUFFER_RESERVED			0x60000000

#if !defined(_WIN32) || defined(_UWP)

struct _SecBuffer
{
	ULONG cbBuffer;
	ULONG BufferType;
	void* pvBuffer;
};
typedef struct _SecBuffer SecBuffer;
typedef SecBuffer* PSecBuffer;

struct _SecBufferDesc
{
	ULONG ulVersion;
	ULONG cBuffers;
	PSecBuffer pBuffers;
};
typedef struct _SecBufferDesc SecBufferDesc;
typedef SecBufferDesc* PSecBufferDesc;

typedef void (SEC_ENTRY* SEC_GET_KEY_FN)(void* Arg, void* Principal, UINT32 KeyVer, void** Key,
        SECURITY_STATUS* pStatus);

typedef SECURITY_STATUS(SEC_ENTRY* ENUMERATE_SECURITY_PACKAGES_FN_A)(ULONG* pcPackages,
        PSecPkgInfoA* ppPackageInfo);
typedef SECURITY_STATUS(SEC_ENTRY* ENUMERATE_SECURITY_PACKAGES_FN_W)(ULONG* pcPackages,
        PSecPkgInfoW* ppPackageInfo);

#ifdef UNICODE
#define EnumerateSecurityPackages EnumerateSecurityPackagesW
#define ENUMERATE_SECURITY_PACKAGES_FN ENUMERATE_SECURITY_PACKAGES_FN_W
#else
#define EnumerateSecurityPackages EnumerateSecurityPackagesA
#define ENUMERATE_SECURITY_PACKAGES_FN ENUMERATE_SECURITY_PACKAGES_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* QUERY_CREDENTIALS_ATTRIBUTES_FN_A)(PCredHandle phCredential,
        ULONG ulAttribute, void* pBuffer);
typedef SECURITY_STATUS(SEC_ENTRY* QUERY_CREDENTIALS_ATTRIBUTES_FN_W)(PCredHandle phCredential,
        ULONG ulAttribute, void* pBuffer);

#ifdef UNICODE
#define QueryCredentialsAttributes QueryCredentialsAttributesW
#define QUERY_CREDENTIALS_ATTRIBUTES_FN QUERY_CREDENTIALS_ATTRIBUTES_FN_W
#else
#define QueryCredentialsAttributes QueryCredentialsAttributesA
#define QUERY_CREDENTIALS_ATTRIBUTES_FN QUERY_CREDENTIALS_ATTRIBUTES_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* ACQUIRE_CREDENTIALS_HANDLE_FN_A)(LPSTR pszPrincipal,
        LPSTR pszPackage,
        ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
        void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry);
typedef SECURITY_STATUS(SEC_ENTRY* ACQUIRE_CREDENTIALS_HANDLE_FN_W)(LPWSTR pszPrincipal,
        LPWSTR pszPackage,
        ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
        void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry);

#ifdef UNICODE
#define AcquireCredentialsHandle AcquireCredentialsHandleW
#define ACQUIRE_CREDENTIALS_HANDLE_FN ACQUIRE_CREDENTIALS_HANDLE_FN_W
#else
#define AcquireCredentialsHandle AcquireCredentialsHandleA
#define ACQUIRE_CREDENTIALS_HANDLE_FN ACQUIRE_CREDENTIALS_HANDLE_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* FREE_CREDENTIALS_HANDLE_FN)(PCredHandle phCredential);

typedef SECURITY_STATUS(SEC_ENTRY* INITIALIZE_SECURITY_CONTEXT_FN_A)(PCredHandle phCredential,
        PCtxtHandle phContext,
        SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
        PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry);
typedef SECURITY_STATUS(SEC_ENTRY* INITIALIZE_SECURITY_CONTEXT_FN_W)(PCredHandle phCredential,
        PCtxtHandle phContext,
        SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
        PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry);

#ifdef UNICODE
#define InitializeSecurityContext InitializeSecurityContextW
#define INITIALIZE_SECURITY_CONTEXT_FN INITIALIZE_SECURITY_CONTEXT_FN_W
#else
#define InitializeSecurityContext InitializeSecurityContextA
#define INITIALIZE_SECURITY_CONTEXT_FN INITIALIZE_SECURITY_CONTEXT_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* ACCEPT_SECURITY_CONTEXT_FN)(PCredHandle phCredential,
        PCtxtHandle phContext,
        PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp);

typedef SECURITY_STATUS(SEC_ENTRY* COMPLETE_AUTH_TOKEN_FN)(PCtxtHandle phContext,
        PSecBufferDesc pToken);

typedef SECURITY_STATUS(SEC_ENTRY* DELETE_SECURITY_CONTEXT_FN)(PCtxtHandle phContext);

typedef SECURITY_STATUS(SEC_ENTRY* APPLY_CONTROL_TOKEN_FN)(PCtxtHandle phContext,
        PSecBufferDesc pInput);

typedef SECURITY_STATUS(SEC_ENTRY* QUERY_CONTEXT_ATTRIBUTES_FN_A)(PCtxtHandle phContext,
        ULONG ulAttribute, void* pBuffer);
typedef SECURITY_STATUS(SEC_ENTRY* QUERY_CONTEXT_ATTRIBUTES_FN_W)(PCtxtHandle phContext,
        ULONG ulAttribute, void* pBuffer);

#ifdef UNICODE
#define QueryContextAttributes QueryContextAttributesW
#define QUERY_CONTEXT_ATTRIBUTES_FN QUERY_CONTEXT_ATTRIBUTES_FN_W
#else
#define QueryContextAttributes QueryContextAttributesA
#define QUERY_CONTEXT_ATTRIBUTES_FN QUERY_CONTEXT_ATTRIBUTES_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* IMPERSONATE_SECURITY_CONTEXT_FN)(PCtxtHandle phContext);

typedef SECURITY_STATUS(SEC_ENTRY* REVERT_SECURITY_CONTEXT_FN)(PCtxtHandle phContext);

typedef SECURITY_STATUS(SEC_ENTRY* MAKE_SIGNATURE_FN)(PCtxtHandle phContext, ULONG fQOP,
        PSecBufferDesc pMessage, ULONG MessageSeqNo);

typedef SECURITY_STATUS(SEC_ENTRY* VERIFY_SIGNATURE_FN)(PCtxtHandle phContext,
        PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP);

typedef SECURITY_STATUS(SEC_ENTRY* FREE_CONTEXT_BUFFER_FN)(void* pvContextBuffer);

typedef SECURITY_STATUS(SEC_ENTRY* QUERY_SECURITY_PACKAGE_INFO_FN_A)(SEC_CHAR* pszPackageName,
        PSecPkgInfoA* ppPackageInfo);
typedef SECURITY_STATUS(SEC_ENTRY* QUERY_SECURITY_PACKAGE_INFO_FN_W)(SEC_WCHAR* pszPackageName,
        PSecPkgInfoW* ppPackageInfo);

#ifdef UNICODE
#define QuerySecurityPackageInfo QuerySecurityPackageInfoW
#define QUERY_SECURITY_PACKAGE_INFO_FN QUERY_SECURITY_PACKAGE_INFO_FN_W
#else
#define QuerySecurityPackageInfo QuerySecurityPackageInfoA
#define QUERY_SECURITY_PACKAGE_INFO_FN QUERY_SECURITY_PACKAGE_INFO_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* EXPORT_SECURITY_CONTEXT_FN)(PCtxtHandle phContext, ULONG fFlags,
        PSecBuffer pPackedContext, HANDLE* pToken);

typedef SECURITY_STATUS(SEC_ENTRY* IMPORT_SECURITY_CONTEXT_FN_A)(SEC_CHAR* pszPackage,
        PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext);
typedef SECURITY_STATUS(SEC_ENTRY* IMPORT_SECURITY_CONTEXT_FN_W)(SEC_WCHAR* pszPackage,
        PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext);

#ifdef UNICODE
#define ImportSecurityContext ImportSecurityContextW
#define IMPORT_SECURITY_CONTEXT_FN IMPORT_SECURITY_CONTEXT_FN_W
#else
#define ImportSecurityContext ImportSecurityContextA
#define IMPORT_SECURITY_CONTEXT_FN IMPORT_SECURITY_CONTEXT_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* ADD_CREDENTIALS_FN_A)(PCredHandle hCredentials,
        SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
        UINT32 fCredentialUse, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument,
        PTimeStamp ptsExpiry);
typedef SECURITY_STATUS(SEC_ENTRY* ADD_CREDENTIALS_FN_W)(PCredHandle hCredentials,
        SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
        UINT32 fCredentialUse, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument,
        PTimeStamp ptsExpiry);

#ifdef UNICODE
#define AddCredentials AddCredentialsW
#define ADD_CREDENTIALS_FN ADD_CREDENTIALS_FN_W
#else
#define AddCredentials AddCredentialsA
#define ADD_CREDENTIALS_FN ADD_CREDENTIALS_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* QUERY_SECURITY_CONTEXT_TOKEN_FN)(PCtxtHandle phContext,
        HANDLE* phToken);

typedef SECURITY_STATUS(SEC_ENTRY* ENCRYPT_MESSAGE_FN)(PCtxtHandle phContext, ULONG fQOP,
        PSecBufferDesc pMessage, ULONG MessageSeqNo);

typedef SECURITY_STATUS(SEC_ENTRY* DECRYPT_MESSAGE_FN)(PCtxtHandle phContext,
        PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP);

typedef SECURITY_STATUS(SEC_ENTRY* SET_CONTEXT_ATTRIBUTES_FN_A)(PCtxtHandle phContext,
        ULONG ulAttribute, void* pBuffer, ULONG cbBuffer);
typedef SECURITY_STATUS(SEC_ENTRY* SET_CONTEXT_ATTRIBUTES_FN_W)(PCtxtHandle phContext,
        ULONG ulAttribute, void* pBuffer, ULONG cbBuffer);

#ifdef UNICODE
#define SetContextAttributes SetContextAttributesW
#define SET_CONTEXT_ATTRIBUTES_FN SET_CONTEXT_ATTRIBUTES_FN_W
#else
#define SetContextAttributes SetContextAttributesA
#define SET_CONTEXT_ATTRIBUTES_FN SET_CONTEXT_ATTRIBUTES_FN_A
#endif

#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION	1 /* Interface has all routines through DecryptMessage */
#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION_2	2 /* Interface has all routines through SetContextAttributes */
#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION_3	3 /* Interface has all routines through SetCredentialsAttributes */
#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION_4	4 /* Interface has all routines through ChangeAccountPassword */

struct _SecurityFunctionTableA
{
	UINT32 dwVersion;
	ENUMERATE_SECURITY_PACKAGES_FN_A EnumerateSecurityPackagesA;
	QUERY_CREDENTIALS_ATTRIBUTES_FN_A QueryCredentialsAttributesA;
	ACQUIRE_CREDENTIALS_HANDLE_FN_A AcquireCredentialsHandleA;
	FREE_CREDENTIALS_HANDLE_FN FreeCredentialsHandle;
	void* Reserved2;
	INITIALIZE_SECURITY_CONTEXT_FN_A InitializeSecurityContextA;
	ACCEPT_SECURITY_CONTEXT_FN AcceptSecurityContext;
	COMPLETE_AUTH_TOKEN_FN CompleteAuthToken;
	DELETE_SECURITY_CONTEXT_FN DeleteSecurityContext;
	APPLY_CONTROL_TOKEN_FN ApplyControlToken;
	QUERY_CONTEXT_ATTRIBUTES_FN_A QueryContextAttributesA;
	IMPERSONATE_SECURITY_CONTEXT_FN ImpersonateSecurityContext;
	REVERT_SECURITY_CONTEXT_FN RevertSecurityContext;
	MAKE_SIGNATURE_FN MakeSignature;
	VERIFY_SIGNATURE_FN VerifySignature;
	FREE_CONTEXT_BUFFER_FN FreeContextBuffer;
	QUERY_SECURITY_PACKAGE_INFO_FN_A QuerySecurityPackageInfoA;
	void* Reserved3;
	void* Reserved4;
	EXPORT_SECURITY_CONTEXT_FN ExportSecurityContext;
	IMPORT_SECURITY_CONTEXT_FN_A ImportSecurityContextA;
	ADD_CREDENTIALS_FN_A AddCredentialsA;
	void* Reserved8;
	QUERY_SECURITY_CONTEXT_TOKEN_FN QuerySecurityContextToken;
	ENCRYPT_MESSAGE_FN EncryptMessage;
	DECRYPT_MESSAGE_FN DecryptMessage;
	SET_CONTEXT_ATTRIBUTES_FN_A SetContextAttributesA;
};
typedef struct _SecurityFunctionTableA SecurityFunctionTableA;
typedef SecurityFunctionTableA* PSecurityFunctionTableA;

struct _SecurityFunctionTableW
{
	UINT32 dwVersion;
	ENUMERATE_SECURITY_PACKAGES_FN_W EnumerateSecurityPackagesW;
	QUERY_CREDENTIALS_ATTRIBUTES_FN_W QueryCredentialsAttributesW;
	ACQUIRE_CREDENTIALS_HANDLE_FN_W AcquireCredentialsHandleW;
	FREE_CREDENTIALS_HANDLE_FN FreeCredentialsHandle;
	void* Reserved2;
	INITIALIZE_SECURITY_CONTEXT_FN_W InitializeSecurityContextW;
	ACCEPT_SECURITY_CONTEXT_FN AcceptSecurityContext;
	COMPLETE_AUTH_TOKEN_FN CompleteAuthToken;
	DELETE_SECURITY_CONTEXT_FN DeleteSecurityContext;
	APPLY_CONTROL_TOKEN_FN ApplyControlToken;
	QUERY_CONTEXT_ATTRIBUTES_FN_W QueryContextAttributesW;
	IMPERSONATE_SECURITY_CONTEXT_FN ImpersonateSecurityContext;
	REVERT_SECURITY_CONTEXT_FN RevertSecurityContext;
	MAKE_SIGNATURE_FN MakeSignature;
	VERIFY_SIGNATURE_FN VerifySignature;
	FREE_CONTEXT_BUFFER_FN FreeContextBuffer;
	QUERY_SECURITY_PACKAGE_INFO_FN_W QuerySecurityPackageInfoW;
	void* Reserved3;
	void* Reserved4;
	EXPORT_SECURITY_CONTEXT_FN ExportSecurityContext;
	IMPORT_SECURITY_CONTEXT_FN_W ImportSecurityContextW;
	ADD_CREDENTIALS_FN_W AddCredentialsW;
	void* Reserved8;
	QUERY_SECURITY_CONTEXT_TOKEN_FN QuerySecurityContextToken;
	ENCRYPT_MESSAGE_FN EncryptMessage;
	DECRYPT_MESSAGE_FN DecryptMessage;
	SET_CONTEXT_ATTRIBUTES_FN_W SetContextAttributesW;
};
typedef struct _SecurityFunctionTableW SecurityFunctionTableW;
typedef SecurityFunctionTableW* PSecurityFunctionTableW;

typedef PSecurityFunctionTableA(SEC_ENTRY* INIT_SECURITY_INTERFACE_A)(void);
typedef PSecurityFunctionTableW(SEC_ENTRY* INIT_SECURITY_INTERFACE_W)(void);

#ifdef UNICODE
#define InitSecurityInterface InitSecurityInterfaceW
#define SecurityFunctionTable SecurityFunctionTableW
#define PSecurityFunctionTable PSecurityFunctionTableW
#define INIT_SECURITY_INTERFACE INIT_SECURITY_INTERFACE_W
#else
#define InitSecurityInterface InitSecurityInterfaceA
#define SecurityFunctionTable SecurityFunctionTableA
#define PSecurityFunctionTable PSecurityFunctionTableA
#define INIT_SECURITY_INTERFACE INIT_SECURITY_INTERFACE_A
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Package Management */

WINPR_API SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesA(ULONG* pcPackages,
        PSecPkgInfoA* ppPackageInfo);
WINPR_API SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesW(ULONG* pcPackages,
        PSecPkgInfoW* ppPackageInfo);

WINPR_API PSecurityFunctionTableA SEC_ENTRY InitSecurityInterfaceA(void);
WINPR_API PSecurityFunctionTableW SEC_ENTRY InitSecurityInterfaceW(void);

WINPR_API SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoA(SEC_CHAR* pszPackageName,
        PSecPkgInfoA* ppPackageInfo);
WINPR_API SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoW(SEC_WCHAR* pszPackageName,
        PSecPkgInfoW* ppPackageInfo);

/* Credential Management */

WINPR_API SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal,
        SEC_CHAR* pszPackage,
        ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
        void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry);
WINPR_API SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal,
        SEC_WCHAR* pszPackage,
        ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
        void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry);

WINPR_API SECURITY_STATUS SEC_ENTRY ExportSecurityContext(PCtxtHandle phContext, ULONG fFlags,
        PSecBuffer pPackedContext, HANDLE* pToken);
WINPR_API SECURITY_STATUS SEC_ENTRY FreeCredentialsHandle(PCredHandle phCredential);

WINPR_API SECURITY_STATUS SEC_ENTRY ImportSecurityContextA(SEC_CHAR* pszPackage,
        PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext);
WINPR_API SECURITY_STATUS SEC_ENTRY ImportSecurityContextW(SEC_WCHAR* pszPackage,
        PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext);

WINPR_API SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesA(PCredHandle phCredential,
        ULONG ulAttribute, void* pBuffer);
WINPR_API SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesW(PCredHandle phCredential,
        ULONG ulAttribute, void* pBuffer);

/* Context Management */

WINPR_API SECURITY_STATUS SEC_ENTRY AcceptSecurityContext(PCredHandle phCredential,
        PCtxtHandle phContext,
        PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp);

WINPR_API SECURITY_STATUS SEC_ENTRY ApplyControlToken(PCtxtHandle phContext, PSecBufferDesc pInput);
WINPR_API SECURITY_STATUS SEC_ENTRY CompleteAuthToken(PCtxtHandle phContext, PSecBufferDesc pToken);
WINPR_API SECURITY_STATUS SEC_ENTRY DeleteSecurityContext(PCtxtHandle phContext);
WINPR_API SECURITY_STATUS SEC_ENTRY FreeContextBuffer(void* pvContextBuffer);
WINPR_API SECURITY_STATUS SEC_ENTRY ImpersonateSecurityContext(PCtxtHandle phContext);

WINPR_API SECURITY_STATUS SEC_ENTRY InitializeSecurityContextA(PCredHandle phCredential,
        PCtxtHandle phContext,
        SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
        PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry);
WINPR_API SECURITY_STATUS SEC_ENTRY InitializeSecurityContextW(PCredHandle phCredential,
        PCtxtHandle phContext,
        SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
        PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry);

WINPR_API SECURITY_STATUS SEC_ENTRY QueryContextAttributes(PCtxtHandle phContext, ULONG ulAttribute,
        void* pBuffer);
WINPR_API SECURITY_STATUS SEC_ENTRY QuerySecurityContextToken(PCtxtHandle phContext,
        HANDLE* phToken);
WINPR_API SECURITY_STATUS SEC_ENTRY SetContextAttributes(PCtxtHandle phContext, ULONG ulAttribute,
        void* pBuffer, ULONG cbBuffer);
WINPR_API SECURITY_STATUS SEC_ENTRY RevertSecurityContext(PCtxtHandle phContext);

/* Message Support */

WINPR_API SECURITY_STATUS SEC_ENTRY DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage,
        ULONG MessageSeqNo, PULONG pfQOP);
WINPR_API SECURITY_STATUS SEC_ENTRY EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
        PSecBufferDesc pMessage, ULONG MessageSeqNo);
WINPR_API SECURITY_STATUS SEC_ENTRY MakeSignature(PCtxtHandle phContext, ULONG fQOP,
        PSecBufferDesc pMessage, ULONG MessageSeqNo);
WINPR_API SECURITY_STATUS SEC_ENTRY VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage,
        ULONG MessageSeqNo, PULONG pfQOP);

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Custom API */

#define SECPKG_ATTR_AUTH_IDENTITY			1001
#define SECPKG_ATTR_AUTH_PASSWORD			1002
#define SECPKG_ATTR_AUTH_NTLM_HASH			1003
#define SECPKG_ATTR_AUTH_NTLM_SAM_FILE			1004
#define SECPKG_ATTR_AUTH_NTLM_MESSAGE			1100
#define SECPKG_ATTR_AUTH_NTLM_TIMESTAMP			1101
#define SECPKG_ATTR_AUTH_NTLM_CLIENT_CHALLENGE		1102
#define SECPKG_ATTR_AUTH_NTLM_SERVER_CHALLENGE		1103
#define SECPKG_ATTR_AUTH_NTLM_NTPROOF_VALUE		1104
#define SECPKG_ATTR_AUTH_NTLM_RANDKEY			1105
#define SECPKG_ATTR_AUTH_NTLM_MIC			1106
#define SECPKG_ATTR_AUTH_NTLM_MIC_VALUE			1107
#define SECPKG_ATTR_AUTH_NTLM_HASH_CB			1108
#define SECPKG_ATTR_AUTH_NTLM_HASH_CB_DATA		1109


struct _SecPkgContext_AuthIdentity
{
	char User[256 + 1];
	char Domain[256 + 1];
};
typedef struct _SecPkgContext_AuthIdentity SecPkgContext_AuthIdentity;

struct _SecPkgContext_AuthPassword
{
	char Password[256 + 1];
};
typedef struct _SecPkgContext_AuthPassword SecPkgContext_AuthPassword;

struct _SecPkgContext_AuthNtlmHash
{
	int Version;
	BYTE NtlmHash[16];
};
typedef struct _SecPkgContext_AuthNtlmHash SecPkgContext_AuthNtlmHash;

struct _SecPkgContext_AuthNtlmTimestamp
{
	BYTE Timestamp[8];
	BOOL ChallengeOrResponse;
};
typedef struct _SecPkgContext_AuthNtlmTimestamp SecPkgContext_AuthNtlmTimestamp;

struct _SecPkgContext_AuthNtlmClientChallenge
{
	BYTE ClientChallenge[8];
};
typedef struct _SecPkgContext_AuthNtlmClientChallenge SecPkgContext_AuthNtlmClientChallenge;

struct _SecPkgContext_AuthNtlmServerChallenge
{
	BYTE ServerChallenge[8];
};
typedef struct _SecPkgContext_AuthNtlmServerChallenge SecPkgContext_AuthNtlmServerChallenge;

struct _SecPkgContext_AuthNtlmMessage
{
	UINT32 type;
	UINT32 length;
	BYTE* buffer;
};
typedef struct _SecPkgContext_AuthNtlmMessage SecPkgContext_AuthNtlmMessage;

#define SSPI_INTERFACE_WINPR	0x00000001
#define SSPI_INTERFACE_NATIVE	0x00000002

typedef PSecurityFunctionTableA(SEC_ENTRY* INIT_SECURITY_INTERFACE_EX_A)(DWORD flags);
typedef PSecurityFunctionTableW(SEC_ENTRY* INIT_SECURITY_INTERFACE_EX_W)(DWORD flags);

WINPR_API void sspi_GlobalInit(void);
WINPR_API void sspi_GlobalFinish(void);

WINPR_API void* sspi_SecBufferAlloc(PSecBuffer SecBuffer, ULONG size);
WINPR_API void sspi_SecBufferFree(PSecBuffer SecBuffer);

WINPR_API int sspi_SetAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity, const char* user,
                                   const char* domain, const char* password);
WINPR_API int sspi_CopyAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity,
                                    SEC_WINNT_AUTH_IDENTITY* srcIdentity);

WINPR_API const char* GetSecurityStatusString(SECURITY_STATUS status);

WINPR_API SecurityFunctionTableW* SEC_ENTRY InitSecurityInterfaceExW(DWORD flags);
WINPR_API SecurityFunctionTableA* SEC_ENTRY InitSecurityInterfaceExA(DWORD flags);

#ifdef UNICODE
#define InitSecurityInterfaceEx InitSecurityInterfaceExW
#define INIT_SECURITY_INTERFACE_EX INIT_SECURITY_INTERFACE_EX_W
#else
#define InitSecurityInterfaceEx InitSecurityInterfaceExA
#define INIT_SECURITY_INTERFACE_EX INIT_SECURITY_INTERFACE_EX_A
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_SSPI_H */
