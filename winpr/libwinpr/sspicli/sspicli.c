/**
 * WinPR: Windows Portable Runtime
 * Security Support Provider Interface
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

#include <winpr/sspicli.h>

/**
 * sspicli.dll:
 *
 * EnumerateSecurityPackagesA
 * EnumerateSecurityPackagesW
 * GetUserNameExW
 * ImportSecurityContextA
 * LogonUserExExW
 * SspiCompareAuthIdentities
 * SspiCopyAuthIdentity
 * SspiDecryptAuthIdentity
 * SspiEncodeAuthIdentityAsStrings
 * SspiEncodeStringsAsAuthIdentity
 * SspiEncryptAuthIdentity
 * SspiExcludePackage
 * SspiFreeAuthIdentity
 * SspiGetTargetHostName
 * SspiIsAuthIdentityEncrypted
 * SspiLocalFree
 * SspiMarshalAuthIdentity
 * SspiPrepareForCredRead
 * SspiPrepareForCredWrite
 * SspiUnmarshalAuthIdentity
 * SspiValidateAuthIdentity
 * SspiZeroAuthIdentity
 */

#ifndef _WIN32

#include <unistd.h>
#include <winpr/crt.h>

BOOL GetUserNameExA(EXTENDED_NAME_FORMAT NameFormat, LPSTR lpNameBuffer, PULONG nSize)
{
	int length;
	char* login;

	switch (NameFormat)
	{
		case NameSamCompatible:

			login = getlogin();
			length = strlen(login);

			if (*nSize >= length)
			{
				CopyMemory(lpNameBuffer, login, length + 1);
				return 1;
			}
			else
			{
				*nSize = length + 1;
			}

			break;

		case NameFullyQualifiedDN:
		case NameDisplay:
		case NameUniqueId:
		case NameCanonical:
		case NameUserPrincipal:
		case NameCanonicalEx:
		case NameServicePrincipal:
		case NameDnsDomain:
			break;

		default:
			break;
	}

	return 0;
}

BOOL GetUserNameExW(EXTENDED_NAME_FORMAT NameFormat, LPWSTR lpNameBuffer, PULONG nSize)
{
	return 0;
}

#endif

