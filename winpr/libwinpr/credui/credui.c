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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/credui.h>

/*
 * Credentials Management UI Functions:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731(v=vs.85).aspx#credentials_management_ui_functions
 */

#ifndef _WIN32

DWORD CredUIPromptForCredentialsW(PCREDUI_INFOW pUiInfo, PCWSTR pszTargetName,
		PCtxtHandle pContext, DWORD dwAuthError, PWSTR pszUserName, ULONG ulUserNameBufferSize,
		PWSTR pszPassword, ULONG ulPasswordBufferSize, BOOL* save, DWORD dwFlags)
{
	return 0;
}

DWORD CredUIPromptForCredentialsA(PCREDUI_INFOA pUiInfo, PCSTR pszTargetName,
		PCtxtHandle pContext, DWORD dwAuthError, PSTR pszUserName, ULONG ulUserNameBufferSize,
		PSTR pszPassword, ULONG ulPasswordBufferSize, BOOL* save, DWORD dwFlags)
{
	return 0;
}

DWORD CredUIParseUserNameW(CONST WCHAR* UserName, WCHAR* user, ULONG userBufferSize, WCHAR* domain, ULONG domainBufferSize)
{
	return 0;
}

DWORD CredUIParseUserNameA(CONST CHAR* userName, CHAR* user, ULONG userBufferSize, CHAR* domain, ULONG domainBufferSize)
{
	return 0;
}

DWORD CredUICmdLinePromptForCredentialsW(PCWSTR pszTargetName, PCtxtHandle pContext,
		DWORD dwAuthError, PWSTR UserName, ULONG ulUserBufferSize, PWSTR pszPassword,
		ULONG ulPasswordBufferSize, PBOOL pfSave, DWORD dwFlags)
{
	return 0;
}

DWORD CredUICmdLinePromptForCredentialsA(PCSTR pszTargetName, PCtxtHandle pContext,
		DWORD dwAuthError, PSTR UserName, ULONG ulUserBufferSize, PSTR pszPassword,
		ULONG ulPasswordBufferSize, PBOOL pfSave, DWORD dwFlags)
{
	return 0;
}

DWORD CredUIConfirmCredentialsW(PCWSTR pszTargetName, BOOL bConfirm)
{
	return 0;
}

DWORD CredUIConfirmCredentialsA(PCSTR pszTargetName, BOOL bConfirm)
{
	return 0;
}

DWORD CredUIStoreSSOCredW(PCWSTR pszRealm, PCWSTR pszUsername, PCWSTR pszPassword, BOOL bPersist)
{
	return 0;
}

DWORD CredUIStoreSSOCredA(PCSTR pszRealm, PCSTR pszUsername, PCSTR pszPassword, BOOL bPersist)
{
	return 0;
}

DWORD CredUIReadSSOCredW(PCWSTR pszRealm, PWSTR* ppszUsername)
{
	return 0;
}

DWORD CredUIReadSSOCredA(PCSTR pszRealm, PSTR* ppszUsername)
{
	return 0;
}

#endif
