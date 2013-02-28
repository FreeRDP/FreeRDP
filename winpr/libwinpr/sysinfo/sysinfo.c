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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
 * GetSystemInfo
 * GetNativeSystemInfo
 * GetProductInfo
 * GetSystemDirectoryA
 * GetSystemDirectoryW
 * GetSystemFirmwareTable
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

#include <time.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>
#include <winpr/platform.h>

#if defined(__MACOSX__) || \
defined(__FreeBSD__) || defined(__NetBSD__) || \
defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/sysctl.h>
#endif

DWORD GetProcessorArchitecture()
{
	DWORD cpuArch = PROCESSOR_ARCHITECTURE_UNKNOWN;

#if defined(_M_AMD64)
	cpuArch = PROCESSOR_ARCHITECTURE_AMD64;
#elif defined(_M_IX86)
	cpuArch = PROCESSOR_ARCHITECTURE_INTEL;
#elif defined(_M_ARM)
	cpuArch = PROCESSOR_ARCHITECTURE_ARM;
#elif defined(_M_IA64)
	cpuArch = PROCESSOR_ARCHITECTURE_IA64;
#elif defined(_M_MIPS)
	cpuArch = PROCESSOR_ARCHITECTURE_MIPS;
#elif defined(_M_PPC)
	cpuArch = PROCESSOR_ARCHITECTURE_PPC;
#elif defined(_M_ALPHA)
	cpuArch = PROCESSOR_ARCHITECTURE_ALPHA;
#endif

	return cpuArch;
}

DWORD GetNumberOfProcessors()
{
	DWORD numCPUs = 1;

	/* TODO: Android and iOS */

#if defined(__linux__) || defined(__sun) || defined(_AIX)
	numCPUs = (DWORD) sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__MACOSX__) || \
	defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__)
	{
		int mib[4];
		size_t length = sizeof(numCPUs);

		mib[0] = CTL_HW;
		mib[1] = HW_AVAILCPU;

		sysctl(mib, 2, &numCPUs, &length, NULL, 0);

		if (numCPUs < 1)
		{
			mib[1] = HW_NCPU;
			sysctl(mib, 2, &numCPUs, &length, NULL, 0);

			if (numCPUs < 1)
				numCPUs = 1;
		}
	}
#elif defined(__hpux)
	numCPUs = (DWORD) mpctl(MPC_GETNUMSPUS, NULL, NULL);
#elif defined(__sgi)
	numCPUs = (DWORD) sysconf(_SC_NPROC_ONLN);
#endif

	return numCPUs;
}

void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
	lpSystemInfo->wProcessorArchitecture = GetProcessorArchitecture();
	lpSystemInfo->wReserved = 0;

	lpSystemInfo->dwPageSize = 0;
	lpSystemInfo->lpMinimumApplicationAddress = NULL;
	lpSystemInfo->lpMaximumApplicationAddress = NULL;
	lpSystemInfo->dwActiveProcessorMask = 0;

	lpSystemInfo->dwNumberOfProcessors = GetNumberOfProcessors();
	lpSystemInfo->dwProcessorType = 0;

	lpSystemInfo->dwAllocationGranularity = 0;

	lpSystemInfo->wProcessorLevel = 0;
	lpSystemInfo->wProcessorRevision = 0;
}

void GetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
	GetSystemInfo(lpSystemInfo);
}

BOOL GetComputerNameA(LPSTR lpBuffer, LPDWORD lpnSize)
{
	char* dot;
	int length;
	char hostname[256];

	gethostname(hostname, sizeof(hostname));
	length = strlen(hostname);

	dot = strchr(hostname, '.');

	if (dot)
		length = dot - hostname;

	if (*lpnSize <= length)
	{
		*lpnSize = length + 1;
		return 0;
	}

	if (!lpBuffer)
		return 0;

	CopyMemory(lpBuffer, hostname, length);
	lpBuffer[length] = '\0';

	return TRUE;
}

BOOL GetComputerNameExA(COMPUTER_NAME_FORMAT NameType, LPSTR lpBuffer, LPDWORD lpnSize)
{
	int length;
	char hostname[256];

	if ((NameType == ComputerNameNetBIOS) || (NameType == ComputerNamePhysicalNetBIOS))
		return GetComputerNameA(lpBuffer, lpnSize);

	gethostname(hostname, sizeof(hostname));
	length = strlen(hostname);

	switch (NameType)
	{
		case ComputerNameDnsHostname:
		case ComputerNameDnsDomain:
		case ComputerNameDnsFullyQualified:
		case ComputerNamePhysicalDnsHostname:
		case ComputerNamePhysicalDnsDomain:
		case ComputerNamePhysicalDnsFullyQualified:

			if (*lpnSize <= length)
			{
				*lpnSize = length + 1;
				return FALSE;
			}

			if (!lpBuffer)
				return FALSE;

			CopyMemory(lpBuffer, hostname, length);
			lpBuffer[length] = '\0';

			break;

		default:
			return FALSE;
			break;
	}

	return TRUE;
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

VOID GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
	ULARGE_INTEGER time64;

	time64.u.HighPart = 0;

	/* time represented in tenths of microseconds since midnight of January 1, 1601 */

	time64.QuadPart = time(NULL) + 11644473600LL; /* Seconds since January 1, 1601 */
	time64.QuadPart *= 10000000; /* Convert timestamp to tenths of a microsecond */

	lpSystemTimeAsFileTime->dwLowDateTime = time64.LowPart;
	lpSystemTimeAsFileTime->dwHighDateTime = time64.HighPart;
}

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW	4
#endif

DWORD GetTickCount(void)
{
	DWORD ticks = 0;

#ifdef __linux__

	struct timespec ts;

	if (!clock_gettime(CLOCK_MONOTONIC_RAW, &ts))
		ticks = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);

#else

	/**
	 * FIXME: this is relative to the Epoch time, and we
	 * need to return a value relative to the system uptime.
	 */

	struct timeval tv;

	if (!gettimeofday(&tv, NULL))
		ticks = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

#endif

	return ticks;
}

#endif
