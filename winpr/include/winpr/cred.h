/**
 * WinPR: Windows Portable Runtime
 * Windows credentials
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#ifndef WINPR_CRED_H_
#define WINPR_CRED_H_

#include <winpr/winpr.h>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#else

#include <winpr/wtypes.h>

#define CERT_HASH_LENGTH 20

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		CertCredential = 1,
		UsernameTargetCredential,
		BinaryBlobCredential,
		UsernameForPackedCredentials,
		BinaryBlobForSystem
	} CRED_MARSHAL_TYPE,
	    *PCRED_MARSHAL_TYPE;

	typedef struct
	{
		ULONG cbSize;
		UCHAR rgbHashOfCert[CERT_HASH_LENGTH];
	} CERT_CREDENTIAL_INFO, *PCERT_CREDENTIAL_INFO;

	typedef struct
	{
		LPSTR Keyword;
		DWORD Flags;
		DWORD ValueSize;
		LPBYTE Value;
	} CREDENTIAL_ATTRIBUTEA, *PCREDENTIAL_ATTRIBUTEA;

	typedef struct
	{
		LPWSTR Keyword;
		DWORD Flags;
		DWORD ValueSize;
		LPBYTE Value;
	} CREDENTIAL_ATTRIBUTEW, *PCREDENTIAL_ATTRIBUTEW;

	typedef struct
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

	typedef struct
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
		PCREDENTIAL_ATTRIBUTEA Attributes;
		LPWSTR TargetAlias;
		LPWSTR UserName;
	} CREDENTIALW, *PCREDENTIALW;

	typedef struct
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

	typedef struct
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

	typedef enum
	{
		CredUnprotected,
		CredUserProtection,
		CredTrustedProtection,
		CredForSystemProtection
	} CRED_PROTECTION_TYPE,
	    *PCRED_PROTECTION_TYPE;

	WINPR_API BOOL CredMarshalCredentialA(CRED_MARSHAL_TYPE CredType, PVOID Credential,
	                                      LPSTR* MarshaledCredential);
	WINPR_API BOOL CredMarshalCredentialW(CRED_MARSHAL_TYPE CredType, PVOID Credential,
	                                      LPWSTR* MarshaledCredential);

#ifdef UNICODE
#define CredMarshalCredential CredMarshalCredentialW
#else
#define CredMarshalCredential CredMarshalCredentialA
#endif

WINPR_API BOOL CredUnmarshalCredentialW(LPCWSTR cred, PCRED_MARSHAL_TYPE CredType,
                                        PVOID* Credential);

WINPR_API BOOL CredUnmarshalCredentialA(LPCSTR cred, PCRED_MARSHAL_TYPE CredType,
                                        PVOID* Credential);

#ifdef UNICODE
#define CredUnmarshalCredential CredUnmarshalCredentialW
#else
#define CredUnmarshalCredential CredUnmarshalCredentialA
#endif

WINPR_API BOOL CredIsMarshaledCredentialA(LPCSTR MarshaledCredential);
WINPR_API BOOL CredIsMarshaledCredentialW(LPCWSTR MarshaledCredential);
WINPR_API VOID CredFree(PVOID Buffer);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* WINPR_CRED_H_ */
