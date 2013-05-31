/**
 * WinPR: Windows Portable Runtime
 * Credentials Management
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_CREDENTIALS_H
#define WINPR_CREDENTIALS_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

#define CRED_SESSION_WILDCARD_NAME_W			L"*Session"
#define CRED_SESSION_WILDCARD_NAME_A			"*Session"
#define CRED_SESSION_WILDCARD_NAME_LENGTH		(sizeof(CRED_SESSION_WILDCARD_NAME_A) - 1)

#define CRED_MAX_STRING_LENGTH				256
#define CRED_MAX_USERNAME_LENGTH			(256 + 1 + 256)
#define CRED_MAX_GENERIC_TARGET_NAME_LENGTH		32767
#define CRED_MAX_DOMAIN_TARGET_NAME_LENGTH		(256 + 1 + 80)
#define CRED_MAX_VALUE_SIZE				256
#define CRED_MAX_ATTRIBUTES				64

#define CRED_FLAGS_PASSWORD_FOR_CERT			0x0001
#define CRED_FLAGS_PROMPT_NOW				0x0002
#define CRED_FLAGS_USERNAME_TARGET			0x0004
#define CRED_FLAGS_OWF_CRED_BLOB			0x0008
#define CRED_FLAGS_VALID_FLAGS				0x000F

#define CRED_TYPE_GENERIC				1
#define CRED_TYPE_DOMAIN_PASSWORD			2
#define CRED_TYPE_DOMAIN_CERTIFICATE			3
#define CRED_TYPE_DOMAIN_VISIBLE_PASSWORD		4
#define CRED_TYPE_MAXIMUM				5
#define CRED_TYPE_MAXIMUM_EX				(CRED_TYPE_MAXIMUM + 1000)

#define CRED_MAX_CREDENTIAL_BLOB_SIZE			512

#define CRED_PERSIST_NONE				0
#define CRED_PERSIST_SESSION				1
#define CRED_PERSIST_LOCAL_MACHINE			2
#define CRED_PERSIST_ENTERPRISE				3

#define CRED_PRESERVE_CREDENTIAL_BLOB			0x1
#define CRED_CACHE_TARGET_INFORMATION			0x1
#define CRED_ALLOW_NAME_RESOLUTION			0x1

typedef struct _CREDENTIAL_ATTRIBUTEA
{
	LPSTR Keyword;
	DWORD Flags;
	DWORD ValueSize;
	LPBYTE Value;
} CREDENTIAL_ATTRIBUTEA, *PCREDENTIAL_ATTRIBUTEA;

typedef struct _CREDENTIAL_ATTRIBUTEW
{
	LPWSTR Keyword;
	DWORD Flags;
	DWORD ValueSize;
	LPBYTE Value;
} CREDENTIAL_ATTRIBUTEW, *PCREDENTIAL_ATTRIBUTEW;

typedef struct _CREDENTIALA
{
	DWORD Flags;
	DWORD Type;
	LPSTR TargetName;
	LPSTR Comment;
	FILETIME LastWritten;
	DWORD CredentialBlobSize;
	LPBYTE CredentialBlob;
	DWORD Persist;
	DWORD AttributeCount;
	PCREDENTIAL_ATTRIBUTEA Attributes;
	LPSTR TargetAlias;
	LPSTR UserName;
} CREDENTIALA, *PCREDENTIALA;

typedef struct _CREDENTIALW
{
	DWORD Flags;
	DWORD Type;
	LPWSTR TargetName;
	LPWSTR Comment;
	FILETIME LastWritten;
	DWORD CredentialBlobSize;
	LPBYTE CredentialBlob;
	DWORD Persist;
	DWORD AttributeCount;
	PCREDENTIAL_ATTRIBUTEW Attributes;
	LPWSTR TargetAlias;
	LPWSTR UserName;
} CREDENTIALW, *PCREDENTIALW;

typedef struct _CREDENTIAL_TARGET_INFORMATIONA
{
	LPSTR TargetName;
	LPSTR NetbiosServerName;
	LPSTR DnsServerName;
	LPSTR NetbiosDomainName;
	LPSTR DnsDomainName;
	LPSTR DnsTreeName;
	LPSTR PackageName;
	ULONG Flags;
	DWORD CredTypeCount;
	LPDWORD CredTypes;
} CREDENTIAL_TARGET_INFORMATIONA, *PCREDENTIAL_TARGET_INFORMATIONA;

typedef struct _CREDENTIAL_TARGET_INFORMATIONW
{
	LPWSTR TargetName;
	LPWSTR NetbiosServerName;
	LPWSTR DnsServerName;
	LPWSTR NetbiosDomainName;
	LPWSTR DnsDomainName;
	LPWSTR DnsTreeName;
	LPWSTR PackageName;
	ULONG Flags;
	DWORD CredTypeCount;
	LPDWORD CredTypes;
} CREDENTIAL_TARGET_INFORMATIONW, *PCREDENTIAL_TARGET_INFORMATIONW;

typedef enum _CRED_MARSHAL_TYPE
{
	CertCredential = 1,
	UsernameTargetCredential
} CRED_MARSHAL_TYPE, *PCRED_MARSHAL_TYPE;

typedef enum _CRED_PROTECTION_TYPE
{
	CredUnprotected = 0,
	CredUserProtection = 1,
	CredTrustedProtection = 2
} CRED_PROTECTION_TYPE, *PCRED_PROTECTION_TYPE;

#ifdef UNICODE
#define CRED_SESSION_WILDCARD_NAME	CRED_SESSION_WILDCARD_NAME_W
#define CREDENTIAL_ATTRIBUTE		CREDENTIAL_ATTRIBUTEW
#define PCREDENTIAL_ATTRIBUTE		PCREDENTIAL_ATTRIBUTEW
#define CREDENTIAL			CREDENTIALW
#define PCREDENTIAL			PCREDENTIALW
#define CREDENTIAL_TARGET_INFORMATION	CREDENTIAL_TARGET_INFORMATIONW
#define PCREDENTIAL_TARGET_INFORMATION	PCREDENTIAL_TARGET_INFORMATIONW
#else
#define CRED_SESSION_WILDCARD_NAME	CRED_SESSION_WILDCARD_NAME_A
#define CREDENTIAL_ATTRIBUTE		CREDENTIAL_ATTRIBUTEA
#define PCREDENTIAL_ATTRIBUTE		PCREDENTIAL_ATTRIBUTEA
#define CREDENTIAL			CREDENTIALA
#define PCREDENTIAL			PCREDENTIALA
#define CREDENTIAL_TARGET_INFORMATION	CREDENTIAL_TARGET_INFORMATIONA
#define PCREDENTIAL_TARGET_INFORMATION	PCREDENTIAL_TARGET_INFORMATIONA
#endif

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API BOOL CredWriteW(PCREDENTIALW Credential, DWORD Flags);
WINPR_API BOOL CredWriteA(PCREDENTIALA Credential, DWORD Flags);

WINPR_API BOOL CredReadW(LPCWSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALW* Credential);
WINPR_API BOOL CredReadA(LPCSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALA* Credential);

WINPR_API BOOL CredEnumerateW(LPCWSTR Filter, DWORD Flags, DWORD* Count, PCREDENTIALW** Credential);
WINPR_API BOOL CredEnumerateA(LPCSTR Filter, DWORD Flags, DWORD* Count, PCREDENTIALA** Credential);

WINPR_API BOOL CredWriteDomainCredentialsW(PCREDENTIAL_TARGET_INFORMATIONW TargetInfo, PCREDENTIALW Credential, DWORD Flags);
WINPR_API BOOL CredWriteDomainCredentialsA(PCREDENTIAL_TARGET_INFORMATIONA TargetInfo, PCREDENTIALA Credential, DWORD Flags);

WINPR_API BOOL CredReadDomainCredentialsW(PCREDENTIAL_TARGET_INFORMATIONW TargetInfo, DWORD Flags, DWORD* Count, PCREDENTIALW** Credential);
WINPR_API BOOL CredReadDomainCredentialsA(PCREDENTIAL_TARGET_INFORMATIONA TargetInfo, DWORD Flags, DWORD* Count, PCREDENTIALA** Credential);

WINPR_API BOOL CredDeleteW(LPCWSTR TargetName, DWORD Type, DWORD Flags);
WINPR_API BOOL CredDeleteA(LPCSTR TargetName, DWORD Type, DWORD Flags);

WINPR_API BOOL CredRenameW(LPCWSTR OldTargetName, LPCWSTR NewTargetName, DWORD Type, DWORD Flags);
WINPR_API BOOL CredRenameA(LPCSTR OldTargetName, LPCSTR NewTargetName, DWORD Type, DWORD Flags);

WINPR_API BOOL CredGetTargetInfoW(LPCWSTR TargetName, DWORD Flags, PCREDENTIAL_TARGET_INFORMATIONW* TargetInfo);
WINPR_API BOOL CredGetTargetInfoA(LPCSTR TargetName, DWORD Flags, PCREDENTIAL_TARGET_INFORMATIONA* TargetInfo);

WINPR_API BOOL CredMarshalCredentialW(CRED_MARSHAL_TYPE CredType, PVOID Credential, LPWSTR* MarshaledCredential);
WINPR_API BOOL CredMarshalCredentialA(CRED_MARSHAL_TYPE CredType, PVOID Credential, LPSTR* MarshaledCredential);

WINPR_API BOOL CredUnmarshalCredentialW(LPCWSTR MarshaledCredential, PCRED_MARSHAL_TYPE CredType, PVOID* Credential);
WINPR_API BOOL CredUnmarshalCredentialA(LPCSTR MarshaledCredential, PCRED_MARSHAL_TYPE CredType, PVOID* Credential);

WINPR_API BOOL CredIsMarshaledCredentialW(LPCWSTR MarshaledCredential);
WINPR_API BOOL CredIsMarshaledCredentialA(LPCSTR MarshaledCredential);

WINPR_API BOOL CredProtectW(BOOL fAsSelf, LPWSTR pszCredentials, DWORD cchCredentials,
		LPWSTR pszProtectedCredentials, DWORD* pcchMaxChars, CRED_PROTECTION_TYPE* ProtectionType);
WINPR_API BOOL CredProtectA(BOOL fAsSelf, LPSTR pszCredentials, DWORD cchCredentials,
		LPSTR pszProtectedCredentials, DWORD* pcchMaxChars, CRED_PROTECTION_TYPE* ProtectionType);

WINPR_API BOOL CredUnprotectW(BOOL fAsSelf, LPWSTR pszProtectedCredentials,
		DWORD cchCredentials, LPWSTR pszCredentials, DWORD* pcchMaxChars);
WINPR_API BOOL CredUnprotectA(BOOL fAsSelf, LPSTR pszProtectedCredentials,
		DWORD cchCredentials, LPSTR pszCredentials, DWORD* pcchMaxChars);

WINPR_API BOOL CredIsProtectedW(LPWSTR pszProtectedCredentials, CRED_PROTECTION_TYPE* pProtectionType);
WINPR_API BOOL CredIsProtectedA(LPSTR pszProtectedCredentials, CRED_PROTECTION_TYPE* pProtectionType);

WINPR_API BOOL CredFindBestCredentialW(LPCWSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALW* Credential);
WINPR_API BOOL CredFindBestCredentialA(LPCSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALA* Credential);

WINPR_API BOOL CredGetSessionTypes(DWORD MaximumPersistCount, LPDWORD MaximumPersist);

WINPR_API VOID CredFree(PVOID Buffer);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define CredWrite			CredWriteW
#define CredRead			CredReadW
#define CredEnumerate			CredEnumerateW
#define CredWriteDomainCredentials	CredWriteDomainCredentialsW
#define CredReadDomainCredentials	CredReadDomainCredentialsW
#define CredDelete			CredDeleteW
#define CredRename			CredRenameW
#define CredGetTargetInfo		CredGetTargetInfoW
#define CredMarshalCredential		CredMarshalCredentialW
#define CredUnmarshalCredential		CredUnmarshalCredentialW
#define CredIsMarshaledCredential	CredIsMarshaledCredentialW
#define CredProtect			CredProtectW
#define CredUnprotect			CredUnprotectW
#define CredIsProtected			CredIsProtectedW
#define CredFindBestCredential		CredFindBestCredentialW
#else
#define CredWrite			CredWriteA
#define CredRead			CredReadA
#define CredEnumerate			CredEnumerateA
#define CredWriteDomainCredentials	CredWriteDomainCredentialsA
#define CredReadDomainCredentials	CredReadDomainCredentialsA
#define CredDelete			CredDeleteA
#define CredRename			CredRenameA
#define CredGetTargetInfo		CredGetTargetInfoA
#define CredMarshalCredential		CredMarshalCredentialA
#define CredUnmarshalCredential		CredUnmarshalCredentialA
#define CredIsMarshaledCredential	CredIsMarshaledCredentialA
#define CredProtect			CredProtectA
#define CredUnprotect			CredUnprotectA
#define CredIsProtected			CredIsProtectedA
#define CredFindBestCredential		CredFindBestCredentialA
#endif

#endif

#endif /* WINPR_CREDENTIALS_H */

