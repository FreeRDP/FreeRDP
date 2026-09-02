/**
 * WinPR: Windows Portable Runtime
 * System Information
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#include <winpr/assert.h>
#include <winpr/sysinfo.h>
#include <winpr/platform.h>

#if defined(ANDROID)
#include "cpufeatures/cpu-features.h"
#endif

#if defined(__linux__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#endif

#if defined(__MACOSX__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#endif

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if !defined(_WIN32)
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 199309L)
#include <time.h>
#elif !defined(__APPLE__)
#include <sys/time.h>
#include <sys/sysinfo.h>
#endif
#endif

#include "../log.h"
#define TAG WINPR_TAG("sysinfo")

#define FILETIME_TO_UNIX_OFFSET_S 11644473600UL

#if defined(__MACH__) && defined(__APPLE__)

#include <mach/mach_time.h>

static UINT64 scaleHighPrecision(UINT64 i, UINT32 numer, UINT32 denom)
{
	UINT64 high = (i >> 32) * numer;
	UINT64 low = (i & 0xffffffffull) * numer / denom;
	UINT64 highRem = ((high % denom) << 32) / denom;
	high /= denom;
	return (high << 32) + highRem + low;
}

static UINT64 mac_get_time_ns(void)
{
	mach_timebase_info_data_t timebase = WINPR_C_ARRAY_INIT;
	mach_timebase_info(&timebase);
	UINT64 t = mach_absolute_time();
	return scaleHighPrecision(t, timebase.numer, timebase.denom);
}

#endif

/**
 * api-ms-win-core-sysinfo-l1-1-1.dll:
 *
 * EnumSystemFirmwareTables
 * GetSystemFirmwareTable
 * GetLogicalProcessorInformation
 * GetLogicalProcessorInformationEx
 * GetProductInfo
 * GetSystemDirectoryA
 * GetSystemDirectoryW
 * GetSystemTimeAdjustment
 * GetSystemWindowsDirectoryA
 * GetSystemWindowsDirectoryW
 * GetWindowsDirectoryA
 * GetWindowsDirectoryW
 * GlobalMemoryStatusEx
 * SetComputerNameExW
 * VerSetConditionMask
 */

#ifndef _WIN32

#include <time.h>
#include <sys/time.h>

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>
#include <winpr/platform.h>

#if defined(__MACOSX__) || defined(__IOS__)
#include <sys/sysctl.h>
#endif

static WORD GetProcessorArchitecture(void)
{
	WORD cpuArch = PROCESSOR_ARCHITECTURE_UNKNOWN;
#if defined(ANDROID)
	AndroidCpuFamily family = android_getCpuFamily();

	switch (family)
	{
		case ANDROID_CPU_FAMILY_ARM:
			return PROCESSOR_ARCHITECTURE_ARM;

		case ANDROID_CPU_FAMILY_X86:
			return PROCESSOR_ARCHITECTURE_INTEL;

		case ANDROID_CPU_FAMILY_MIPS:
			return PROCESSOR_ARCHITECTURE_MIPS;

		case ANDROID_CPU_FAMILY_ARM64:
			return PROCESSOR_ARCHITECTURE_ARM64;

		case ANDROID_CPU_FAMILY_X86_64:
			return PROCESSOR_ARCHITECTURE_AMD64;

		case ANDROID_CPU_FAMILY_MIPS64:
			return PROCESSOR_ARCHITECTURE_MIPS64;

		default:
			return PROCESSOR_ARCHITECTURE_UNKNOWN;
	}
#elif defined(__MACOSX__) || defined(__IOS__)
	int32_t val = 0;
	size_t len = sizeof(val);

	const int rc = sysctlbyname("hw.cputype", &val, &len, nullptr, 0);
	if (rc != 0)
		return PROCESSOR_ARCHITECTURE_UNKNOWN;

	switch (val)
	{
		case CPU_TYPE_X86:
			return PROCESSOR_ARCHITECTURE_INTEL;
		case CPU_TYPE_X86_64:
			return PROCESSOR_ARCHITECTURE_AMD64;
		case CPU_TYPE_ARM:
			return PROCESSOR_ARCHITECTURE_ARM;
		case CPU_TYPE_ARM64:
			return PROCESSOR_ARCHITECTURE_ARM64;
		case CPU_TYPE_ARM64_32:
			return PROCESSOR_ARCHITECTURE_ARM;
		default:
			return PROCESSOR_ARCHITECTURE_UNKNOWN;
	}

#elif defined(_M_ARM)
	cpuArch = PROCESSOR_ARCHITECTURE_ARM;
#elif defined(_M_IX86)
	cpuArch = PROCESSOR_ARCHITECTURE_INTEL;
#elif defined(_M_MIPS64)
	/* Needs to be before __mips__ since the compiler defines both */
	cpuArch = PROCESSOR_ARCHITECTURE_MIPS64;
#elif defined(_M_MIPS)
	cpuArch = PROCESSOR_ARCHITECTURE_MIPS;
#elif defined(_M_ARM64)
	cpuArch = PROCESSOR_ARCHITECTURE_ARM64;
#elif defined(_M_AMD64)
	cpuArch = PROCESSOR_ARCHITECTURE_AMD64;
#elif defined(_M_PPC)
	cpuArch = PROCESSOR_ARCHITECTURE_PPC;
#elif defined(_M_ALPHA)
	cpuArch = PROCESSOR_ARCHITECTURE_ALPHA;
#elif defined(_M_E2K)
	cpuArch = PROCESSOR_ARCHITECTURE_E2K;
#endif
	return cpuArch;
}

static DWORD GetNumberOfProcessors(void)
{
	DWORD numCPUs = 1;
#if defined(ANDROID)
	return android_getCpuCount();
#elif defined(__MACOSX__) || defined(__IOS__)
	int32_t val = 0;
	size_t len = sizeof(val);
	const int rc = sysctlbyname("hw.logicalcpu", &val, &len, nullptr, 0);
	if (rc == 0)
		numCPUs = WINPR_ASSERTING_INT_CAST(DWORD, val);
#elif defined(__hpux)
	numCPUs = (DWORD)mpctl(MPC_GETNUMSPUS, nullptr, nullptr);
#elif defined(__sgi)
	numCPUs = (DWORD)sysconf(_SC_NPROC_ONLN);
#elif defined(_SC_NPROCESSORS_ONLN)
	numCPUs = (DWORD)sysconf(_SC_NPROCESSORS_ONLN);
#endif
	return numCPUs;
}

static DWORD GetSystemPageSize(void)
{
	DWORD dwPageSize = 0;
	long sc_page_size = -1;
#if defined(_SC_PAGESIZE)

	if (sc_page_size < 0)
		sc_page_size = sysconf(_SC_PAGESIZE);

#endif
#if defined(_SC_PAGE_SIZE)

	if (sc_page_size < 0)
		sc_page_size = sysconf(_SC_PAGE_SIZE);

#endif

	if (sc_page_size > 0)
		dwPageSize = (DWORD)sc_page_size;

	if (dwPageSize < 4096)
		dwPageSize = 4096;

	return dwPageSize;
}

void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
	const SYSTEM_INFO empty = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(lpSystemInfo);

	*lpSystemInfo = empty;
	lpSystemInfo->DUMMYUNIONNAME.DUMMYSTRUCTNAME.wProcessorArchitecture =
	    GetProcessorArchitecture();
	lpSystemInfo->dwPageSize = GetSystemPageSize();
	lpSystemInfo->dwNumberOfProcessors = GetNumberOfProcessors();
}

void GetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
	GetSystemInfo(lpSystemInfo);
}

void GetSystemTime(LPSYSTEMTIME lpSystemTime)
{
	time_t ct = 0;
	struct tm tres;
	struct tm* stm = nullptr;
	WORD wMilliseconds = 0;
	UINT64 now = winpr_GetUnixTimeNS();
	ct = WINPR_TIME_NS_TO_S(now);
	wMilliseconds = (WORD)(WINPR_TIME_NS_REM_MS(now));
	stm = gmtime_r(&ct, &tres);
	ZeroMemory(lpSystemTime, sizeof(SYSTEMTIME));

	if (stm)
	{
		lpSystemTime->wYear = (WORD)(stm->tm_year + 1900);
		lpSystemTime->wMonth = (WORD)(stm->tm_mon + 1);
		lpSystemTime->wDayOfWeek = (WORD)stm->tm_wday;
		lpSystemTime->wDay = (WORD)stm->tm_mday;
		lpSystemTime->wHour = (WORD)stm->tm_hour;
		lpSystemTime->wMinute = (WORD)stm->tm_min;
		lpSystemTime->wSecond = (WORD)stm->tm_sec;
		lpSystemTime->wMilliseconds = wMilliseconds;
	}
}

BOOL SetSystemTime(WINPR_ATTR_UNUSED CONST SYSTEMTIME* lpSystemTime)
{
	/* TODO: Implement */
	WLog_ERR("TODO", "TODO: Implement");
	return FALSE;
}

VOID GetLocalTime(LPSYSTEMTIME lpSystemTime)
{
	time_t ct = 0;
	struct tm tres;
	struct tm* ltm = nullptr;
	WORD wMilliseconds = 0;
	UINT64 now = winpr_GetUnixTimeNS();
	ct = WINPR_TIME_NS_TO_S(now);
	wMilliseconds = (WORD)(WINPR_TIME_NS_REM_MS(now));
	ltm = localtime_r(&ct, &tres);
	ZeroMemory(lpSystemTime, sizeof(SYSTEMTIME));

	if (ltm)
	{
		lpSystemTime->wYear = (WORD)(ltm->tm_year + 1900);
		lpSystemTime->wMonth = (WORD)(ltm->tm_mon + 1);
		lpSystemTime->wDayOfWeek = (WORD)ltm->tm_wday;
		lpSystemTime->wDay = (WORD)ltm->tm_mday;
		lpSystemTime->wHour = (WORD)ltm->tm_hour;
		lpSystemTime->wMinute = (WORD)ltm->tm_min;
		lpSystemTime->wSecond = (WORD)ltm->tm_sec;
		lpSystemTime->wMilliseconds = wMilliseconds;
	}
}

BOOL SetLocalTime(WINPR_ATTR_UNUSED CONST SYSTEMTIME* lpSystemTime)
{
	/* TODO: Implement */
	WLog_ERR("TODO", "TODO: Implement");
	return FALSE;
}

VOID GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
	union
	{
		UINT64 u64;
		FILETIME ft;
	} t;

	t.u64 = (winpr_GetUnixTimeNS() / 100ull) + FILETIME_TO_UNIX_OFFSET_S * 10000000ull;
	*lpSystemTimeAsFileTime = t.ft;
}

BOOL GetSystemTimeAdjustment(WINPR_ATTR_UNUSED PDWORD lpTimeAdjustment,
                             WINPR_ATTR_UNUSED PDWORD lpTimeIncrement,
                             WINPR_ATTR_UNUSED PBOOL lpTimeAdjustmentDisabled)
{
	/* TODO: Implement */
	WLog_ERR("TODO", "TODO: Implement");
	return FALSE;
}

#ifdef CLOCK_MONOTONIC_RAW
#define CLOCK_ID CLOCK_MONOTONIC_RAW
#else
#define CLOCK_ID CLOCK_MONOTONIC
#endif

DWORD GetTickCount(void)
{
	return (DWORD)GetTickCount64();
}
#endif // _WIN32

#if !defined(_WIN32) || defined(_UWP)

#if defined(WITH_WINPR_DEPRECATED)
/* OSVERSIONINFOEX Structure:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms724833
 */

BOOL GetVersionExA(LPOSVERSIONINFOA lpVersionInformation)
{
#ifdef _UWP

	/* Windows 10 Version Info */
	if ((lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOA)) ||
	    (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXA)))
	{
		lpVersionInformation->dwMajorVersion = 10;
		lpVersionInformation->dwMinorVersion = 0;
		lpVersionInformation->dwBuildNumber = 0;
		lpVersionInformation->dwPlatformId = VER_PLATFORM_WIN32_NT;
		ZeroMemory(lpVersionInformation->szCSDVersion, sizeof(lpVersionInformation->szCSDVersion));

		if (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXA))
		{
			LPOSVERSIONINFOEXA lpVersionInformationEx = (LPOSVERSIONINFOEXA)lpVersionInformation;
			lpVersionInformationEx->wServicePackMajor = 0;
			lpVersionInformationEx->wServicePackMinor = 0;
			lpVersionInformationEx->wSuiteMask = 0;
			lpVersionInformationEx->wProductType = VER_NT_WORKSTATION;
			lpVersionInformationEx->wReserved = 0;
		}

		return TRUE;
	}

#else

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
			LPOSVERSIONINFOEXA lpVersionInformationEx = (LPOSVERSIONINFOEXA)lpVersionInformation;
			lpVersionInformationEx->wServicePackMajor = 1;
			lpVersionInformationEx->wServicePackMinor = 0;
			lpVersionInformationEx->wSuiteMask = 0;
			lpVersionInformationEx->wProductType = VER_NT_WORKSTATION;
			lpVersionInformationEx->wReserved = 0;
		}

		return TRUE;
	}

#endif
	return FALSE;
}

BOOL GetVersionExW(LPOSVERSIONINFOW lpVersionInformation)
{
	ZeroMemory(lpVersionInformation->szCSDVersion, sizeof(lpVersionInformation->szCSDVersion));
	return GetVersionExA((LPOSVERSIONINFOA)lpVersionInformation);
}

#endif

#endif

#if !defined(_WIN32) || defined(_UWP)

BOOL GetComputerNameW(LPWSTR lpBuffer, LPDWORD lpnSize)
{
	BOOL rc = 0;
	LPSTR buffer = nullptr;
	if (!lpnSize || (*lpnSize > INT_MAX))
		return FALSE;

	if (*lpnSize > 0)
	{
		buffer = malloc(*lpnSize);
		if (!buffer)
			return FALSE;
	}
	rc = GetComputerNameA(buffer, lpnSize);

	if (rc && (*lpnSize > 0))
	{
		const SSIZE_T res = ConvertUtf8NToWChar(buffer, *lpnSize, lpBuffer, *lpnSize);
		rc = res > 0;
	}

	free(buffer);

	return rc;
}

BOOL GetComputerNameA(LPSTR lpBuffer, LPDWORD lpnSize)
{
	if (!lpnSize)
	{
		SetLastError(ERROR_BAD_ARGUMENTS);
		return FALSE;
	}

	char hostname[256 + 1] = WINPR_C_ARRAY_INIT;
	if (gethostname(hostname, ARRAYSIZE(hostname) - 1) == -1)
		return FALSE;

	size_t length = strnlen(hostname, MAX_COMPUTERNAME_LENGTH);
	const char* dot = strchr(hostname, '.');
	if (dot)
	{
		const size_t dotlen = WINPR_ASSERTING_INT_CAST(size_t, (dot - hostname));
		if (dotlen < length)
			length = dotlen;
	}

	if ((*lpnSize <= (DWORD)length) || !lpBuffer)
	{
		SetLastError(ERROR_BUFFER_OVERFLOW);
		*lpnSize = (DWORD)(length + 1);
		return FALSE;
	}

	strncpy(lpBuffer, hostname, length);
	lpBuffer[length] = '\0';
	*lpnSize = (DWORD)length;
	return TRUE;
}

BOOL GetComputerNameExA(COMPUTER_NAME_FORMAT NameType, LPSTR lpBuffer, LPDWORD lpnSize)
{
	size_t length = 0;
	char hostname[256] = WINPR_C_ARRAY_INIT;

	if (!lpnSize)
	{
		SetLastError(ERROR_BAD_ARGUMENTS);
		return FALSE;
	}

	if ((NameType == ComputerNameNetBIOS) || (NameType == ComputerNamePhysicalNetBIOS))
	{
		BOOL rc = GetComputerNameA(lpBuffer, lpnSize);

		if (!rc)
		{
			if (GetLastError() == ERROR_BUFFER_OVERFLOW)
				SetLastError(ERROR_MORE_DATA);
		}

		return rc;
	}

	if (gethostname(hostname, sizeof(hostname)) == -1)
		return FALSE;

	length = strnlen(hostname, sizeof(hostname));

	switch (NameType)
	{
		case ComputerNameDnsHostname:
		case ComputerNameDnsDomain:
		case ComputerNameDnsFullyQualified:
		case ComputerNamePhysicalDnsHostname:
		case ComputerNamePhysicalDnsDomain:
		case ComputerNamePhysicalDnsFullyQualified:
			if ((*lpnSize <= (DWORD)length) || !lpBuffer)
			{
				*lpnSize = (DWORD)(length + 1);
				SetLastError(ERROR_MORE_DATA);
				return FALSE;
			}

			CopyMemory(lpBuffer, hostname, length);
			lpBuffer[length] = '\0';
			*lpnSize = (DWORD)length;
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD lpnSize)
{
	BOOL rc = 0;
	LPSTR lpABuffer = nullptr;

	if (!lpnSize)
	{
		SetLastError(ERROR_BAD_ARGUMENTS);
		return FALSE;
	}

	if (*lpnSize > 0)
	{
		lpABuffer = calloc(*lpnSize, sizeof(CHAR));

		if (!lpABuffer)
			return FALSE;
	}

	rc = GetComputerNameExA(NameType, lpABuffer, lpnSize);

	if (rc && (*lpnSize > 0))
	{
		const SSIZE_T res = ConvertUtf8NToWChar(lpABuffer, *lpnSize, lpBuffer, *lpnSize);
		rc = res > 0;
	}

	free(lpABuffer);
	return rc;
}

#if defined(__linux__)

static BOOL sysinfo_read_sysfs_string(const char* path, char* buffer, size_t size)
{
	FILE* fp = fopen(path, "r");
	if (!fp)
		return FALSE;

	BOOL rc = FALSE;
	if (fgets(buffer, WINPR_ASSERTING_INT_CAST(int, size), fp))
	{
		size_t len = strlen(buffer);
		while ((len > 0) && ((buffer[len - 1] == '\n') || (buffer[len - 1] == '\r')))
			buffer[--len] = '\0';
		rc = TRUE;
	}
	else
	{
		WLog_ERR(TAG, "error reading string in %s", path);
	}
	(void)fclose(fp);
	return rc;
}

static BOOL sysinfo_read_sysfs_int(const char* path, long long* value)
{
	char buffer[64] = { 0 };
	if (!sysinfo_read_sysfs_string(path, buffer, sizeof(buffer)))
		return FALSE;

	char* end = nullptr;
	const long long v = strtoll(buffer, &end, 10);
	if (end == buffer)
		return FALSE;

	*value = v;
	return TRUE;
}

/** @brief reads /sys/class/power_supply/, the same kernel interface on both desktop Linux and
 *  Android (no ANDROID-specific carve-out needed: Android exposes the identical sysfs nodes). */
static BOOL sysinfo_get_power_status_platform(LPSYSTEM_POWER_STATUS status)
{
	DIR* dir = opendir("/sys/class/power_supply");
	if (!dir)
	{
		/* no power_supply class at all: treat as a desktop with no battery, AC power assumed -
		 * matches real Windows' behavior on a machine with no battery present */
		status->ACLineStatus = AC_LINE_ONLINE;
		status->BatteryFlag = BATTERY_FLAG_NO_BATTERY;
		status->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;
		return TRUE;
	}

	BOOL haveBattery = FALSE;
	BOOL onAC = FALSE;
	BOOL charging = FALSE;
	long long percent = -1;
	long long energyNow = -1;
	long long energyFull = -1;
	long long timeToEmpty = -1;

	struct dirent* entry = nullptr;
	// NOLINTNEXTLINE(concurrency-mt-unsafe): dir is a local DIR*, not shared across threads
	while ((entry = readdir(dir)) != nullptr)
	{
		char path[PATH_MAX] = { 0 };
		char value[64] = { 0 };
		long long v = 0;

		if (entry->d_name[0] == '.')
			continue;

		(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/type", entry->d_name);
		if (!sysinfo_read_sysfs_string(path, value, sizeof(value)))
			continue;

		if (strcmp(value, "Battery") == 0)
		{
			(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/present",
			               entry->d_name);
			if (sysinfo_read_sysfs_int(path, &v) && (v == 0))
				continue; /* battery slot present in sysfs, no battery physically installed */

			haveBattery = TRUE;

			(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/status",
			               entry->d_name);
			if (sysinfo_read_sysfs_string(path, value, sizeof(value)) &&
			    (strcmp(value, "Charging") == 0))
				charging = TRUE;

			(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/capacity",
			               entry->d_name);
			if (sysinfo_read_sysfs_int(path, &v))
				percent = v;

			(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/time_to_empty_now",
			               entry->d_name);
			if (sysinfo_read_sysfs_int(path, &v) && (v > 0))
				timeToEmpty = v;

			/* not every driver exposes "capacity" directly; fall back to computing it from
			 * energy_now/energy_full when present */
			if (percent < 0)
			{
				(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/energy_now",
				               entry->d_name);
				(void)sysinfo_read_sysfs_int(path, &energyNow);
				(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/energy_full",
				               entry->d_name);
				(void)sysinfo_read_sysfs_int(path, &energyFull);

				if ((energyNow < 0) || (energyFull <= 0))
				{
					(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/charge_now",
					               entry->d_name);
					(void)sysinfo_read_sysfs_int(path, &energyNow);
					(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/charge_full",
					               entry->d_name);
					(void)sysinfo_read_sysfs_int(path, &energyFull);
				}

				if ((energyNow >= 0) && (energyFull > 0))
					percent = (energyNow * 100) / energyFull;
			}
		}
		else
		{
			/* Mains / USB / Wireless: any of these reporting online means we're on AC power */
			(void)snprintf(path, sizeof(path) - 1, "/sys/class/power_supply/%s/online",
			               entry->d_name);
			if (sysinfo_read_sysfs_int(path, &v) && (v != 0))
				onAC = TRUE;
		}
	}
	(void)closedir(dir);

	if (!haveBattery)
	{
		status->ACLineStatus = AC_LINE_ONLINE;
		status->BatteryFlag = BATTERY_FLAG_NO_BATTERY;
		status->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;
		return TRUE;
	}

	status->ACLineStatus = (onAC || charging) ? AC_LINE_ONLINE : AC_LINE_OFFLINE;

	BYTE flag = 0;
	if (charging)
		flag |= BATTERY_FLAG_CHARGING;

	if (percent >= 0)
	{
		if (percent > 100)
			percent = 100;
		status->BatteryLifePercent = (BYTE)percent;

		if (percent <= 5)
			flag |= BATTERY_FLAG_CRITICAL;
		else if (percent <= 33)
			flag |= BATTERY_FLAG_LOW;
		else if (percent > 66)
			flag |= BATTERY_FLAG_HIGH;
	}
	else
		status->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;

	status->BatteryFlag = flag ? flag : BATTERY_FLAG_UNKNOWN;

	if (timeToEmpty > 0)
		status->BatteryLifeTime = (DWORD)timeToEmpty;

	return TRUE;
}

#elif defined(__MACOSX__)

static BOOL sysinfo_get_power_status_platform(LPSYSTEM_POWER_STATUS status)
{
	BOOL rc = TRUE;
	CFTypeRef blob = IOPSCopyPowerSourcesInfo();
	if (!blob)
	{
		WLog_ERR(TAG, "unable to retrieve IOPSCopyPowerSourcesInfo()");
		status->ACLineStatus = AC_LINE_ONLINE;
		status->BatteryFlag = BATTERY_FLAG_NO_BATTERY;
		status->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;
		return TRUE;
	}

	BOOL haveBattery = FALSE;
	BOOL onAC = FALSE;
	BOOL charging = FALSE;
	int percent = -1;
	int timeToEmptySeconds = -1;

	CFArrayRef sources = IOPSCopyPowerSourcesList(blob);
	if (sources)
	{
		const CFIndex count = CFArrayGetCount(sources);
		for (CFIndex i = 0; i < count; i++)
		{
			CFDictionaryRef desc =
			    IOPSGetPowerSourceDescription(blob, CFArrayGetValueAtIndex(sources, i));
			if (!desc)
				continue;

			CFBooleanRef isPresent = CFDictionaryGetValue(desc, CFSTR(kIOPSIsPresentKey));
			if (isPresent && (CFBooleanGetValue(isPresent) == false))
				continue;

			CFStringRef stateStr = CFDictionaryGetValue(desc, CFSTR(kIOPSPowerSourceStateKey));
			if (stateStr &&
			    (CFStringCompare(stateStr, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo))
				onAC = TRUE;

			CFNumberRef curCapacity = CFDictionaryGetValue(desc, CFSTR(kIOPSCurrentCapacityKey));
			CFNumberRef maxCapacity = CFDictionaryGetValue(desc, CFSTR(kIOPSMaxCapacityKey));
			if (curCapacity && maxCapacity)
			{
				int cur = 0;
				int max = 0;
				CFNumberGetValue(curCapacity, kCFNumberIntType, &cur);
				CFNumberGetValue(maxCapacity, kCFNumberIntType, &max);
				if (max > 0)
				{
					haveBattery = TRUE;
					percent = (int)(((long long)cur * 100) / max);
				}
			}

			CFBooleanRef isCharging = CFDictionaryGetValue(desc, CFSTR(kIOPSIsChargingKey));
			if (isCharging && CFBooleanGetValue(isCharging))
				charging = TRUE;

			CFNumberRef timeToEmpty = CFDictionaryGetValue(desc, CFSTR(kIOPSTimeToEmptyKey));
			if (timeToEmpty)
			{
				int minutes = -1;
				CFNumberGetValue(timeToEmpty, kCFNumberIntType, &minutes);
				if (minutes > 0)
					timeToEmptySeconds = minutes * 60;
			}
		}
		CFRelease(sources);
	}
	CFRelease(blob);

	if (!haveBattery)
	{
		status->ACLineStatus = AC_LINE_ONLINE;
		status->BatteryFlag = BATTERY_FLAG_NO_BATTERY;
		status->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;
		return rc;
	}

	status->ACLineStatus = onAC ? AC_LINE_ONLINE : AC_LINE_OFFLINE;

	BYTE flag = 0;
	if (charging)
		flag |= BATTERY_FLAG_CHARGING;

	if (percent >= 0)
	{
		if (percent > 100)
			percent = 100;
		status->BatteryLifePercent = (BYTE)percent;

		if (percent <= 5)
			flag |= BATTERY_FLAG_CRITICAL;
		else if (percent <= 33)
			flag |= BATTERY_FLAG_LOW;
		else if (percent > 66)
			flag |= BATTERY_FLAG_HIGH;
	}
	else
		status->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;

	status->BatteryFlag = flag ? flag : BATTERY_FLAG_UNKNOWN;

	if (timeToEmptySeconds > 0)
		status->BatteryLifeTime = (DWORD)timeToEmptySeconds;

	return rc;
}

#elif defined(__FreeBSD__)

static BOOL sysinfo_sysctl_int(const char* name, int* value)
{
	size_t len = sizeof(*value);
	return sysctlbyname(name, value, &len, nullptr, 0) == 0;
}

static BOOL sysinfo_get_power_status_platform(LPSYSTEM_POWER_STATUS status)
{
	int units = 0;
	if (!sysinfo_sysctl_int("hw.acpi.battery.units", &units) || (units <= 0))
	{
		status->ACLineStatus = AC_LINE_ONLINE;
		status->BatteryFlag = BATTERY_FLAG_NO_BATTERY;
		status->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;
		return TRUE;
	}

	int acline = -1;
	(void)sysinfo_sysctl_int("hw.acpi.acline", &acline);
	status->ACLineStatus =
	    (acline == 1) ? AC_LINE_ONLINE : ((acline == 0) ? AC_LINE_OFFLINE : AC_LINE_UNKNOWN);

	int life = -1;
	(void)sysinfo_sysctl_int("hw.acpi.battery.life", &life);

	int remaining = -1;
	(void)sysinfo_sysctl_int("hw.acpi.battery.time", &remaining);

	/* hw.acpi.battery.state bits mirror the ACPI _BST battery-state field:
	 * 1=discharging, 2=charging, 4=critical */
	int state = 0;
	(void)sysinfo_sysctl_int("hw.acpi.battery.state", &state);

	BYTE flag = 0;
	if (state & 0x2)
		flag |= BATTERY_FLAG_CHARGING;
	if (state & 0x4)
		flag |= BATTERY_FLAG_CRITICAL;

	if ((life >= 0) && (life <= 100))
	{
		status->BatteryLifePercent = (BYTE)life;
		if (!(flag & BATTERY_FLAG_CRITICAL))
		{
			if (life <= 5)
				flag |= BATTERY_FLAG_CRITICAL;
			else if (life <= 33)
				flag |= BATTERY_FLAG_LOW;
			else if (life > 66)
				flag |= BATTERY_FLAG_HIGH;
		}
	}
	else
		status->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;

	status->BatteryFlag = flag ? flag : BATTERY_FLAG_UNKNOWN;

	if (remaining > 0)
		status->BatteryLifeTime = (DWORD)remaining * 60; /* minutes -> seconds */

	return TRUE;
}

#endif

BOOL GetSystemPowerStatus(LPSYSTEM_POWER_STATUS lpSystemPowerStatus)
{
	if (!lpSystemPowerStatus)
	{
		SetLastError(ERROR_BAD_ARGUMENTS);
		return FALSE;
	}

	/* safe, "no battery information available" defaults - overwritten below on platforms with a
	 * real backend */
	lpSystemPowerStatus->ACLineStatus = AC_LINE_UNKNOWN;
	lpSystemPowerStatus->BatteryFlag = BATTERY_FLAG_UNKNOWN;
	lpSystemPowerStatus->BatteryLifePercent = BATTERY_PERCENTAGE_UNKNOWN;
	lpSystemPowerStatus->SystemStatusFlag = 0;
	lpSystemPowerStatus->BatteryLifeTime = BATTERY_LIFE_UNKNOWN;
	lpSystemPowerStatus->BatteryFullLifeTime = BATTERY_LIFE_UNKNOWN;

#if defined(__linux__) || defined(__MACOSX__) || defined(__FreeBSD__)
	return sysinfo_get_power_status_platform(lpSystemPowerStatus);
#else
	WLog_WARN(TAG, "GetSystemPowerStatus is not implemented on this platform, reporting "
	               "\"no battery, unknown state\"");
	return TRUE;
#endif
}

#endif

#if defined(_UWP)

DWORD GetTickCount(void)
{
	return (DWORD)GetTickCount64();
}

#endif

#if (!defined(_WIN32)) || (defined(_WIN32) && (_WIN32_WINNT < 0x0600))

ULONGLONG winpr_GetTickCount64(void)
{
	const UINT64 ns = winpr_GetTickCount64NS();
	return WINPR_TIME_NS_TO_MS(ns);
}

#endif

UINT64 winpr_GetTickCount64NS(void)
{
	UINT64 ticks = 0;
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 199309L)
	struct timespec ts = WINPR_C_ARRAY_INIT;

	if (clock_gettime(CLOCK_ID, &ts) == 0)
		ticks = (WINPR_ASSERTING_INT_CAST(uint64_t, ts.tv_sec) * 1000000000ull) +
		        WINPR_ASSERTING_INT_CAST(uint64_t, ts.tv_nsec);
#elif defined(__MACH__) && defined(__APPLE__)
	ticks = mac_get_time_ns();
#elif defined(_WIN32)
	LARGE_INTEGER li = WINPR_C_ARRAY_INIT;
	LARGE_INTEGER freq = WINPR_C_ARRAY_INIT;
	if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&li))
		ticks = li.QuadPart * 1000000000ull / freq.QuadPart;
#else
	struct timeval tv = WINPR_C_ARRAY_INIT;

	if (gettimeofday(&tv, nullptr) == 0)
		ticks = (tv.tv_sec * 1000000000ull) + (tv.tv_usec * 1000ull);

	/* We need to trick here:
	 * this function should return the system uptime, but we need higher resolution.
	 * so on first call get the actual timestamp along with the system uptime.
	 *
	 * return the uptime measured from now on (e.g. current measure - first measure + uptime at
	 * first measure)
	 */
	static UINT64 first = 0;
	static UINT64 uptime = 0;
	if (first == 0)
	{
		struct sysinfo info = WINPR_C_ARRAY_INIT;
		if (sysinfo(&info) == 0)
		{
			first = ticks;
			uptime = 1000000000ull * info.uptime;
		}
	}

	ticks = ticks - first + uptime;
#endif
	return ticks;
}

UINT64 winpr_GetUnixTimeNS(void)
{
#if defined(_WIN32)

	union
	{
		UINT64 u64;
		FILETIME ft;
	} t = WINPR_C_ARRAY_INIT;
	GetSystemTimeAsFileTime(&t.ft);
	return (t.u64 - FILETIME_TO_UNIX_OFFSET_S * 10000000ull) * 100ull;
#elif defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 199309L)
	struct timespec ts = WINPR_C_ARRAY_INIT;
	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return 0;
	return WINPR_ASSERTING_INT_CAST(uint64_t, ts.tv_sec) * 1000000000ull +
	       WINPR_ASSERTING_INT_CAST(uint64_t, ts.tv_nsec);
#else
	struct timeval tv = WINPR_C_ARRAY_INIT;
	if (gettimeofday(&tv, nullptr) != 0)
		return 0;
	return tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000ull;
#endif
}

/* If x86 */
#ifdef _M_IX86_AMD64

#if defined(__GNUC__)
#define xgetbv(_func_, _lo_, _hi_) \
	__asm__ __volatile__("xgetbv" : "=a"(_lo_), "=d"(_hi_) : "c"(_func_))
#elif defined(_MSC_VER)
#define xgetbv(_func_, _lo_, _hi_)              \
	{                                           \
		unsigned __int64 val = _xgetbv(_func_); \
		_lo_ = val & 0xFFFFFFFF;                \
		_hi_ = (val >> 32);                     \
	}
#endif

#define B_BIT_AVX2 (1 << 5)
#define B_BIT_AVX512F (1 << 16)
#define D_BIT_MMX (1 << 23)
#define D_BIT_SSE (1 << 25)
#define D_BIT_SSE2 (1 << 26)
#define D_BIT_3DN (1 << 30)
// #define C_BIT_SSE3 (1 << 0)
#define C_BIT_PCLMULQDQ (1 << 1)
#define C81_BIT_LZCNT (1 << 5)
// #define C_BIT_3DNP (1 << 8)
#define C_BIT_3DNP (1 << 8)
#define C_BIT_SSSE3 (1 << 9)
#define C_BIT_SSE41 (1 << 19)
#define C_BIT_SSE42 (1 << 20)
#define C_BIT_FMA (1 << 12)
#define C_BIT_AES (1 << 25)
#define C_BIT_XGETBV (1 << 27)
#define C_BIT_AVX (1 << 28)
#define E_BIT_XMM (1 << 1)
#define E_BIT_YMM (1 << 2)
#define E_BITS_AVX (E_BIT_XMM | E_BIT_YMM)

static void cpuid(unsigned info, unsigned* eax, unsigned* ebx, unsigned* ecx, unsigned* edx)
{
#ifdef __GNUC__
	*eax = *ebx = *ecx = *edx = 0;
	__asm volatile(
	/* The EBX (or RBX register on x86_64) is used for the PIC base address
	 * and must not be corrupted by our inline assembly.
	 */
#ifdef _M_IX86
	    "mov %%ebx, %%esi;"
	    "cpuid;"
	    "xchg %%ebx, %%esi;"
#else
	    "mov %%rbx, %%rsi;"
	    "cpuid;"
	    "xchg %%rbx, %%rsi;"
#endif
	    : "=a"(*eax), "=S"(*ebx), "=c"(*ecx), "=d"(*edx)
	    : "a"(info), "c"(0));
#elif defined(_MSC_VER)
	int a[4];
	__cpuid(a, info);
	*eax = a[0];
	*ebx = a[1];
	*ecx = a[2];
	*edx = a[3];
#endif
}
#elif defined(_M_ARM) || defined(_M_ARM64)
#if defined(__linux__)
// HWCAP flags from linux kernel - uapi/asm/hwcap.h
#define HWCAP_SWP (1 << 0)
#define HWCAP_HALF (1 << 1)
#define HWCAP_THUMB (1 << 2)
#define HWCAP_26BIT (1 << 3) /* Play it safe */
#define HWCAP_FAST_MULT (1 << 4)
#define HWCAP_FPA (1 << 5)
#define HWCAP_VFP (1 << 6)
#define HWCAP_EDSP (1 << 7)
#define HWCAP_JAVA (1 << 8)
#define HWCAP_IWMMXT (1 << 9)
#define HWCAP_CRUNCH (1 << 10)
#define HWCAP_THUMBEE (1 << 11)
#define HWCAP_NEON (1 << 12)
#define HWCAP_VFPv3 (1 << 13)
#define HWCAP_VFPv3D16 (1 << 14) /* also set for VFPv4-D16 */
#define HWCAP_TLS (1 << 15)
#define HWCAP_VFPv4 (1 << 16)
#define HWCAP_IDIVA (1 << 17)
#define HWCAP_IDIVT (1 << 18)
#define HWCAP_VFPD32 (1 << 19) /* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV (HWCAP_IDIVA | HWCAP_IDIVT)

// From linux kernel uapi/linux/auxvec.h
#define AT_HWCAP 16

static unsigned GetARMCPUCaps(void)
{
	unsigned caps = 0;
	int fd = open("/proc/self/auxv", O_RDONLY);

	if (fd == -1)
		return 0;

	static struct
	{
		unsigned a_type; /* Entry type */
		unsigned a_val;  /* Integer value */
	} auxvec;

	while (1)
	{
		int num;
		num = read(fd, (char*)&auxvec, sizeof(auxvec));

		if (num < 1 || (auxvec.a_type == 0 && auxvec.a_val == 0))
			break;

		if (auxvec.a_type == AT_HWCAP)
		{
			caps = auxvec.a_val;
		}
	}

	close(fd);
	return caps;
}

#endif // defined(__linux__)
#endif // _M_IX86_AMD64

#ifndef _WIN32

#if defined(_M_ARM) || defined(_M_ARM64)
#ifdef __linux__
#include <sys/auxv.h>
#endif
#endif

BOOL IsProcessorFeaturePresent(DWORD ProcessorFeature)
{
	BOOL ret = FALSE;
#if defined(ANDROID)
	const uint64_t features = android_getCpuFeatures();
	const AndroidCpuFamily family = android_getCpuFamily();
	const BOOL isArm = (family == ANDROID_CPU_FAMILY_ARM) || (family == ANDROID_CPU_FAMILY_ARM64);
	const BOOL isX86 = (family == ANDROID_CPU_FAMILY_X86) || (family == ANDROID_CPU_FAMILY_X86_64);

	if (isX86)
	{
		switch (ProcessorFeature)
		{
			case PF_MMX_INSTRUCTIONS_AVAILABLE:
			case PF_XMMI_INSTRUCTIONS_AVAILABLE:
			case PF_XMMI64_INSTRUCTIONS_AVAILABLE:
			case PF_3DNOW_INSTRUCTIONS_AVAILABLE:
			case PF_SSE3_INSTRUCTIONS_AVAILABLE:
				return TRUE;
			case PF_SSSE3_INSTRUCTIONS_AVAILABLE:
				return features & ANDROID_CPU_X86_FEATURE_SSSE3;
			case PF_SSE4_1_INSTRUCTIONS_AVAILABLE:
				return features & ANDROID_CPU_X86_FEATURE_SSE4_1;
			case PF_SSE4_2_INSTRUCTIONS_AVAILABLE:
				return features & ANDROID_CPU_X86_FEATURE_SSE4_2;
			case PF_AVX_INSTRUCTIONS_AVAILABLE:
				return features & ANDROID_CPU_X86_FEATURE_AVX;
			case PF_AVX2_INSTRUCTIONS_AVAILABLE:
				return features & ANDROID_CPU_X86_FEATURE_AVX2;
			case PF_AVX512F_INSTRUCTIONS_AVAILABLE:
			default:
				WLog_WARN(TAG, "feature 0x%08" PRIx32 " check not implemented", ProcessorFeature);
				return FALSE;
		}
	}

	if (isArm)
	{
		switch (ProcessorFeature)
		{
			case PF_ARM_NEON_INSTRUCTIONS_AVAILABLE:
			case PF_ARM_NEON:
				return features & ANDROID_CPU_ARM_FEATURE_NEON;

			default:
				WLog_WARN(TAG, "feature 0x%08" PRIx32 " check not implemented", ProcessorFeature);
				return FALSE;
		}
	}

	WLog_WARN(TAG, "Unsupported Android CPU family 0x%08" PRIx32, family);
	return FALSE;

#elif defined(_M_ARM) || defined(_M_ARM64)
#ifdef __linux__
	const unsigned long caps = getauxval(AT_HWCAP);

	switch (ProcessorFeature)
	{
		case PF_ARM_NEON_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_NEON:

			if (caps & HWCAP_NEON)
				ret = TRUE;

			break;

		case PF_ARM_THUMB:
			if (caps & HWCAP_THUMB)
				ret = TRUE;

		case PF_ARM_VFP_32_REGISTERS_AVAILABLE:
			if (caps & HWCAP_VFPD32)
				ret = TRUE;

		case PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE:
			if ((caps & HWCAP_IDIVA) || (caps & HWCAP_IDIVT))
				ret = TRUE;

		case PF_ARM_VFP3:
			if (caps & HWCAP_VFPv3)
				ret = TRUE;

			break;

		case PF_ARM_JAZELLE:
			if (caps & HWCAP_JAVA)
				ret = TRUE;

			break;

		case PF_ARM_DSP:
			if (caps & HWCAP_EDSP)
				ret = TRUE;

			break;

		case PF_ARM_MPU:
			if (caps & HWCAP_EDSP)
				ret = TRUE;

			break;

		case PF_ARM_THUMB2:
			if ((caps & HWCAP_IDIVT) || (caps & HWCAP_VFPv4))
				ret = TRUE;

			break;

		case PF_ARM_T2EE:
			if (caps & HWCAP_THUMBEE)
				ret = TRUE;

			break;

		case PF_ARM_INTEL_WMMX:
			if (caps & HWCAP_IWMMXT)
				ret = TRUE;

			break;
		case PF_MMX_INSTRUCTIONS_AVAILABLE:
		case PF_XMMI_INSTRUCTIONS_AVAILABLE:
		case PF_XMMI64_INSTRUCTIONS_AVAILABLE:
		case PF_3DNOW_INSTRUCTIONS_AVAILABLE:
		case PF_SSE3_INSTRUCTIONS_AVAILABLE:
		case PF_SSSE3_INSTRUCTIONS_AVAILABLE:
		case PF_SSE4_1_INSTRUCTIONS_AVAILABLE:
		case PF_SSE4_2_INSTRUCTIONS_AVAILABLE:
		case PF_AVX_INSTRUCTIONS_AVAILABLE:
		case PF_AVX2_INSTRUCTIONS_AVAILABLE:
		case PF_AVX512F_INSTRUCTIONS_AVAILABLE:
			ret = FALSE;
			break;
		case PF_ARM_V8_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V83_LRCPC_INSTRUCTIONS_AVAILABLE:
		default:
			WLog_WARN(TAG, "feature 0x%08" PRIx32 " check not implemented", ProcessorFeature);
			break;
	}

#else // __linux__

	switch (ProcessorFeature)
	{
		case PF_MMX_INSTRUCTIONS_AVAILABLE:
		case PF_XMMI_INSTRUCTIONS_AVAILABLE:
		case PF_XMMI64_INSTRUCTIONS_AVAILABLE:
		case PF_3DNOW_INSTRUCTIONS_AVAILABLE:
		case PF_SSE3_INSTRUCTIONS_AVAILABLE:
		case PF_SSSE3_INSTRUCTIONS_AVAILABLE:
		case PF_SSE4_1_INSTRUCTIONS_AVAILABLE:
		case PF_SSE4_2_INSTRUCTIONS_AVAILABLE:
		case PF_AVX_INSTRUCTIONS_AVAILABLE:
		case PF_AVX2_INSTRUCTIONS_AVAILABLE:
		case PF_AVX512F_INSTRUCTIONS_AVAILABLE:
			ret = FALSE;
			break;
		case PF_ARM_NEON_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_NEON:
#ifdef __ARM_NEON
			ret = TRUE;
#endif
			break;
		case PF_ARM_V8_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_V83_LRCPC_INSTRUCTIONS_AVAILABLE:
		default:
			WLog_WARN(TAG, "feature 0x%08" PRIx32 " check not implemented", ProcessorFeature);
			break;
	}

#endif // __linux__
#endif

#if defined(_M_IX86_AMD64)
#ifdef __GNUC__
	unsigned a = 0;
	unsigned b = 0;
	unsigned c = 0;
	unsigned d = 0;
	cpuid(1, &a, &b, &c, &d);

	switch (ProcessorFeature)
	{
		case PF_MMX_INSTRUCTIONS_AVAILABLE:
			if (d & D_BIT_MMX)
				ret = TRUE;

			break;

		case PF_XMMI_INSTRUCTIONS_AVAILABLE:
			if (d & D_BIT_SSE)
				ret = TRUE;

			break;

		case PF_XMMI64_INSTRUCTIONS_AVAILABLE:
			if (d & D_BIT_SSE2)
				ret = TRUE;

			break;

		case PF_3DNOW_INSTRUCTIONS_AVAILABLE:
			if (d & D_BIT_3DN)
				ret = TRUE;

			break;

		case PF_SSE3_INSTRUCTIONS_AVAILABLE:
			ret = __builtin_cpu_supports("sse3");
			break;

		case PF_SSSE3_INSTRUCTIONS_AVAILABLE:
			ret = __builtin_cpu_supports("ssse3");
			break;
		case PF_SSE4_1_INSTRUCTIONS_AVAILABLE:
			ret = __builtin_cpu_supports("sse4.1");
			break;
		case PF_SSE4_2_INSTRUCTIONS_AVAILABLE:
			ret = __builtin_cpu_supports("sse4.2");
			break;
		case PF_AVX_INSTRUCTIONS_AVAILABLE:
			ret = __builtin_cpu_supports("avx");
			break;
		case PF_AVX2_INSTRUCTIONS_AVAILABLE:
			ret = __builtin_cpu_supports("avx2");
			break;
		case PF_AVX512F_INSTRUCTIONS_AVAILABLE:
			ret = __builtin_cpu_supports("avx512f");
			break;
		case PF_ARM_NEON_INSTRUCTIONS_AVAILABLE:
#if defined(__ARM_NEON__)
			ret = TRUE;
#endif
			break;
		default:
			WLog_WARN(TAG, "feature 0x%08" PRIx32 " check not implemented", ProcessorFeature);
			break;
	}

#endif // __GNUC__
#endif

#if defined(_M_E2K)
	/* compiler flags on e2k arch determine CPU features */
	switch (ProcessorFeature)
	{
		case PF_MMX_INSTRUCTIONS_AVAILABLE:
#ifdef __MMX__
			ret = TRUE;
#endif
			break;

		case PF_3DNOW_INSTRUCTIONS_AVAILABLE:
#ifdef __3dNOW__
			ret = TRUE;
#endif
			break;

		case PF_SSE3_INSTRUCTIONS_AVAILABLE:
#ifdef __SSE3__
			ret = TRUE;
#endif
			break;

		default:
			break;
	}

#endif
	return ret;
}

#endif //_WIN32

DWORD GetTickCountPrecise(void)
{
#ifdef _WIN32
	LARGE_INTEGER freq = WINPR_C_ARRAY_INIT;
	LARGE_INTEGER current = WINPR_C_ARRAY_INIT;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&current);
	return (DWORD)(current.QuadPart * 1000LL / freq.QuadPart);
#else
	return GetTickCount();
#endif
}

BOOL IsProcessorFeaturePresentEx(DWORD ProcessorFeature)
{
	BOOL ret = FALSE;
#if defined(_M_ARM) || defined(_M_ARM64)
#ifdef __linux__
	unsigned caps;
	caps = GetARMCPUCaps();

	switch (ProcessorFeature)
	{
		case PF_EX_ARM_VFP1:
			if (caps & HWCAP_VFP)
				ret = TRUE;

			break;

		case PF_EX_ARM_VFP3D16:
			if (caps & HWCAP_VFPv3D16)
				ret = TRUE;

			break;

		case PF_EX_ARM_VFP4:
			if (caps & HWCAP_VFPv4)
				ret = TRUE;

			break;

		case PF_EX_ARM_IDIVA:
			if (caps & HWCAP_IDIVA)
				ret = TRUE;

			break;

		case PF_EX_ARM_IDIVT:
			if (caps & HWCAP_IDIVT)
				ret = TRUE;

			break;
	}

#endif // __linux__
#elif defined(_M_IX86_AMD64)
	unsigned a = 0;
	unsigned b = 0;
	unsigned c = 0;
	unsigned d = 0;
	cpuid(1, &a, &b, &c, &d);

	switch (ProcessorFeature)
	{
		case PF_EX_LZCNT:
		{
			unsigned a81 = 0;
			unsigned b81 = 0;
			unsigned c81 = 0;
			unsigned d81 = 0;
			cpuid(0x80000001, &a81, &b81, &c81, &d81);

			if (c81 & C81_BIT_LZCNT)
				ret = TRUE;
		}
		break;

		case PF_EX_3DNOW_PREFETCH:
			if (c & C_BIT_3DNP)
				ret = TRUE;

			break;

		case PF_EX_SSSE3:
			if (c & C_BIT_SSSE3)
				ret = TRUE;

			break;

		case PF_EX_SSE41:
			if (c & C_BIT_SSE41)
				ret = TRUE;

			break;

		case PF_EX_SSE42:
			if (c & C_BIT_SSE42)
				ret = TRUE;

			break;
#if defined(__GNUC__) || defined(_MSC_VER)

		case PF_EX_AVX:
		case PF_EX_AVX2:
		case PF_EX_AVX512F:
		case PF_EX_FMA:
		case PF_EX_AVX_AES:
		case PF_EX_AVX_PCLMULQDQ:
		{
			/* Check for general AVX support */
			if (!(c & C_BIT_AVX))
				break;

			/* Check for xgetbv support */
			if (!(c & C_BIT_XGETBV))
				break;

			int e = 0;
			int f = 0;
			xgetbv(0, e, f);

			/* XGETBV enabled for applications and XMM/YMM states enabled */
			if ((e & E_BITS_AVX) == E_BITS_AVX)
			{
				switch (ProcessorFeature)
				{
					case PF_EX_AVX:
						ret = TRUE;
						break;

					case PF_EX_AVX2:
					case PF_EX_AVX512F:
						cpuid(7, &a, &b, &c, &d);
						switch (ProcessorFeature)
						{
							case PF_EX_AVX2:
								if (b & B_BIT_AVX2)
									ret = TRUE;
								break;

							case PF_EX_AVX512F:
								if (b & B_BIT_AVX512F)
									ret = TRUE;
								break;

							default:
								break;
						}
						break;

					case PF_EX_FMA:
						if (c & C_BIT_FMA)
							ret = TRUE;

						break;

					case PF_EX_AVX_AES:
						if (c & C_BIT_AES)
							ret = TRUE;

						break;

					case PF_EX_AVX_PCLMULQDQ:
						if (c & C_BIT_PCLMULQDQ)
							ret = TRUE;

						break;
					default:
						break;
				}
			}
		}
		break;
#endif // __GNUC__ || _MSC_VER

		default:
			break;
	}
#elif defined(_M_E2K)
	/* compiler flags on e2k arch determine CPU features */
	switch (ProcessorFeature)
	{
		case PF_EX_LZCNT:
#ifdef __LZCNT__
			ret = TRUE;
#endif
			break;

		case PF_EX_SSSE3:
#ifdef __SSSE3__
			ret = TRUE;
#endif
			break;

		case PF_EX_SSE41:
#ifdef __SSE4_1__
			ret = TRUE;
#endif
			break;

		case PF_EX_SSE42:
#ifdef __SSE4_2__
			ret = TRUE;
#endif
			break;

		case PF_EX_AVX:
#ifdef __AVX__
			ret = TRUE;
#endif
			break;

		case PF_EX_AVX2:
#ifdef __AVX2__
			ret = TRUE;
#endif
			break;

		case PF_EX_FMA:
#ifdef __FMA__
			ret = TRUE;
#endif
			break;

		default:
			break;
	}
#endif
	return ret;
}
