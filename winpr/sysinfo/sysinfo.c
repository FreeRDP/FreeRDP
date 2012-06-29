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

#include <winpr/sysinfo.h>

/**
 * api-ms-win-core-sysinfo-l1-1-1.dll:
 *
 * EnumSystemFirmwareTables
 * GetComputerNameExA
 * GetComputerNameExW
 * GetDynamicTimeZoneInformation
 * GetLocalTime
 * GetLogicalProcessorInformation
 * GetLogicalProcessorInformationEx
 * GetNativeSystemInfo
 * GetProductInfo
 * GetSystemDirectoryA
 * GetSystemDirectoryW
 * GetSystemFirmwareTable
 * GetSystemInfo
 * GetSystemTime
 * GetSystemTimeAdjustment
 * GetSystemTimeAsFileTime
 * GetSystemWindowsDirectoryA
 * GetSystemWindowsDirectoryW
 * GetTickCount
 * GetTickCount64
 * GetTimeZoneInformation
 * GetTimeZoneInformationForYear
 * GetVersion
 * GetVersionExA
 * GetVersionExW
 * GetWindowsDirectoryA
 * GetWindowsDirectoryW
 * GlobalMemoryStatusEx
 * SetComputerNameExW
 * SetDynamicTimeZoneInformation
 * SetLocalTime
 * SetSystemTime
 * SetTimeZoneInformation
 * SystemTimeToFileTime
 * SystemTimeToTzSpecificLocalTime
 * TzSpecificLocalTimeToSystemTime
 * VerSetConditionMask
 */

#ifndef _WIN32

#include <unistd.h>
#include <winpr/crt.h>

BOOL GetComputerNameExA(COMPUTER_NAME_FORMAT NameType, LPSTR lpBuffer, LPDWORD nSize)
{
	char hostname[256];
	int hostname_length;

	gethostname(hostname, sizeof(hostname));
	hostname_length = strlen(hostname);

	switch (NameType)
	{
		case ComputerNameNetBIOS:
		case ComputerNameDnsHostname:
		case ComputerNameDnsDomain:
		case ComputerNameDnsFullyQualified:
		case ComputerNamePhysicalNetBIOS:
		case ComputerNamePhysicalDnsHostname:
		case ComputerNamePhysicalDnsDomain:
		case ComputerNamePhysicalDnsFullyQualified:

			if (*nSize <= hostname_length)
			{
				*nSize = hostname_length + 1;
				return 0;
			}

			if (!lpBuffer)
				return 0;

			CopyMemory(lpBuffer, hostname, hostname_length + 1);

			break;

		default:
			return 0;
			break;
	}

	return 1;
}

BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize)
{
	printf("GetComputerNameExW unimplemented\n");
	return 0;
}

/* OSVERSIONINFOEX Structure:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms724833
 */

BOOL GetVersionExA(LPOSVERSIONINFOA lpVersionInformation)
{
	/* Windows 7 SP1 Version Info */

	if ((lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOA)) ||
		(lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXA)))
	{
		lpVersionInformation->dwMajorVersion = 6;
		lpVersionInformation->dwMinorVersion = 1;
		lpVersionInformation->dwBuildNumber = 7601;
		lpVersionInformation->dwPlatformId = VER_PLATFORM_WIN32_NT;
		ZeroMemory(lpVersionInformation->szCSDVersion, sizeof(lpVersionInformation->szCSDVersion));

		if (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXA))
		{
			LPOSVERSIONINFOEXA lpVersionInformationEx = (LPOSVERSIONINFOEXA) lpVersionInformation;

			lpVersionInformationEx->wServicePackMajor = 1;
			lpVersionInformationEx->wServicePackMinor = 0;
			lpVersionInformationEx->wSuiteMask = 0;
			lpVersionInformationEx->wProductType = VER_NT_WORKSTATION;
			lpVersionInformationEx->wReserved = 0;
		}

		return 1;
	}

	return 0;
}

BOOL GetVersionExW(LPOSVERSIONINFOW lpVersionInformation)
{
	printf("GetVersionExW unimplemented\n");
	return 1;
}

#endif
