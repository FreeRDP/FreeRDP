/**
 * WinPR: Windows Portable Runtime
 * System Information
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

#ifndef WINPR_SYSINFO_H
#define WINPR_SYSINFO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

typedef enum _COMPUTER_NAME_FORMAT
{
	ComputerNameNetBIOS,
	ComputerNameDnsHostname,
	ComputerNameDnsDomain,
	ComputerNameDnsFullyQualified,
	ComputerNamePhysicalNetBIOS,
	ComputerNamePhysicalDnsHostname,
	ComputerNamePhysicalDnsDomain,
	ComputerNamePhysicalDnsFullyQualified,
	ComputerNameMax
} COMPUTER_NAME_FORMAT;

WINPR_API BOOL GetComputerNameExA(COMPUTER_NAME_FORMAT NameType, LPSTR lpBuffer, LPDWORD nSize);
WINPR_API BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize);

#ifdef UNICODE
#define GetComputerNameEx	GetComputerNameExW
#else
#define GetComputerNameEx	GetComputerNameExA
#endif

typedef struct _OSVERSIONINFOA
{
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	CHAR szCSDVersion[128];
} OSVERSIONINFOA, *POSVERSIONINFOA, *LPOSVERSIONINFOA;

typedef struct _OSVERSIONINFOW
{
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	WCHAR szCSDVersion[128];
} OSVERSIONINFOW, *POSVERSIONINFOW, *LPOSVERSIONINFOW;

typedef struct _OSVERSIONINFOEXA
{
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	CHAR szCSDVersion[128];
	WORD wServicePackMajor;
	WORD wServicePackMinor;
	WORD wSuiteMask;
	BYTE wProductType;
	BYTE wReserved;
} OSVERSIONINFOEXA, *POSVERSIONINFOEXA, *LPOSVERSIONINFOEXA;

typedef struct _OSVERSIONINFOEXW
{
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	WCHAR szCSDVersion[128];
	WORD wServicePackMajor;
	WORD wServicePackMinor;
	WORD wSuiteMask;
	BYTE wProductType;
	BYTE wReserved;
} OSVERSIONINFOEXW, *POSVERSIONINFOEXW, *LPOSVERSIONINFOEXW;

#ifdef UNICODE
#define OSVERSIONINFO		OSVERSIONINFOW
#define OSVERSIONINFOEX		OSVERSIONINFOEXW
#define POSVERSIONINFO		POSVERSIONINFOW
#define POSVERSIONINFOEX	POSVERSIONINFOEXW
#define LPOSVERSIONINFO		LPOSVERSIONINFOW
#define LPOSVERSIONINFOEX	LPOSVERSIONINFOEXW
#else
#define OSVERSIONINFO		OSVERSIONINFOA
#define OSVERSIONINFOEX		OSVERSIONINFOEXA
#define POSVERSIONINFO		POSVERSIONINFOA
#define POSVERSIONINFOEX	POSVERSIONINFOEXA
#define LPOSVERSIONINFO		LPOSVERSIONINFOA
#define LPOSVERSIONINFOEX	LPOSVERSIONINFOEXA
#endif

#define VER_PLATFORM_WIN32_NT			0x00000002

#define VER_SUITE_BACKOFFICE			0x00000004
#define VER_SUITE_BLADE				0x00000400
#define VER_SUITE_COMPUTE_SERVER		0x00004000
#define VER_SUITE_DATACENTER			0x00000080
#define VER_SUITE_ENTERPRISE			0x00000002
#define VER_SUITE_EMBEDDEDNT			0x00000040
#define VER_SUITE_PERSONAL			0x00000200
#define VER_SUITE_SINGLEUSERTS			0x00000100
#define VER_SUITE_SMALLBUSINESS			0x00000001
#define VER_SUITE_SMALLBUSINESS_RESTRICTED	0x00000020
#define VER_SUITE_STORAGE_SERVER		0x00002000
#define VER_SUITE_TERMINAL			0x00000010
#define VER_SUITE_WH_SERVER			0x00008000

#define VER_NT_DOMAIN_CONTROLLER		0x0000002
#define VER_NT_SERVER				0x0000003
#define VER_NT_WORKSTATION			0x0000001

WINPR_API BOOL GetVersionExA(LPOSVERSIONINFOA lpVersionInformation);
WINPR_API BOOL GetVersionExW(LPOSVERSIONINFOW lpVersionInformation);

#ifdef UNICODE
#define GetVersionEx	GetVersionExW
#else
#define GetVersionEx	GetVersionExA
#endif

WINPR_API VOID GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);

#endif

#endif /* WINPR_SYSINFO_H */

