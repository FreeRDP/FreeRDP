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

#include <winpr/config.h>

#include <winpr/dsparse.h>
#include <winpr/assert.h>

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

DWORD DsMakeSpnW(LPCWSTR ServiceClass, LPCWSTR ServiceName, LPCWSTR InstanceName,
                 USHORT InstancePort, LPCWSTR Referrer, DWORD* pcSpnLength, LPWSTR pszSpn)
{
	DWORD res = ERROR_OUTOFMEMORY;
	char* ServiceClassA = NULL;
	char* ServiceNameA = NULL;
	char* InstanceNameA = NULL;
	char* ReferrerA = NULL;
	char* pszSpnA = NULL;
	DWORD length;

	WINPR_ASSERT(ServiceClass);
	WINPR_ASSERT(ServiceName);
	WINPR_ASSERT(pcSpnLength);

	length = *pcSpnLength;
	if ((length > 0) && pszSpn)
		pszSpnA = calloc(length + 1, sizeof(char));

	if (ServiceClass)
	{
		int rc = ConvertFromUnicode(CP_UTF8, 0, ServiceClass, -1, &ServiceClassA, 0, NULL, NULL);
		if (rc <= 0)
			goto fail;
	}
	if (ServiceName)
	{
		int rc = ConvertFromUnicode(CP_UTF8, 0, ServiceName, -1, &ServiceNameA, 0, NULL, NULL);
		if (rc <= 0)
			goto fail;
	}
	if (InstanceName)
	{
		int rc = ConvertFromUnicode(CP_UTF8, 0, InstanceName, -1, &InstanceNameA, 0, NULL, NULL);
		if (rc <= 0)
			goto fail;
	}
	if (Referrer)
	{
		int rc = ConvertFromUnicode(CP_UTF8, 0, Referrer, -1, &ReferrerA, 0, NULL, NULL);
		if (rc <= 0)
			goto fail;
	}
	res = DsMakeSpnA(ServiceClassA, ServiceNameA, InstanceNameA, InstancePort, ReferrerA,
	                 pcSpnLength, pszSpnA);

	if (res == ERROR_SUCCESS)
	{
		int rc = ConvertToUnicode(CP_UTF8, 0, pszSpnA, *pcSpnLength, &pszSpn, length);
		if (rc <= 0)
			res = ERROR_OUTOFMEMORY;
	}

fail:
	free(ServiceClassA);
	free(ServiceNameA);
	free(InstanceNameA);
	free(ReferrerA);
	free(pszSpnA);
	return res;
}

DWORD DsMakeSpnA(LPCSTR ServiceClass, LPCSTR ServiceName, LPCSTR InstanceName, USHORT InstancePort,
                 LPCSTR Referrer, DWORD* pcSpnLength, LPSTR pszSpn)
{
	DWORD SpnLength;
	DWORD ServiceClassLength;
	DWORD ServiceNameLength;

	WINPR_ASSERT(ServiceClass);
	WINPR_ASSERT(ServiceName);
	WINPR_ASSERT(pcSpnLength);

	WINPR_UNUSED(InstanceName);
	WINPR_UNUSED(InstancePort);
	WINPR_UNUSED(Referrer);

	if ((*pcSpnLength != 0) && (pszSpn == NULL))
		return ERROR_INVALID_PARAMETER;

	ServiceClassLength = (DWORD)strlen(ServiceClass);
	ServiceNameLength = (DWORD)strlen(ServiceName);

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
