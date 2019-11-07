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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/credentials.h>

/*
 * Low-Level Credentials Management Functions:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731(v=vs.85).aspx#low_level_credentials_management_functions
 */

#ifndef _WIN32

BOOL CredWriteW(PCREDENTIALW Credential, DWORD Flags)
{
	return TRUE;
}

BOOL CredWriteA(PCREDENTIALA Credential, DWORD Flags)
{
	return TRUE;
}

BOOL CredReadW(LPCWSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALW* Credential)
{
	return TRUE;
}

BOOL CredReadA(LPCSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALA* Credential)
{
	return TRUE;
}

BOOL CredEnumerateW(LPCWSTR Filter, DWORD Flags, DWORD* Count, PCREDENTIALW** Credential)
{
	return TRUE;
}

BOOL CredEnumerateA(LPCSTR Filter, DWORD Flags, DWORD* Count, PCREDENTIALA** Credential)
{
	return TRUE;
}

BOOL CredWriteDomainCredentialsW(PCREDENTIAL_TARGET_INFORMATIONW TargetInfo,
                                 PCREDENTIALW Credential, DWORD Flags)
{
	return TRUE;
}

BOOL CredWriteDomainCredentialsA(PCREDENTIAL_TARGET_INFORMATIONA TargetInfo,
                                 PCREDENTIALA Credential, DWORD Flags)
{
	return TRUE;
}

BOOL CredReadDomainCredentialsW(PCREDENTIAL_TARGET_INFORMATIONW TargetInfo, DWORD Flags,
                                DWORD* Count, PCREDENTIALW** Credential)
{
	return TRUE;
}

BOOL CredReadDomainCredentialsA(PCREDENTIAL_TARGET_INFORMATIONA TargetInfo, DWORD Flags,
                                DWORD* Count, PCREDENTIALA** Credential)
{
	return TRUE;
}

BOOL CredDeleteW(LPCWSTR TargetName, DWORD Type, DWORD Flags)
{
	return TRUE;
}

BOOL CredDeleteA(LPCSTR TargetName, DWORD Type, DWORD Flags)
{
	return TRUE;
}

BOOL CredRenameW(LPCWSTR OldTargetName, LPCWSTR NewTargetName, DWORD Type, DWORD Flags)
{
	return TRUE;
}

BOOL CredRenameA(LPCSTR OldTargetName, LPCSTR NewTargetName, DWORD Type, DWORD Flags)
{
	return TRUE;
}

BOOL CredGetTargetInfoW(LPCWSTR TargetName, DWORD Flags,
                        PCREDENTIAL_TARGET_INFORMATIONW* TargetInfo)
{
	return TRUE;
}

BOOL CredGetTargetInfoA(LPCSTR TargetName, DWORD Flags, PCREDENTIAL_TARGET_INFORMATIONA* TargetInfo)
{
	return TRUE;
}

BOOL CredMarshalCredentialW(CRED_MARSHAL_TYPE CredType, PVOID Credential,
                            LPWSTR* MarshaledCredential)
{
	return TRUE;
}

BOOL CredMarshalCredentialA(CRED_MARSHAL_TYPE CredType, PVOID Credential,
                            LPSTR* MarshaledCredential)
{
	return TRUE;
}

BOOL CredUnmarshalCredentialW(LPCWSTR MarshaledCredential, PCRED_MARSHAL_TYPE CredType,
                              PVOID* Credential)
{
	return TRUE;
}

BOOL CredUnmarshalCredentialA(LPCSTR MarshaledCredential, PCRED_MARSHAL_TYPE CredType,
                              PVOID* Credential)
{
	return TRUE;
}

BOOL CredIsMarshaledCredentialW(LPCWSTR MarshaledCredential)
{
	return TRUE;
}

BOOL CredIsMarshaledCredentialA(LPCSTR MarshaledCredential)
{
	return TRUE;
}

BOOL CredProtectW(BOOL fAsSelf, LPWSTR pszCredentials, DWORD cchCredentials,
                  LPWSTR pszProtectedCredentials, DWORD* pcchMaxChars,
                  CRED_PROTECTION_TYPE* ProtectionType)
{
	return TRUE;
}

BOOL CredProtectA(BOOL fAsSelf, LPSTR pszCredentials, DWORD cchCredentials,
                  LPSTR pszProtectedCredentials, DWORD* pcchMaxChars,
                  CRED_PROTECTION_TYPE* ProtectionType)
{
	return TRUE;
}

BOOL CredUnprotectW(BOOL fAsSelf, LPWSTR pszProtectedCredentials, DWORD cchCredentials,
                    LPWSTR pszCredentials, DWORD* pcchMaxChars)
{
	return TRUE;
}

BOOL CredUnprotectA(BOOL fAsSelf, LPSTR pszProtectedCredentials, DWORD cchCredentials,
                    LPSTR pszCredentials, DWORD* pcchMaxChars)
{
	return TRUE;
}

BOOL CredIsProtectedW(LPWSTR pszProtectedCredentials, CRED_PROTECTION_TYPE* pProtectionType)
{
	return TRUE;
}

BOOL CredIsProtectedA(LPSTR pszProtectedCredentials, CRED_PROTECTION_TYPE* pProtectionType)
{
	return TRUE;
}

BOOL CredFindBestCredentialW(LPCWSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALW* Credential)
{
	return TRUE;
}

BOOL CredFindBestCredentialA(LPCSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALA* Credential)
{
	return TRUE;
}

BOOL CredGetSessionTypes(DWORD MaximumPersistCount, LPDWORD MaximumPersist)
{
	return TRUE;
}

VOID CredFree(PVOID Buffer)
{
}

#endif
