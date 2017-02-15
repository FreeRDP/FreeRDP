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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/dsparse.h>

/**
 * dsparse.dll:
 *
 * DsCrackSpnA
 * DsCrackSpnW
 * DsCrackUnquotedMangledRdnA
 * DsCrackUnquotedMangledRdnW
 * DsGetRdnW
 * DsIsMangledDnA
 * DsIsMangledDnW
 * DsIsMangledRdnValueA
 * DsIsMangledRdnValueW
 * DsMakeSpnA
 * DsMakeSpnW
 * DsQuoteRdnValueA
 * DsQuoteRdnValueW
 * DsUnquoteRdnValueA
 * DsUnquoteRdnValueW
 */

#if !defined(_WIN32) || defined(_UWP)

DWORD DsCrackSpnW(LPCWSTR pszSpn, DWORD* pcServiceClass, LPWSTR ServiceClass, DWORD* pcServiceName,
		LPWSTR ServiceName, DWORD* pcInstanceName, LPWSTR InstanceName, USHORT* pInstancePort)
{
	return 0;
}

DWORD DsCrackSpnA(LPCSTR pszSpn, LPDWORD pcServiceClass, LPSTR ServiceClass, LPDWORD pcServiceName,
		LPSTR ServiceName, LPDWORD pcInstanceName, LPSTR InstanceName, USHORT* pInstancePort)
{
	return 0;
}

DWORD DsMakeSpnW(LPCWSTR ServiceClass, LPCWSTR ServiceName, LPCWSTR InstanceName,
		USHORT InstancePort, LPCWSTR Referrer, DWORD* pcSpnLength, LPWSTR pszSpn)
{
	return 0;
}

DWORD DsMakeSpnA(LPCSTR ServiceClass, LPCSTR ServiceName, LPCSTR InstanceName,
		USHORT InstancePort, LPCSTR Referrer, DWORD* pcSpnLength, LPSTR pszSpn)
{
	DWORD SpnLength;
	DWORD ServiceClassLength;
	DWORD ServiceNameLength;

	if ((*pcSpnLength != 0) && (pszSpn == NULL))
		return ERROR_INVALID_PARAMETER;

	ServiceClassLength = (DWORD) strlen(ServiceClass);
	ServiceNameLength = (DWORD) strlen(ServiceName);

	SpnLength = ServiceClassLength + 1 + ServiceNameLength + 1;

	if ((*pcSpnLength == 0) || (*pcSpnLength < SpnLength))
	{
		*pcSpnLength = SpnLength;
		return ERROR_BUFFER_OVERFLOW;
	}

	sprintf_s(pszSpn, *pcSpnLength, "%s/%s", ServiceClass, ServiceName);

	return ERROR_SUCCESS;
}

#endif
