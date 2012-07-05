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

#ifdef _WIN32

#include <winpr/windows.h>

#else

#include <winpr/crt.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

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

#ifdef UNICODE
#define DsMakeSpn	DsMakeSpnW
#else
#define DsMakeSpn	DsMakeSpnA
#endif

#endif

#endif /* WINPR_DSPARSE_H */
/* Modeline for vim. Don't delete */
/* vim: set cindent:noet:sw=8:ts=8 */
