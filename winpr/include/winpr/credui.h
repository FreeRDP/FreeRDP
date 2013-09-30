/**
 * WinPR: Windows Portable Runtime
 * Credentials Management UI
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

#ifndef WINPR_CREDUI_H
#define WINPR_CREDUI_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/nt.h>
#include <winpr/sspi.h>
#include <winpr/error.h>
#include <winpr/credentials.h>

#ifdef _WIN32

#include <wincred.h>

#else

#define CREDUI_MAX_MESSAGE_LENGTH			32767
#define CREDUI_MAX_CAPTION_LENGTH			128
#define CREDUI_MAX_GENERIC_TARGET_LENGTH		CRED_MAX_GENERIC_TARGET_NAME_LENGTH
#define CREDUI_MAX_DOMAIN_TARGET_LENGTH			CRED_MAX_DOMAIN_TARGET_NAME_LENGTH
#define CREDUI_MAX_USERNAME_LENGTH			CRED_MAX_USERNAME_LENGTH
#define CREDUI_MAX_PASSWORD_LENGTH			(CRED_MAX_CREDENTIAL_BLOB_SIZE / 2)

#define CREDUI_FLAGS_INCORRECT_PASSWORD			0x00000001
#define CREDUI_FLAGS_DO_NOT_PERSIST			0x00000002
#define CREDUI_FLAGS_REQUEST_ADMINISTRATOR		0x00000004
#define CREDUI_FLAGS_EXCLUDE_CERTIFICATES		0x00000008
#define CREDUI_FLAGS_REQUIRE_CERTIFICATE		0x00000010
#define CREDUI_FLAGS_SHOW_SAVE_CHECK_BOX		0x00000040
#define CREDUI_FLAGS_ALWAYS_SHOW_UI			0x00000080
#define CREDUI_FLAGS_REQUIRE_SMARTCARD			0x00000100
#define CREDUI_FLAGS_PASSWORD_ONLY_OK			0x00000200
#define CREDUI_FLAGS_VALIDATE_USERNAME			0x00000400
#define CREDUI_FLAGS_COMPLETE_USERNAME			0x00000800
#define CREDUI_FLAGS_PERSIST				0x00001000
#define CREDUI_FLAGS_SERVER_CREDENTIAL			0x00004000
#define CREDUI_FLAGS_EXPECT_CONFIRMATION		0x00020000
#define CREDUI_FLAGS_GENERIC_CREDENTIALS		0x00040000
#define CREDUI_FLAGS_USERNAME_TARGET_CREDENTIALS	0x00080000
#define CREDUI_FLAGS_KEEP_USERNAME			0x00100000

#define CREDUIWIN_GENERIC				0x00000001
#define CREDUIWIN_CHECKBOX				0x00000002
#define CREDUIWIN_AUTHPACKAGE_ONLY			0x00000010
#define CREDUIWIN_IN_CRED_ONLY				0x00000020
#define CREDUIWIN_ENUMERATE_ADMINS			0x00000100
#define CREDUIWIN_ENUMERATE_CURRENT_USER		0x00000200
#define CREDUIWIN_SECURE_PROMPT				0x00001000
#define CREDUIWIN_PACK_32_WOW				0x10000000

typedef struct _CREDUI_INFOA
{
	DWORD cbSize;
	HWND hwndParent;
	PCSTR pszMessageText;
	PCSTR pszCaptionText;
	HBITMAP hbmBanner;
} CREDUI_INFOA, *PCREDUI_INFOA;

typedef struct _CREDUI_INFOW
{
	DWORD cbSize;
	HWND hwndParent;
	PCWSTR pszMessageText;
	PCWSTR pszCaptionText;
	HBITMAP hbmBanner;
} CREDUI_INFOW, *PCREDUI_INFOW;

#ifdef UNICODE
#define CREDUI_INFO	CREDUI_INFOW
#define PCREDUI_INFO	PCREDUI_INFOW
#else
#define CREDUI_INFO	CREDUI_INFOA
#define PCREDUI_INFO	PCREDUI_INFOA
#endif

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API DWORD CredUIPromptForCredentialsW(PCREDUI_INFOW pUiInfo, PCWSTR pszTargetName,
		PCtxtHandle pContext, DWORD dwAuthError, PWSTR pszUserName, ULONG ulUserNameBufferSize,
		PWSTR pszPassword, ULONG ulPasswordBufferSize, BOOL* save, DWORD dwFlags);
WINPR_API DWORD CredUIPromptForCredentialsA(PCREDUI_INFOA pUiInfo, PCSTR pszTargetName,
		PCtxtHandle pContext, DWORD dwAuthError, PSTR pszUserName, ULONG ulUserNameBufferSize,
		PSTR pszPassword, ULONG ulPasswordBufferSize, BOOL* save, DWORD dwFlags);

WINPR_API DWORD CredUIParseUserNameW(CONST WCHAR* UserName, WCHAR* user, ULONG userBufferSize, WCHAR* domain, ULONG domainBufferSize);
WINPR_API DWORD CredUIParseUserNameA(CONST CHAR* userName, CHAR* user, ULONG userBufferSize, CHAR* domain, ULONG domainBufferSize);

WINPR_API DWORD CredUICmdLinePromptForCredentialsW(PCWSTR pszTargetName, PCtxtHandle pContext,
		DWORD dwAuthError, PWSTR UserName, ULONG ulUserBufferSize, PWSTR pszPassword,
		ULONG ulPasswordBufferSize, PBOOL pfSave, DWORD dwFlags);
WINPR_API DWORD CredUICmdLinePromptForCredentialsA(PCSTR pszTargetName, PCtxtHandle pContext,
		DWORD dwAuthError, PSTR UserName, ULONG ulUserBufferSize, PSTR pszPassword,
		ULONG ulPasswordBufferSize, PBOOL pfSave, DWORD dwFlags);

WINPR_API DWORD CredUIConfirmCredentialsW(PCWSTR pszTargetName, BOOL bConfirm);
WINPR_API DWORD CredUIConfirmCredentialsA(PCSTR pszTargetName, BOOL bConfirm);

WINPR_API DWORD CredUIStoreSSOCredW(PCWSTR pszRealm, PCWSTR pszUsername, PCWSTR pszPassword, BOOL bPersist);
WINPR_API DWORD CredUIStoreSSOCredA(PCSTR pszRealm, PCSTR pszUsername, PCSTR pszPassword, BOOL bPersist);

WINPR_API DWORD CredUIReadSSOCredW(PCWSTR pszRealm, PWSTR* ppszUsername);
WINPR_API DWORD CredUIReadSSOCredA(PCSTR pszRealm, PSTR* ppszUsername);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define CredUIPromptForCredentials		CredUIPromptForCredentialsW
#define CredUIParseUserName			CredUIParseUserNameW
#define CredUICmdLinePromptForCredentials	CredUICmdLinePromptForCredentialsW
#define CredUIConfirmCredentials		CredUIConfirmCredentialsW
#define CredUIStoreSSOCred			CredUIStoreSSOCredW
#define CredUIReadSSOCred			CredUIReadSSOCredW
#else
#define CredUIPromptForCredentials		CredUIPromptForCredentialsA
#define CredUIParseUserName			CredUIParseUserNameA
#define CredUICmdLinePromptForCredentials	CredUICmdLinePromptForCredentialsA
#define CredUIConfirmCredentials		CredUIConfirmCredentialsA
#define CredUIStoreSSOCred			CredUIStoreSSOCredA
#define CredUIReadSSOCred			CredUIReadSSOCredA
#endif

#endif

#endif /* WINPR_CREDUI_H */

