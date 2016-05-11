/**
 * WinPR: Windows Portable Runtime
 * Active Directory Domain Services Parsing Functions
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

#ifndef WINPR_DSPARSE_H
#define WINPR_DSPARSE_H

#if defined(_WIN32) && !defined(_UWP)

#include <winpr/windows.h>
#include <winpr/rpc.h>

#include <ntdsapi.h>

#else

#include <winpr/crt.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/error.h>

typedef enum
{ 
	DS_NAME_NO_FLAGS = 0x0,
	DS_NAME_FLAG_SYNTACTICAL_ONLY = 0x1,
	DS_NAME_FLAG_EVAL_AT_DC = 0x2,
	DS_NAME_FLAG_GCVERIFY = 0x4,
	DS_NAME_FLAG_TRUST_REFERRAL = 0x8
} DS_NAME_FLAGS;

typedef enum
{ 
	DS_UNKNOWN_NAME = 0,
	DS_FQDN_1779_NAME = 1,
	DS_NT4_ACCOUNT_NAME = 2,
	DS_DISPLAY_NAME = 3,
	DS_UNIQUE_ID_NAME = 6,
	DS_CANONICAL_NAME = 7,
	DS_USER_PRINCIPAL_NAME = 8,
	DS_CANONICAL_NAME_EX = 9,
	DS_SERVICE_PRINCIPAL_NAME = 10,
	DS_SID_OR_SID_HISTORY_NAME = 11,
	DS_DNS_DOMAIN_NAME = 12
} DS_NAME_FORMAT;

typedef enum
{ 
	DS_NAME_NO_ERROR = 0,
	DS_NAME_ERROR_RESOLVING = 1,
	DS_NAME_ERROR_NOT_FOUND = 2,
	DS_NAME_ERROR_NOT_UNIQUE = 3,
	DS_NAME_ERROR_NO_MAPPING = 4,
	DS_NAME_ERROR_DOMAIN_ONLY = 5,
	DS_NAME_ERROR_NO_SYNTACTICAL_MAPPING = 6,
	DS_NAME_ERROR_TRUST_REFERRAL = 7
} DS_NAME_ERROR;

typedef enum
{
	DS_SPN_DNS_HOST = 0,
	DS_SPN_DN_HOST = 1,
	DS_SPN_NB_HOST = 2,
	DS_SPN_DOMAIN = 3,
	DS_SPN_NB_DOMAIN = 4,
	DS_SPN_SERVICE = 5
} DS_SPN_NAME_TYPE;

typedef struct
{
	DWORD status;
	LPTSTR pDomain;
	LPTSTR pName;
} DS_NAME_RESULT_ITEM, *PDS_NAME_RESULT_ITEM;

typedef struct
{
	DWORD cItems;
	PDS_NAME_RESULT_ITEM rItems;
} DS_NAME_RESULT, *PDS_NAME_RESULT;

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API DWORD DsCrackSpnW(LPCWSTR pszSpn, DWORD* pcServiceClass, LPWSTR ServiceClass, DWORD* pcServiceName,
		LPWSTR ServiceName, DWORD* pcInstanceName, LPWSTR InstanceName, USHORT* pInstancePort);

WINPR_API DWORD DsCrackSpnA(LPCSTR pszSpn, LPDWORD pcServiceClass, LPSTR ServiceClass, LPDWORD pcServiceName,
		LPSTR ServiceName, LPDWORD pcInstanceName, LPSTR InstanceName, USHORT* pInstancePort);

#ifdef UNICODE
#define DsCrackSpn	DsCrackSpnW
#else
#define DsCrackSpn	DsCrackSpnA
#endif

WINPR_API DWORD DsMakeSpnW(LPCWSTR ServiceClass, LPCWSTR ServiceName, LPCWSTR InstanceName,
		USHORT InstancePort, LPCWSTR Referrer, DWORD* pcSpnLength, LPWSTR pszSpn);

WINPR_API DWORD DsMakeSpnA(LPCSTR ServiceClass, LPCSTR ServiceName, LPCSTR InstanceName,
		USHORT InstancePort, LPCSTR Referrer, DWORD* pcSpnLength, LPSTR pszSpn);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define DsMakeSpn	DsMakeSpnW
#else
#define DsMakeSpn	DsMakeSpnA
#endif

#endif

#endif /* WINPR_DSPARSE_H */
