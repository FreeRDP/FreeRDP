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

#ifndef WINPR_SSPICLI_H
#define WINPR_SSPICLI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

typedef enum
{
	/* An unknown name type */
	NameUnknown = 0,

	/* The fully qualified distinguished name (for example, CN=Jeff Smith,OU=Users,DC=Engineering,DC=Microsoft,DC=Com) */
	NameFullyQualifiedDN = 1,

	/* 
	 * A legacy account name (for example, Engineering\JSmith).
	 * The domain-only version includes trailing backslashes (\\)
	 */
	NameSamCompatible = 2,

	/*
	 * A "friendly" display name (for example, Jeff Smith).
	 * The display name is not necessarily the defining relative distinguished name (RDN)
	 */
	NameDisplay = 3,

	/* A GUID string that the IIDFromString function returns (for example, {4fa050f0-f561-11cf-bdd9-00aa003a77b6}) */
	NameUniqueId = 6,

	/* 
	 * The complete canonical name (for example, engineering.microsoft.com/software/someone).
	 * The domain-only version includes a trailing forward slash (/)
	 */
	NameCanonical = 7,

	/* The user principal name (for example, someone@example.com) */
	NameUserPrincipal = 8,

	/* 
	 * The same as NameCanonical except that the rightmost forward slash (/)
	 * is replaced with a new line character (\n), even in a domain-only case
	 * (for example, engineering.microsoft.com/software\nJSmith)
	 */
	NameCanonicalEx = 9,

	/* The generalized service principal name (for example, www/www.microsoft.com@microsoft.com)  */
	NameServicePrincipal = 10,

	/* The DNS domain name followed by a backward-slash and the SAM user name */
	NameDnsDomain = 12

} EXTENDED_NAME_FORMAT, *PEXTENDED_NAME_FORMAT;

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API BOOL GetUserNameExA(EXTENDED_NAME_FORMAT NameFormat, LPSTR lpNameBuffer, PULONG nSize);
WINPR_API BOOL GetUserNameExW(EXTENDED_NAME_FORMAT NameFormat, LPWSTR lpNameBuffer, PULONG nSize);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define GetUserNameEx	GetUserNameExW
#else
#define GetUserNameEx	GetUserNameExA
#endif

#endif

#endif /* WINPR_SSPICLI_H */

