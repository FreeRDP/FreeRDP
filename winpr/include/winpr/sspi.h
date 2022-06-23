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

#endif /* _WIN32 */

#if !defined(_WIN32) || defined(_UWP)

#ifndef SEC_ENTRY
#define SEC_ENTRY
#endif /* SEC_ENTRY */

typedef CHAR SEC_CHAR;
typedef WCHAR SEC_WCHAR;

typedef struct
{
	UINT32 LowPart;
	INT32 HighPart;
} SECURITY_INTEGER;

typedef SECURITY_INTEGER TimeStamp;
typedef SECURITY_INTEGER* PTimeStamp;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

#ifndef __SECSTATUS_DEFINED__
typedef LONG SECURITY_STATUS;
#define __SECSTATUS_DEFINED__
#endif /* __SECSTATUS_DEFINED__ */

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

typedef struct
{
	UINT32 fCapabilities;
	UINT16 wVersion;
	UINT16 wRPCID;
	UINT32 cbMaxToken;
	SEC_CHAR* Name;
	SEC_CHAR* Comment;
} SecPkgInfoA;
typedef SecPkgInfoA* PSecPkgInfoA;

typedef struct
{
	UINT32 fCapabilities;
	UINT16 wVersion;
	UINT16 wRPCID;
	UINT32 cbMaxToken;
	SEC_WCHAR* Name;
	SEC_WCHAR* Comment;
} SecPkgInfoW;
typedef SecPkgInfoW* PSecPkgInfoW;

#ifdef UNICODE
#define SecPkgInfo SecPkgInfoW
#define PSecPkgInfo PSecPkgInfoW
#else
#define SecPkgInfo SecPkgInfoA
#define PSecPkgInfo PSecPkgInfoA
#endif /* UNICODE */

#endif /* !defined(_WIN32) || defined(_UWP) */

#define NTLM_SSP_NAME _T("NTLM")
#define KERBEROS_SSP_NAME _T("Kerberos")
#define NEGO_SSP_NAME _T("Negotiate")

#define SECPKG_ID_NONE 0xFFFF

#define SECPKG_FLAG_INTEGRITY 0x00000001
#define SECPKG_FLAG_PRIVACY 0x00000002
#define SECPKG_FLAG_TOKEN_ONLY 0x00000004
#define SECPKG_FLAG_DATAGRAM 0x00000008
#define SECPKG_FLAG_CONNECTION 0x00000010
#define SECPKG_FLAG_MULTI_REQUIRED 0x00000020
#define SECPKG_FLAG_CLIENT_ONLY 0x00000040
#define SECPKG_FLAG_EXTENDED_ERROR 0x00000080
#define SECPKG_FLAG_IMPERSONATION 0x00000100
#define SECPKG_FLAG_ACCEPT_WIN32_NAME 0x00000200
#define SECPKG_FLAG_STREAM 0x00000400
#define SECPKG_FLAG_NEGOTIABLE 0x00000800
#define SECPKG_FLAG_GSS_COMPATIBLE 0x00001000
#define SECPKG_FLAG_LOGON 0x00002000
#define SECPKG_FLAG_ASCII_BUFFERS 0x00004000
#define SECPKG_FLAG_FRAGMENT 0x00008000
#define SECPKG_FLAG_MUTUAL_AUTH 0x00010000
#define SECPKG_FLAG_DELEGATION 0x00020000
#define SECPKG_FLAG_READONLY_WITH_CHECKSUM 0x00040000
#define SECPKG_FLAG_RESTRICTED_TOKENS 0x00080000
#define SECPKG_FLAG_NEGO_EXTENDER 0x00100000
#define SECPKG_FLAG_NEGOTIABLE2 0x00200000

#ifndef _WINERROR_

#define SEC_E_OK (SECURITY_STATUS)0x00000000L
#define SEC_E_INSUFFICIENT_MEMORY (SECURITY_STATUS)0x80090300L
#define SEC_E_INVALID_HANDLE (SECURITY_STATUS)0x80090301L
#define SEC_E_UNSUPPORTED_FUNCTION (SECURITY_STATUS)0x80090302L
#define SEC_E_TARGET_UNKNOWN (SECURITY_STATUS)0x80090303L
#define SEC_E_INTERNAL_ERROR (SECURITY_STATUS)0x80090304L
#define SEC_E_SECPKG_NOT_FOUND (SECURITY_STATUS)0x80090305L
#define SEC_E_NOT_OWNER (SECURITY_STATUS)0x80090306L
#define SEC_E_CANNOT_INSTALL (SECURITY_STATUS)0x80090307L
#define SEC_E_INVALID_TOKEN (SECURITY_STATUS)0x80090308L
#define SEC_E_CANNOT_PACK (SECURITY_STATUS)0x80090309L
#define SEC_E_QOP_NOT_SUPPORTED (SECURITY_STATUS)0x8009030AL
#define SEC_E_NO_IMPERSONATION (SECURITY_STATUS)0x8009030BL
#define SEC_E_LOGON_DENIED (SECURITY_STATUS)0x8009030CL
#define SEC_E_UNKNOWN_CREDENTIALS (SECURITY_STATUS)0x8009030DL
#define SEC_E_NO_CREDENTIALS (SECURITY_STATUS)0x8009030EL
#define SEC_E_MESSAGE_ALTERED (SECURITY_STATUS)0x8009030FL
#define SEC_E_OUT_OF_SEQUENCE (SECURITY_STATUS)0x80090310L
#define SEC_E_NO_AUTHENTICATING_AUTHORITY (SECURITY_STATUS)0x80090311L
#define SEC_E_BAD_PKGID (SECURITY_STATUS)0x80090316L
#define SEC_E_CONTEXT_EXPIRED (SECURITY_STATUS)0x80090317L
#define SEC_E_INCOMPLETE_MESSAGE (SECURITY_STATUS)0x80090318L
#define SEC_E_INCOMPLETE_CREDENTIALS (SECURITY_STATUS)0x80090320L
#define SEC_E_BUFFER_TOO_SMALL (SECURITY_STATUS)0x80090321L
#define SEC_E_WRONG_PRINCIPAL (SECURITY_STATUS)0x80090322L
#define SEC_E_TIME_SKEW (SECURITY_STATUS)0x80090324L
#define SEC_E_UNTRUSTED_ROOT (SECURITY_STATUS)0x80090325L
#define SEC_E_ILLEGAL_MESSAGE (SECURITY_STATUS)0x80090326L
#define SEC_E_CERT_UNKNOWN (SECURITY_STATUS)0x80090327L
#define SEC_E_CERT_EXPIRED (SECURITY_STATUS)0x80090328L
#define SEC_E_ENCRYPT_FAILURE (SECURITY_STATUS)0x80090329L
#define SEC_E_DECRYPT_FAILURE (SECURITY_STATUS)0x80090330L
#define SEC_E_ALGORITHM_MISMATCH (SECURITY_STATUS)0x80090331L
#define SEC_E_SECURITY_QOS_FAILED (SECURITY_STATUS)0x80090332L
#define SEC_E_UNFINISHED_CONTEXT_DELETED (SECURITY_STATUS)0x80090333L
#define SEC_E_NO_TGT_REPLY (SECURITY_STATUS)0x80090334L
#define SEC_E_NO_IP_ADDRESSES (SECURITY_STATUS)0x80090335L
#define SEC_E_WRONG_CREDENTIAL_HANDLE (SECURITY_STATUS)0x80090336L
#define SEC_E_CRYPTO_SYSTEM_INVALID (SECURITY_STATUS)0x80090337L
#define SEC_E_MAX_REFERRALS_EXCEEDED (SECURITY_STATUS)0x80090338L
#define SEC_E_MUST_BE_KDC (SECURITY_STATUS)0x80090339L
#define SEC_E_STRONG_CRYPTO_NOT_SUPPORTED (SECURITY_STATUS)0x8009033AL
#define SEC_E_TOO_MANY_PRINCIPALS (SECURITY_STATUS)0x8009033BL
#define SEC_E_NO_PA_DATA (SECURITY_STATUS)0x8009033CL
#define SEC_E_PKINIT_NAME_MISMATCH (SECURITY_STATUS)0x8009033DL
#define SEC_E_SMARTCARD_LOGON_REQUIRED (SECURITY_STATUS)0x8009033EL
#define SEC_E_SHUTDOWN_IN_PROGRESS (SECURITY_STATUS)0x8009033FL
#define SEC_E_KDC_INVALID_REQUEST (SECURITY_STATUS)0x80090340L
#define SEC_E_KDC_UNABLE_TO_REFER (SECURITY_STATUS)0x80090341L
#define SEC_E_KDC_UNKNOWN_ETYPE (SECURITY_STATUS)0x80090342L
#define SEC_E_UNSUPPORTED_PREAUTH (SECURITY_STATUS)0x80090343L
#define SEC_E_DELEGATION_REQUIRED (SECURITY_STATUS)0x80090345L
#define SEC_E_BAD_BINDINGS (SECURITY_STATUS)0x80090346L
#define SEC_E_MULTIPLE_ACCOUNTS (SECURITY_STATUS)0x80090347L
#define SEC_E_NO_KERB_KEY (SECURITY_STATUS)0x80090348L
#define SEC_E_CERT_WRONG_USAGE (SECURITY_STATUS)0x80090349L
#define SEC_E_DOWNGRADE_DETECTED (SECURITY_STATUS)0x80090350L
#define SEC_E_SMARTCARD_CERT_REVOKED (SECURITY_STATUS)0x80090351L
#define SEC_E_ISSUING_CA_UNTRUSTED (SECURITY_STATUS)0x80090352L
#define SEC_E_REVOCATION_OFFLINE_C (SECURITY_STATUS)0x80090353L
#define SEC_E_PKINIT_CLIENT_FAILURE (SECURITY_STATUS)0x80090354L
#define SEC_E_SMARTCARD_CERT_EXPIRED (SECURITY_STATUS)0x80090355L
#define SEC_E_NO_S4U_PROT_SUPPORT (SECURITY_STATUS)0x80090356L
#define SEC_E_CROSSREALM_DELEGATION_FAILURE (SECURITY_STATUS)0x80090357L
#define SEC_E_REVOCATION_OFFLINE_KDC (SECURITY_STATUS)0x80090358L
#define SEC_E_ISSUING_CA_UNTRUSTED_KDC (SECURITY_STATUS)0x80090359L
#define SEC_E_KDC_CERT_EXPIRED (SECURITY_STATUS)0x8009035AL
#define SEC_E_KDC_CERT_REVOKED (SECURITY_STATUS)0x8009035BL
#define SEC_E_INVALID_PARAMETER (SECURITY_STATUS)0x8009035DL
#define SEC_E_DELEGATION_POLICY (SECURITY_STATUS)0x8009035EL
#define SEC_E_POLICY_NLTM_ONLY (SECURITY_STATUS)0x8009035FL
#define SEC_E_NO_CONTEXT (SECURITY_STATUS)0x80090361L
#define SEC_E_PKU2U_CERT_FAILURE (SECURITY_STATUS)0x80090362L
#define SEC_E_MUTUAL_AUTH_FAILED (SECURITY_STATUS)0x80090363L

#define SEC_I_CONTINUE_NEEDED (SECURITY_STATUS)0x00090312L
#define SEC_I_COMPLETE_NEEDED (SECURITY_STATUS)0x00090313L
#define SEC_I_COMPLETE_AND_CONTINUE (SECURITY_STATUS)0x00090314L
#define SEC_I_LOCAL_LOGON (SECURITY_STATUS)0x00090315L
#define SEC_I_CONTEXT_EXPIRED (SECURITY_STATUS)0x00090317L
#define SEC_I_INCOMPLETE_CREDENTIALS (SECURITY_STATUS)0x00090320L
#define SEC_I_RENEGOTIATE (SECURITY_STATUS)0x00090321L
#define SEC_I_NO_LSA_CONTEXT (SECURITY_STATUS)0x00090323L
#define SEC_I_SIGNATURE_NEEDED (SECURITY_STATUS)0x0009035CL
#define SEC_I_NO_RENEGOTIATION (SECURITY_STATUS)0x00090360L

#endif /* _WINERROR_ */

/* ============== some definitions missing in mingw ========================*/
#ifndef SEC_E_INVALID_PARAMETER
#define SEC_E_INVALID_PARAMETER (SECURITY_STATUS)0x8009035DL
#endif

#ifndef SEC_E_DELEGATION_POLICY
#define SEC_E_DELEGATION_POLICY (SECURITY_STATUS)0x8009035EL
#endif

#ifndef SEC_E_POLICY_NLTM_ONLY
#define SEC_E_POLICY_NLTM_ONLY (SECURITY_STATUS)0x8009035FL
#endif

#ifndef SEC_E_NO_CONTEXT
#define SEC_E_NO_CONTEXT (SECURITY_STATUS)0x80090361L
#endif

#ifndef SEC_E_PKU2U_CERT_FAILURE
#define SEC_E_PKU2U_CERT_FAILURE (SECURITY_STATUS)0x80090362L
#endif

#ifndef SEC_E_MUTUAL_AUTH_FAILED
#define SEC_E_MUTUAL_AUTH_FAILED (SECURITY_STATUS)0x80090363L
#endif

#ifndef SEC_I_SIGNATURE_NEEDED
#define SEC_I_SIGNATURE_NEEDED (SECURITY_STATUS)0x0009035CL
#endif

#ifndef SEC_I_NO_RENEGOTIATION
#define SEC_I_NO_RENEGOTIATION (SECURITY_STATUS)0x00090360L
#endif

/* ==================================================================================== */

#define SECURITY_NATIVE_DREP 0x00000010
#define SECURITY_NETWORK_DREP 0x00000000

#define SECPKG_CRED_INBOUND 0x00000001
#define SECPKG_CRED_OUTBOUND 0x00000002
#define SECPKG_CRED_BOTH 0x00000003
#define SECPKG_CRED_AUTOLOGON_RESTRICTED 0x00000010
#define SECPKG_CRED_PROCESS_POLICY_ONLY 0x00000020

/* Security Context Attributes */

#define SECPKG_ATTR_SIZES 0
#define SECPKG_ATTR_NAMES 1
#define SECPKG_ATTR_LIFESPAN 2
#define SECPKG_ATTR_DCE_INFO 3
#define SECPKG_ATTR_STREAM_SIZES 4
#define SECPKG_ATTR_KEY_INFO 5
#define SECPKG_ATTR_AUTHORITY 6
#define SECPKG_ATTR_PROTO_INFO 7
#define SECPKG_ATTR_PASSWORD_EXPIRY 8
#define SECPKG_ATTR_SESSION_KEY 9
#define SECPKG_ATTR_PACKAGE_INFO 10
#define SECPKG_ATTR_USER_FLAGS 11
#define SECPKG_ATTR_NEGOTIATION_INFO 12
#define SECPKG_ATTR_NATIVE_NAMES 13
#define SECPKG_ATTR_FLAGS 14
#define SECPKG_ATTR_USE_VALIDATED 15
#define SECPKG_ATTR_CREDENTIAL_NAME 16
#define SECPKG_ATTR_TARGET_INFORMATION 17
#define SECPKG_ATTR_ACCESS_TOKEN 18
#define SECPKG_ATTR_TARGET 19
#define SECPKG_ATTR_AUTHENTICATION_ID 20
#define SECPKG_ATTR_LOGOFF_TIME 21
#define SECPKG_ATTR_NEGO_KEYS 22
#define SECPKG_ATTR_PROMPTING_NEEDED 24
#define SECPKG_ATTR_UNIQUE_BINDINGS 25
#define SECPKG_ATTR_ENDPOINT_BINDINGS 26
#define SECPKG_ATTR_CLIENT_SPECIFIED_TARGET 27
#define SECPKG_ATTR_LAST_CLIENT_TOKEN_STATUS 30
#define SECPKG_ATTR_NEGO_PKG_INFO 31
#define SECPKG_ATTR_NEGO_STATUS 32
#define SECPKG_ATTR_CONTEXT_DELETED 33

#if !defined(_WIN32) || defined(_UWP)

typedef struct
{
	void* AccessToken;
} SecPkgContext_AccessToken;

typedef struct
{
	UINT32 dwFlags;
	UINT32 cbAppData;
	BYTE* pbAppData;
} SecPkgContext_SessionAppData;

typedef struct
{
	char* sAuthorityName;
} SecPkgContext_Authority;

typedef struct
{
	char* sTargetName;
} SecPkgContext_ClientSpecifiedTarget;

typedef UINT32 ALG_ID;

typedef struct
{
	UINT32 dwProtocol;
	ALG_ID aiCipher;
	UINT32 dwCipherStrength;
	ALG_ID aiHash;
	UINT32 dwHashStrength;
	ALG_ID aiExch;
	UINT32 dwExchStrength;
} SecPkgContext_ConnectionInfo;

typedef struct
{
	UINT32 AuthBufferLen;
	BYTE* AuthBuffer;
} SecPkgContext_ClientCreds;

typedef struct
{
	UINT32 AuthzSvc;
	void* pPac;
} SecPkgContex_DceInfo;

typedef struct
{
	UINT32 dwInitiatorAddrType;
	UINT32 cbInitiatorLength;
	UINT32 dwInitiatorOffset;
	UINT32 dwAcceptorAddrType;
	UINT32 cbAcceptorLength;
	UINT32 dwAcceptorOffset;
	UINT32 cbApplicationDataLength;
	UINT32 dwApplicationDataOffset;
} SEC_CHANNEL_BINDINGS;

typedef struct
{
	BYTE rgbKeys[128];
	BYTE rgbIVs[64];
} SecPkgContext_EapKeyBlock;

typedef struct
{
	UINT32 Flags;
} SecPkgContext_Flags;

typedef struct
{
	char* sSignatureAlgorithmName;
	char* sEncryptAlgorithmName;
	UINT32 KeySize;
	UINT32 SignatureAlgorithm;
	UINT32 EncryptAlgorithm;
} SecPkgContext_KeyInfo;

typedef struct
{
	TimeStamp tsStart;
	TimeStamp tsExpiry;
} SecPkgContext_Lifespan;

typedef struct
{
	char* sUserName;
} SecPkgContext_Names;

typedef struct
{
	char* sClientName;
	char* sServerName;
} SecPkgContext_NativeNames;

typedef struct
{
	SecPkgInfo* PackageInfo;
	UINT32 NegotiationState;
} SecPkgContext_NegotiationInfo;

typedef struct
{
	SecPkgInfo* PackageInfo;
} SecPkgContext_PackageInfo;

typedef struct
{
	TimeStamp tsPasswordExpires;
} SecPkgContext_PasswordExpiry;

typedef struct
{
	UINT32 SessionKeyLength;
	BYTE* SessionKey;
} SecPkgContext_SessionKey;

typedef struct
{
	UINT32 dwFlags;
	UINT32 cbSessionId;
	BYTE rgbSessionId[32];
} SecPkgContext_SessionInfo;

typedef struct
{
	UINT32 cbMaxToken;
	UINT32 cbMaxSignature;
	UINT32 cbBlockSize;
	UINT32 cbSecurityTrailer;
} SecPkgContext_Sizes;

typedef struct
{
	UINT32 cbHeader;
	UINT32 cbTrailer;
	UINT32 cbMaximumMessage;
	UINT32 cBuffers;
	UINT32 cbBlockSize;
} SecPkgContext_StreamSizes;

typedef struct
{
	void* AttributeInfo;
} SecPkgContext_SubjectAttributes;

typedef struct
{
	UINT16 cSignatureAndHashAlgorithms;
	UINT16* pSignatureAndHashAlgorithms;
} SecPkgContext_SupportedSignatures;

typedef struct
{
	UINT32 MarshalledTargetInfoLength;
	BYTE* MarshalledTargetInfo;
} SecPkgContext_TargetInformation;

/* Security Credentials Attributes */

#define SECPKG_CRED_ATTR_NAMES 1

typedef struct
{
	SEC_CHAR* sUserName;
} SecPkgCredentials_NamesA;
typedef SecPkgCredentials_NamesA* PSecPkgCredentials_NamesA;

typedef struct
{
	SEC_WCHAR* sUserName;
} SecPkgCredentials_NamesW;
typedef SecPkgCredentials_NamesW* PSecPkgCredentials_NamesW;

#ifdef UNICODE
#define SecPkgCredentials_Names SecPkgCredentials_NamesW
#define PSecPkgCredentials_Names PSecPkgCredentials_NamesW
#else
#define SecPkgCredentials_Names SecPkgCredentials_NamesA
#define PSecPkgCredentials_Names PSecPkgCredentials_NamesA
#endif

#endif /* !defined(_WIN32) || defined(_UWP) */

#if !defined(_WIN32) || defined(_UWP) || (defined(__MINGW32__) && (__MINGW64_VERSION_MAJOR < 8))
typedef struct
{
	UINT32 BindingsLength;
	SEC_CHANNEL_BINDINGS* Bindings;
} SecPkgContext_Bindings;
#endif

/* InitializeSecurityContext Flags */

#define ISC_REQ_DELEGATE 0x00000001
#define ISC_REQ_MUTUAL_AUTH 0x00000002
#define ISC_REQ_REPLAY_DETECT 0x00000004
#define ISC_REQ_SEQUENCE_DETECT 0x00000008
#define ISC_REQ_CONFIDENTIALITY 0x00000010
#define ISC_REQ_USE_SESSION_KEY 0x00000020
#define ISC_REQ_PROMPT_FOR_CREDS 0x00000040
#define ISC_REQ_USE_SUPPLIED_CREDS 0x00000080
#define ISC_REQ_ALLOCATE_MEMORY 0x00000100
#define ISC_REQ_USE_DCE_STYLE 0x00000200
#define ISC_REQ_DATAGRAM 0x00000400
#define ISC_REQ_CONNECTION 0x00000800
#define ISC_REQ_CALL_LEVEL 0x00001000
#define ISC_REQ_FRAGMENT_SUPPLIED 0x00002000
#define ISC_REQ_EXTENDED_ERROR 0x00004000
#define ISC_REQ_STREAM 0x00008000
#define ISC_REQ_INTEGRITY 0x00010000
#define ISC_REQ_IDENTIFY 0x00020000
#define ISC_REQ_NULL_SESSION 0x00040000
#define ISC_REQ_MANUAL_CRED_VALIDATION 0x00080000
#define ISC_REQ_RESERVED1 0x00100000
#define ISC_REQ_FRAGMENT_TO_FIT 0x00200000
#define ISC_REQ_FORWARD_CREDENTIALS 0x00400000
#define ISC_REQ_NO_INTEGRITY 0x00800000
#define ISC_REQ_USE_HTTP_STYLE 0x01000000

#define ISC_RET_DELEGATE 0x00000001
#define ISC_RET_MUTUAL_AUTH 0x00000002
#define ISC_RET_REPLAY_DETECT 0x00000004
#define ISC_RET_SEQUENCE_DETECT 0x00000008
#define ISC_RET_CONFIDENTIALITY 0x00000010
#define ISC_RET_USE_SESSION_KEY 0x00000020
#define ISC_RET_USED_COLLECTED_CREDS 0x00000040
#define ISC_RET_USED_SUPPLIED_CREDS 0x00000080
#define ISC_RET_ALLOCATED_MEMORY 0x00000100
#define ISC_RET_USED_DCE_STYLE 0x00000200
#define ISC_RET_DATAGRAM 0x00000400
#define ISC_RET_CONNECTION 0x00000800
#define ISC_RET_INTERMEDIATE_RETURN 0x00001000
#define ISC_RET_CALL_LEVEL 0x00002000
#define ISC_RET_EXTENDED_ERROR 0x00004000
#define ISC_RET_STREAM 0x00008000
#define ISC_RET_INTEGRITY 0x00010000
#define ISC_RET_IDENTIFY 0x00020000
#define ISC_RET_NULL_SESSION 0x00040000
#define ISC_RET_MANUAL_CRED_VALIDATION 0x00080000
#define ISC_RET_RESERVED1 0x00100000
#define ISC_RET_FRAGMENT_ONLY 0x00200000
#define ISC_RET_FORWARD_CREDENTIALS 0x00400000
#define ISC_RET_USED_HTTP_STYLE 0x01000000

/* AcceptSecurityContext Flags */

#define ASC_REQ_DELEGATE 0x00000001
#define ASC_REQ_MUTUAL_AUTH 0x00000002
#define ASC_REQ_REPLAY_DETECT 0x00000004
#define ASC_REQ_SEQUENCE_DETECT 0x00000008
#define ASC_REQ_CONFIDENTIALITY 0x00000010
#define ASC_REQ_USE_SESSION_KEY 0x00000020
#define ASC_REQ_ALLOCATE_MEMORY 0x00000100
#define ASC_REQ_USE_DCE_STYLE 0x00000200
#define ASC_REQ_DATAGRAM 0x00000400
#define ASC_REQ_CONNECTION 0x00000800
#define ASC_REQ_CALL_LEVEL 0x00001000
#define ASC_REQ_EXTENDED_ERROR 0x00008000
#define ASC_REQ_STREAM 0x00010000
#define ASC_REQ_INTEGRITY 0x00020000
#define ASC_REQ_LICENSING 0x00040000
#define ASC_REQ_IDENTIFY 0x00080000
#define ASC_REQ_ALLOW_NULL_SESSION 0x00100000
#define ASC_REQ_ALLOW_NON_USER_LOGONS 0x00200000
#define ASC_REQ_ALLOW_CONTEXT_REPLAY 0x00400000
#define ASC_REQ_FRAGMENT_TO_FIT 0x00800000
#define ASC_REQ_FRAGMENT_SUPPLIED 0x00002000
#define ASC_REQ_NO_TOKEN 0x01000000
#define ASC_REQ_PROXY_BINDINGS 0x04000000
#define ASC_REQ_ALLOW_MISSING_BINDINGS 0x10000000

#define ASC_RET_DELEGATE 0x00000001
#define ASC_RET_MUTUAL_AUTH 0x00000002
#define ASC_RET_REPLAY_DETECT 0x00000004
#define ASC_RET_SEQUENCE_DETECT 0x00000008
#define ASC_RET_CONFIDENTIALITY 0x00000010
#define ASC_RET_USE_SESSION_KEY 0x00000020
#define ASC_RET_ALLOCATED_MEMORY 0x00000100
#define ASC_RET_USED_DCE_STYLE 0x00000200
#define ASC_RET_DATAGRAM 0x00000400
#define ASC_RET_CONNECTION 0x00000800
#define ASC_RET_CALL_LEVEL 0x00002000
#define ASC_RET_THIRD_LEG_FAILED 0x00004000
#define ASC_RET_EXTENDED_ERROR 0x00008000
#define ASC_RET_STREAM 0x00010000
#define ASC_RET_INTEGRITY 0x00020000
#define ASC_RET_LICENSING 0x00040000
#define ASC_RET_IDENTIFY 0x00080000
#define ASC_RET_NULL_SESSION 0x00100000
#define ASC_RET_ALLOW_NON_USER_LOGONS 0x00200000
#define ASC_RET_FRAGMENT_ONLY 0x00800000
#define ASC_RET_NO_TOKEN 0x01000000
#define ASC_RET_NO_PROXY_BINDINGS 0x04000000
#define ASC_RET_MISSING_BINDINGS 0x10000000

#define SEC_WINNT_AUTH_IDENTITY_ANSI 0x1
#define SEC_WINNT_AUTH_IDENTITY_UNICODE 0x2
#define SEC_WINNT_AUTH_IDENTITY_EXTENDED 0x100

#if !defined(_WIN32) || defined(_UWP)

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

#ifndef _AUTH_IDENTITY_DEFINED
#define _AUTH_IDENTITY_DEFINED

typedef struct
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

typedef struct
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

typedef struct
{
	/* TSPasswordCreds */
	UINT16* User;
	UINT32 UserLength;
	UINT16* Domain;
	UINT32 DomainLength;
	UINT16* Password;
	UINT32 PasswordLength;
	UINT32 Flags;
} SEC_WINNT_AUTH_IDENTITY;

#endif /* _AUTH_IDENTITY_DEFINED */

#ifndef SEC_WINNT_AUTH_IDENTITY_VERSION
#define SEC_WINNT_AUTH_IDENTITY_VERSION 0x200

typedef struct
{
	UINT32 Version;
	UINT32 Length;
	UINT16* User;
	UINT32 UserLength;
	UINT16* Domain;
	UINT32 DomainLength;
	UINT16* Password;
	UINT32 PasswordLength;
	UINT32 Flags;
	BYTE* PackageList;
	UINT32 PackageListLength;
} SEC_WINNT_AUTH_IDENTITY_EXW, *PSEC_WINNT_AUTH_IDENTITY_EXW;

typedef struct
{
	UINT32 Version;
	UINT32 Length;
	BYTE* User;
	UINT32 UserLength;
	BYTE* Domain;
	UINT32 DomainLength;
	BYTE* Password;
	UINT32 PasswordLength;
	UINT32 Flags;
	BYTE* PackageList;
	UINT32 PackageListLength;
} SEC_WINNT_AUTH_IDENTITY_EXA, *PSEC_WINNT_AUTH_IDENTITY_EXA;

#endif /* SEC_WINNT_AUTH_IDENTITY_VERSION */

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

typedef struct
{
	ULONG_PTR dwLower;
	ULONG_PTR dwUpper;
} SecHandle;
typedef SecHandle* PSecHandle;

typedef SecHandle CredHandle;
typedef CredHandle* PCredHandle;
typedef SecHandle CtxtHandle;
typedef CtxtHandle* PCtxtHandle;

#define SecInvalidateHandle(x) \
	((PSecHandle)(x))->dwLower = ((PSecHandle)(x))->dwUpper = ((ULONG_PTR)((INT_PTR)-1))

#define SecIsValidHandle(x)                                        \
	((((PSecHandle)(x))->dwLower != ((ULONG_PTR)((INT_PTR)-1))) && \
	 (((PSecHandle)(x))->dwUpper != ((ULONG_PTR)((INT_PTR)-1))))

typedef struct
{
	ULONG cbBuffer;
	ULONG BufferType;
	void* pvBuffer;
} SecBuffer;
typedef SecBuffer* PSecBuffer;

typedef struct
{
	ULONG ulVersion;
	ULONG cBuffers;
	PSecBuffer pBuffers;
} SecBufferDesc;
typedef SecBufferDesc* PSecBufferDesc;

#endif /* !defined(_WIN32) || defined(_UWP) */

typedef SECURITY_STATUS (*psSspiNtlmHashCallback)(void* client,
                                                  const SEC_WINNT_AUTH_IDENTITY* authIdentity,
                                                  const SecBuffer* ntproofvalue,
                                                  const BYTE* randkey, const BYTE* mic,
                                                  const SecBuffer* micvalue, BYTE* ntlmhash);

typedef struct
{
	char* samFile;
	psSspiNtlmHashCallback hashCallback;
	void* hashCallbackArg;
} SEC_WINPR_NTLM_SETTINGS;

typedef struct
{
	char* keytab;
	char* cache;
	char* armorCache;
	char* pkinitX509Anchors;
	char* pkinitX509Identity;
	BOOL withPac;
	INT32 startTime;
	INT32 renewLifeTime;
	INT32 lifeTime;
	BYTE certSha1[20];
} SEC_WINPR_KERBEROS_SETTINGS;

typedef struct
{
	SEC_WINNT_AUTH_IDENTITY identity;
	SEC_WINPR_NTLM_SETTINGS ntlmSettings;
	SEC_WINPR_KERBEROS_SETTINGS kerberosSettings;
} SEC_WINNT_AUTH_IDENTITY_WINPR;

#define SECBUFFER_VERSION 0

/* Buffer Types */
#define SECBUFFER_EMPTY 0
#define SECBUFFER_DATA 1
#define SECBUFFER_TOKEN 2
#define SECBUFFER_PKG_PARAMS 3
#define SECBUFFER_MISSING 4
#define SECBUFFER_EXTRA 5
#define SECBUFFER_STREAM_TRAILER 6
#define SECBUFFER_STREAM_HEADER 7
#define SECBUFFER_NEGOTIATION_INFO 8
#define SECBUFFER_PADDING 9
#define SECBUFFER_STREAM 10
#define SECBUFFER_MECHLIST 11
#define SECBUFFER_MECHLIST_SIGNATURE 12
#define SECBUFFER_TARGET 13
#define SECBUFFER_CHANNEL_BINDINGS 14
#define SECBUFFER_CHANGE_PASS_RESPONSE 15
#define SECBUFFER_TARGET_HOST 16
#define SECBUFFER_ALERT 17

/* Security Buffer Flags */
#define SECBUFFER_ATTRMASK 0xF0000000
#define SECBUFFER_READONLY 0x80000000
#define SECBUFFER_READONLY_WITH_CHECKSUM 0x10000000
#define SECBUFFER_RESERVED 0x60000000

#if !defined(_WIN32) || defined(_UWP)

typedef void(SEC_ENTRY* SEC_GET_KEY_FN)(void* Arg, void* Principal, UINT32 KeyVer, void** Key,
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
                                                                      ULONG ulAttribute,
                                                                      void* pBuffer);
typedef SECURITY_STATUS(SEC_ENTRY* QUERY_CREDENTIALS_ATTRIBUTES_FN_W)(PCredHandle phCredential,
                                                                      ULONG ulAttribute,
                                                                      void* pBuffer);

#ifdef UNICODE
#define QueryCredentialsAttributes QueryCredentialsAttributesW
#define QUERY_CREDENTIALS_ATTRIBUTES_FN QUERY_CREDENTIALS_ATTRIBUTES_FN_W
#else
#define QueryCredentialsAttributes QueryCredentialsAttributesA
#define QUERY_CREDENTIALS_ATTRIBUTES_FN QUERY_CREDENTIALS_ATTRIBUTES_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* ACQUIRE_CREDENTIALS_HANDLE_FN_A)(
    LPSTR pszPrincipal, LPSTR pszPackage, ULONG fCredentialUse, void* pvLogonID, void* pAuthData,
    SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry);
typedef SECURITY_STATUS(SEC_ENTRY* ACQUIRE_CREDENTIALS_HANDLE_FN_W)(
    LPWSTR pszPrincipal, LPWSTR pszPackage, ULONG fCredentialUse, void* pvLogonID, void* pAuthData,
    SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry);

#ifdef UNICODE
#define AcquireCredentialsHandle AcquireCredentialsHandleW
#define ACQUIRE_CREDENTIALS_HANDLE_FN ACQUIRE_CREDENTIALS_HANDLE_FN_W
#else
#define AcquireCredentialsHandle AcquireCredentialsHandleA
#define ACQUIRE_CREDENTIALS_HANDLE_FN ACQUIRE_CREDENTIALS_HANDLE_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* FREE_CREDENTIALS_HANDLE_FN)(PCredHandle phCredential);

typedef SECURITY_STATUS(SEC_ENTRY* INITIALIZE_SECURITY_CONTEXT_FN_A)(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry);
typedef SECURITY_STATUS(SEC_ENTRY* INITIALIZE_SECURITY_CONTEXT_FN_W)(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry);

#ifdef UNICODE
#define InitializeSecurityContext InitializeSecurityContextW
#define INITIALIZE_SECURITY_CONTEXT_FN INITIALIZE_SECURITY_CONTEXT_FN_W
#else
#define InitializeSecurityContext InitializeSecurityContextA
#define INITIALIZE_SECURITY_CONTEXT_FN INITIALIZE_SECURITY_CONTEXT_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* ACCEPT_SECURITY_CONTEXT_FN)(
    PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput, ULONG fContextReq,
    ULONG TargetDataRep, PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr,
    PTimeStamp ptsTimeStamp);

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
                                                        PSecBufferDesc pMessage, ULONG MessageSeqNo,
                                                        PULONG pfQOP);

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
                                                               PSecBuffer pPackedContext,
                                                               HANDLE* pToken);

typedef SECURITY_STATUS(SEC_ENTRY* IMPORT_SECURITY_CONTEXT_FN_A)(SEC_CHAR* pszPackage,
                                                                 PSecBuffer pPackedContext,
                                                                 HANDLE pToken,
                                                                 PCtxtHandle phContext);
typedef SECURITY_STATUS(SEC_ENTRY* IMPORT_SECURITY_CONTEXT_FN_W)(SEC_WCHAR* pszPackage,
                                                                 PSecBuffer pPackedContext,
                                                                 HANDLE pToken,
                                                                 PCtxtHandle phContext);

#ifdef UNICODE
#define ImportSecurityContext ImportSecurityContextW
#define IMPORT_SECURITY_CONTEXT_FN IMPORT_SECURITY_CONTEXT_FN_W
#else
#define ImportSecurityContext ImportSecurityContextA
#define IMPORT_SECURITY_CONTEXT_FN IMPORT_SECURITY_CONTEXT_FN_A
#endif

typedef SECURITY_STATUS(SEC_ENTRY* ADD_CREDENTIALS_FN_A)(
    PCredHandle hCredentials, SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, UINT32 fCredentialUse,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PTimeStamp ptsExpiry);
typedef SECURITY_STATUS(SEC_ENTRY* ADD_CREDENTIALS_FN_W)(
    PCredHandle hCredentials, SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, UINT32 fCredentialUse,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PTimeStamp ptsExpiry);

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
                                                       PSecBufferDesc pMessage, ULONG MessageSeqNo,
                                                       PULONG pfQOP);

typedef SECURITY_STATUS(SEC_ENTRY* SET_CONTEXT_ATTRIBUTES_FN_A)(PCtxtHandle phContext,
                                                                ULONG ulAttribute, void* pBuffer,
                                                                ULONG cbBuffer);
typedef SECURITY_STATUS(SEC_ENTRY* SET_CONTEXT_ATTRIBUTES_FN_W)(PCtxtHandle phContext,
                                                                ULONG ulAttribute, void* pBuffer,
                                                                ULONG cbBuffer);

#ifdef UNICODE
#define SetContextAttributes SetContextAttributesW
#define SET_CONTEXT_ATTRIBUTES_FN SET_CONTEXT_ATTRIBUTES_FN_W
#else
#define SetContextAttributes SetContextAttributesA
#define SET_CONTEXT_ATTRIBUTES_FN SET_CONTEXT_ATTRIBUTES_FN_A
#endif

#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION \
	1 /* Interface has all routines through DecryptMessage */
#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION_2 \
	2 /* Interface has all routines through SetContextAttributes */
#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION_3 \
	3 /* Interface has all routines through SetCredentialsAttributes */
#define SECURITY_SUPPORT_PROVIDER_INTERFACE_VERSION_4 \
	4 /* Interface has all routines through ChangeAccountPassword */

typedef struct
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
} SecurityFunctionTableA;
typedef SecurityFunctionTableA* PSecurityFunctionTableA;

typedef struct
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
} SecurityFunctionTableW;
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
extern "C"
{
#endif

#ifdef SSPI_DLL

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

	WINPR_API SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleA(
	    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
	    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
	    PTimeStamp ptsExpiry);
	WINPR_API SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleW(
	    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
	    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
	    PTimeStamp ptsExpiry);

	WINPR_API SECURITY_STATUS SEC_ENTRY ExportSecurityContext(PCtxtHandle phContext, ULONG fFlags,
	                                                          PSecBuffer pPackedContext,
	                                                          HANDLE* pToken);
	WINPR_API SECURITY_STATUS SEC_ENTRY FreeCredentialsHandle(PCredHandle phCredential);

	WINPR_API SECURITY_STATUS SEC_ENTRY ImportSecurityContextA(SEC_CHAR* pszPackage,
	                                                           PSecBuffer pPackedContext,
	                                                           HANDLE pToken,
	                                                           PCtxtHandle phContext);
	WINPR_API SECURITY_STATUS SEC_ENTRY ImportSecurityContextW(SEC_WCHAR* pszPackage,
	                                                           PSecBuffer pPackedContext,
	                                                           HANDLE pToken,
	                                                           PCtxtHandle phContext);

	WINPR_API SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesA(PCredHandle phCredential,
	                                                                ULONG ulAttribute,
	                                                                void* pBuffer);
	WINPR_API SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesW(PCredHandle phCredential,
	                                                                ULONG ulAttribute,
	                                                                void* pBuffer);

	/* Context Management */

	WINPR_API SECURITY_STATUS SEC_ENTRY
	AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput,
	                      ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
	                      PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp);

	WINPR_API SECURITY_STATUS SEC_ENTRY ApplyControlToken(PCtxtHandle phContext,
	                                                      PSecBufferDesc pInput);
	WINPR_API SECURITY_STATUS SEC_ENTRY CompleteAuthToken(PCtxtHandle phContext,
	                                                      PSecBufferDesc pToken);
	WINPR_API SECURITY_STATUS SEC_ENTRY DeleteSecurityContext(PCtxtHandle phContext);
	WINPR_API SECURITY_STATUS SEC_ENTRY FreeContextBuffer(void* pvContextBuffer);
	WINPR_API SECURITY_STATUS SEC_ENTRY ImpersonateSecurityContext(PCtxtHandle phContext);

	WINPR_API SECURITY_STATUS SEC_ENTRY InitializeSecurityContextA(
	    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
	    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
	    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr,
	    PTimeStamp ptsExpiry);
	WINPR_API SECURITY_STATUS SEC_ENTRY InitializeSecurityContextW(
	    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName,
	    ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput,
	    ULONG Reserved2, PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr,
	    PTimeStamp ptsExpiry);

	WINPR_API SECURITY_STATUS SEC_ENTRY QueryContextAttributes(PCtxtHandle phContext,
	                                                           ULONG ulAttribute, void* pBuffer);
	WINPR_API SECURITY_STATUS SEC_ENTRY QuerySecurityContextToken(PCtxtHandle phContext,
	                                                              HANDLE* phToken);
	WINPR_API SECURITY_STATUS SEC_ENTRY SetContextAttributes(PCtxtHandle phContext,
	                                                         ULONG ulAttribute, void* pBuffer,
	                                                         ULONG cbBuffer);
	WINPR_API SECURITY_STATUS SEC_ENTRY RevertSecurityContext(PCtxtHandle phContext);

	/* Message Support */

	WINPR_API SECURITY_STATUS SEC_ENTRY DecryptMessage(PCtxtHandle phContext,
	                                                   PSecBufferDesc pMessage, ULONG MessageSeqNo,
	                                                   PULONG pfQOP);
	WINPR_API SECURITY_STATUS SEC_ENTRY EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
	                                                   PSecBufferDesc pMessage, ULONG MessageSeqNo);
	WINPR_API SECURITY_STATUS SEC_ENTRY MakeSignature(PCtxtHandle phContext, ULONG fQOP,
	                                                  PSecBufferDesc pMessage, ULONG MessageSeqNo);
	WINPR_API SECURITY_STATUS SEC_ENTRY VerifySignature(PCtxtHandle phContext,
	                                                    PSecBufferDesc pMessage, ULONG MessageSeqNo,
	                                                    PULONG pfQOP);

#endif /* SSPI_DLL */

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C"
{
#endif

	/* Custom API */

#define SECPKG_ATTR_AUTH_IDENTITY 1001
#define SECPKG_ATTR_AUTH_PASSWORD 1002
#define SECPKG_ATTR_AUTH_NTLM_HASH 1003
#define SECPKG_ATTR_AUTH_NTLM_MESSAGE 1100
#define SECPKG_ATTR_AUTH_NTLM_TIMESTAMP 1101
#define SECPKG_ATTR_AUTH_NTLM_CLIENT_CHALLENGE 1102
#define SECPKG_ATTR_AUTH_NTLM_SERVER_CHALLENGE 1103
#define SECPKG_ATTR_AUTH_NTLM_NTPROOF_VALUE 1104
#define SECPKG_ATTR_AUTH_NTLM_RANDKEY 1105
#define SECPKG_ATTR_AUTH_NTLM_MIC 1106
#define SECPKG_ATTR_AUTH_NTLM_MIC_VALUE 1107

	typedef struct
	{
		char User[256 + 1];
		char Domain[256 + 1];
	} SecPkgContext_AuthIdentity;

	typedef struct
	{
		char Password[256 + 1];
	} SecPkgContext_AuthPassword;

	typedef struct
	{
		int Version;
		BYTE NtlmHash[16];
	} SecPkgContext_AuthNtlmHash;

	typedef struct
	{
		BYTE Timestamp[8];
		BOOL ChallengeOrResponse;
	} SecPkgContext_AuthNtlmTimestamp;

	typedef struct
	{
		BYTE ClientChallenge[8];
	} SecPkgContext_AuthNtlmClientChallenge;

	typedef struct
	{
		BYTE ServerChallenge[8];
	} SecPkgContext_AuthNtlmServerChallenge;

	typedef struct
	{
		UINT32 type;
		UINT32 length;
		BYTE* buffer;
	} SecPkgContext_AuthNtlmMessage;

#define SSPI_INTERFACE_WINPR 0x00000001
#define SSPI_INTERFACE_NATIVE 0x00000002

	typedef PSecurityFunctionTableA(SEC_ENTRY* INIT_SECURITY_INTERFACE_EX_A)(DWORD flags);
	typedef PSecurityFunctionTableW(SEC_ENTRY* INIT_SECURITY_INTERFACE_EX_W)(DWORD flags);

	WINPR_API void sspi_GlobalInit(void);
	WINPR_API void sspi_GlobalFinish(void);

	WINPR_API void* sspi_SecBufferAlloc(PSecBuffer SecBuffer, ULONG size);
	WINPR_API void sspi_SecBufferFree(PSecBuffer SecBuffer);

#define sspi_SetAuthIdentity sspi_SetAuthIdentityA
	WINPR_API int sspi_SetAuthIdentityA(SEC_WINNT_AUTH_IDENTITY* identity, const char* user,
	                                    const char* domain, const char* password);
	WINPR_API int sspi_SetAuthIdentityW(SEC_WINNT_AUTH_IDENTITY* identity, const WCHAR* user,
	                                    const WCHAR* domain, const WCHAR* password);
	WINPR_API int sspi_SetAuthIdentityWithLengthW(SEC_WINNT_AUTH_IDENTITY* identity,
	                                              const WCHAR* user, size_t userLen,
	                                              const WCHAR* domain, size_t domainLen,
	                                              const WCHAR* password, size_t passwordLen);
	WINPR_API int sspi_SetAuthIdentityWithUnicodePassword(SEC_WINNT_AUTH_IDENTITY* identity,
	                                                      const char* user, const char* domain,
	                                                      LPWSTR password, ULONG passwordLength);
	WINPR_API int sspi_CopyAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity,
	                                    const SEC_WINNT_AUTH_IDENTITY* srcIdentity);

	WINPR_API void sspi_FreeAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity);

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
