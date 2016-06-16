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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include <winpr/platform.h>

#if defined(__linux__) && defined(__GNUC__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "../log.h"
#define TAG WINPR_TAG("sysinfo")

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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>
#include <winpr/platform.h>

#if defined(__MACOSX__) || defined(__IOS__) || \
defined(__FreeBSD__) || defined(__NetBSD__) || \
defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/sysctl.h>
#endif

static DWORD GetProcessorArchitecture()
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

static DWORD GetNumberOfProcessors()
{
	DWORD numCPUs = 1;
	/* TODO: iOS */
#if defined(__linux__) || defined(__sun) || defined(_AIX)
	numCPUs = (DWORD) sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__MACOSX__) || \
	defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__)
	{
		int mib[4];
		size_t length = sizeof(numCPUs);
		mib[0] = CTL_HW;
#if defined(__FreeBSD__) || defined(__OpenBSD__)
		mib[1] = HW_NCPU;
#else
		mib[1] = HW_AVAILCPU;
#endif
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

static DWORD GetSystemPageSize()
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
		dwPageSize = (DWORD) sc_page_size;

	if (dwPageSize < 4096)
		dwPageSize = 4096;

	return dwPageSize;
}

void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
	lpSystemInfo->wProcessorArchitecture = GetProcessorArchitecture();
	lpSystemInfo->wReserved = 0;
	lpSystemInfo->dwPageSize = GetSystemPageSize();
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

void GetSystemTime(LPSYSTEMTIME lpSystemTime)
{
	time_t ct = 0;
	struct tm* stm = NULL;
	WORD wMilliseconds = 0;
	ct = time(NULL);
	wMilliseconds = (WORD)(GetTickCount() % 1000);
	stm = gmtime(&ct);
	ZeroMemory(lpSystemTime, sizeof(SYSTEMTIME));

	if (stm)
	{
		lpSystemTime->wYear = (WORD)(stm->tm_year + 1900);
		lpSystemTime->wMonth = (WORD)(stm->tm_mon + 1);
		lpSystemTime->wDayOfWeek = (WORD) stm->tm_wday;
		lpSystemTime->wDay = (WORD) stm->tm_mday;
		lpSystemTime->wHour = (WORD) stm->tm_hour;
		lpSystemTime->wMinute = (WORD) stm->tm_min;
		lpSystemTime->wSecond = (WORD) stm->tm_sec;
		lpSystemTime->wMilliseconds = wMilliseconds;
	}
}

BOOL SetSystemTime(CONST SYSTEMTIME* lpSystemTime)
{
	return FALSE;
}

VOID GetLocalTime(LPSYSTEMTIME lpSystemTime)
{
	time_t ct = 0;
	struct tm* ltm = NULL;
	WORD wMilliseconds = 0;
	ct = time(NULL);
	wMilliseconds = (WORD)(GetTickCount() % 1000);
	ltm = localtime(&ct);
	ZeroMemory(lpSystemTime, sizeof(SYSTEMTIME));

	if (ltm)
	{
		lpSystemTime->wYear = (WORD)(ltm->tm_year + 1900);
		lpSystemTime->wMonth = (WORD)(ltm->tm_mon + 1);
		lpSystemTime->wDayOfWeek = (WORD) ltm->tm_wday;
		lpSystemTime->wDay = (WORD) ltm->tm_mday;
		lpSystemTime->wHour = (WORD) ltm->tm_hour;
		lpSystemTime->wMinute = (WORD) ltm->tm_min;
		lpSystemTime->wSecond = (WORD) ltm->tm_sec;
		lpSystemTime->wMilliseconds = wMilliseconds;
	}
}

BOOL SetLocalTime(CONST SYSTEMTIME* lpSystemTime)
{
	return FALSE;
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

BOOL GetSystemTimeAdjustment(PDWORD lpTimeAdjustment, PDWORD lpTimeIncrement, PBOOL lpTimeAdjustmentDisabled)
{
	return FALSE;
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
#endif // _WIN32

#if !defined(_WIN32) || defined(_UWP)

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
	return GetVersionExA((LPOSVERSIONINFOA) lpVersionInformation);
}

#endif

#if !defined(_WIN32) || defined(_UWP)

BOOL GetComputerNameA(LPSTR lpBuffer, LPDWORD lpnSize)
{
	char* dot;
	int length;
	char hostname[256];

	if (gethostname(hostname, sizeof(hostname)) == -1)
		return FALSE;

	length = (int) strlen(hostname);
	dot = strchr(hostname, '.');

	if (dot)
		length = (int) (dot - hostname);

	if (*lpnSize <= (DWORD) length)
	{
		SetLastError(ERROR_BUFFER_OVERFLOW);
		*lpnSize = length + 1;
		return FALSE;
	}

	if (!lpBuffer)
		return FALSE;

	CopyMemory(lpBuffer, hostname, length);
	lpBuffer[length] = '\0';
	*lpnSize = length;

	return TRUE;
}

BOOL GetComputerNameExA(COMPUTER_NAME_FORMAT NameType, LPSTR lpBuffer, LPDWORD lpnSize)
{
	int length;
	char hostname[256];

	if ((NameType == ComputerNameNetBIOS) || (NameType == ComputerNamePhysicalNetBIOS))
		return GetComputerNameA(lpBuffer, lpnSize);

	if (gethostname(hostname, sizeof(hostname)) == -1)
		return FALSE;

	length = (int) strlen(hostname);

	switch (NameType)
	{
		case ComputerNameDnsHostname:
		case ComputerNameDnsDomain:
		case ComputerNameDnsFullyQualified:
		case ComputerNamePhysicalDnsHostname:
		case ComputerNamePhysicalDnsDomain:
		case ComputerNamePhysicalDnsFullyQualified:
			if (*lpnSize <= (DWORD) length)
			{
				*lpnSize = length + 1;
				SetLastError(ERROR_MORE_DATA);
				return FALSE;
			}

			if (!lpBuffer)
				return FALSE;

			CopyMemory(lpBuffer, hostname, length);
			lpBuffer[length] = '\0';
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize)
{
	WLog_ERR(TAG, "GetComputerNameExW unimplemented");
	return FALSE;
}

#endif

#if defined(_UWP)

DWORD GetTickCount(void)
{
	return (DWORD) GetTickCount64();
}

#endif

#if (!defined(_WIN32)) || (defined(_WIN32) && (_WIN32_WINNT < 0x0600))

ULONGLONG winpr_GetTickCount64(void)
{
	ULONGLONG ticks = 0;
#if defined(__linux__)
	struct timespec ts;

	if (!clock_gettime(CLOCK_MONOTONIC_RAW, &ts))
		ticks = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);

#elif defined(_WIN32)
	ticks = (ULONGLONG) GetTickCount();
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

/* If x86 */
#ifdef _M_IX86_AMD64

#if defined(__GNUC__) && defined(__AVX__)
#define xgetbv(_func_, _lo_, _hi_) \
	__asm__ __volatile__ ("xgetbv" : "=a" (_lo_), "=d" (_hi_) : "c" (_func_))
#endif

#define D_BIT_MMX       (1<<23)
#define D_BIT_SSE       (1<<25)
#define D_BIT_SSE2      (1<<26)
#define D_BIT_3DN       (1<<30)
#define C_BIT_SSE3      (1<<0)
#define C_BIT_PCLMULQDQ (1<<1)
#define C81_BIT_LZCNT   (1<<5)
#define C_BIT_3DNP      (1<<8)
#define C_BIT_3DNP      (1<<8)
#define C_BIT_SSSE3     (1<<9)
#define C_BIT_SSE41     (1<<19)
#define C_BIT_SSE42     (1<<20)
#define C_BIT_FMA       (1<<12)
#define C_BIT_AES       (1<<25)
#define C_BIT_XGETBV    (1<<27)
#define C_BIT_AVX       (1<<28)
#define C_BITS_AVX      (C_BIT_XGETBV|C_BIT_AVX)
#define E_BIT_XMM       (1<<1)
#define E_BIT_YMM       (1<<2)
#define E_BITS_AVX      (E_BIT_XMM|E_BIT_YMM)

static void cpuid(
	unsigned info,
	unsigned* eax,
	unsigned* ebx,
	unsigned* ecx,
	unsigned* edx)
{
#ifdef __GNUC__
	*eax = *ebx = *ecx = *edx = 0;
	__asm volatile
	(
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
			: "0"(info)
		);
#elif defined(_MSC_VER)
	int a[4];
	__cpuid(a, info);
	*eax = a[0];
	*ebx = a[1];
	*ecx = a[2];
	*edx = a[3];
#endif
}
#elif defined(_M_ARM)
#if defined(__linux__)
// HWCAP flags from linux kernel - uapi/asm/hwcap.h
#define HWCAP_SWP (1 << 0)
#define HWCAP_HALF  (1 << 1)
#define HWCAP_THUMB (1 << 2)
#define HWCAP_26BIT (1 << 3)  /* Play it safe */
#define HWCAP_FAST_MULT (1 << 4)
#define HWCAP_FPA (1 << 5)
#define HWCAP_VFP (1 << 6)
#define HWCAP_EDSP  (1 << 7)
#define HWCAP_JAVA  (1 << 8)
#define HWCAP_IWMMXT  (1 << 9)
#define HWCAP_CRUNCH  (1 << 10)
#define HWCAP_THUMBEE (1 << 11)
#define HWCAP_NEON  (1 << 12)
#define HWCAP_VFPv3 (1 << 13)
#define HWCAP_VFPv3D16  (1 << 14) /* also set for VFPv4-D16 */
#define HWCAP_TLS (1 << 15)
#define HWCAP_VFPv4 (1 << 16)
#define HWCAP_IDIVA (1 << 17)
#define HWCAP_IDIVT (1 << 18)
#define HWCAP_VFPD32  (1 << 19) /* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV  (HWCAP_IDIVA | HWCAP_IDIVT)

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
		unsigned a_type;    /* Entry type */
		unsigned  a_val;   /* Integer value */
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

BOOL IsProcessorFeaturePresent(DWORD ProcessorFeature)
{
	BOOL ret = FALSE;
#ifdef _M_ARM
#ifdef __linux__
	unsigned caps;
	caps = GetARMCPUCaps();

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

		default:
			break;
	}

#elif defined(__APPLE__) // __linux__

	switch (ProcessorFeature)
	{
		case PF_ARM_NEON_INSTRUCTIONS_AVAILABLE:
		case PF_ARM_NEON:
			ret = TRUE;
			break;
	}

#endif // __linux__
#elif defined(_M_IX86_AMD64)
#ifdef __GNUC__
	unsigned a, b, c, d;
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
			if (c & C_BIT_SSE3)
				ret = TRUE;

			break;

		default:
			break;
	}

#endif // __GNUC__
#endif
	return ret;
}

#endif //_WIN32

DWORD GetTickCountPrecise(void)
{
#ifdef _WIN32
	LARGE_INTEGER freq;
	LARGE_INTEGER current;

	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&current);

	return (DWORD) (current.QuadPart * 1000LL / freq.QuadPart);
#else
	return GetTickCount();
#endif
}

BOOL IsProcessorFeaturePresentEx(DWORD ProcessorFeature)
{
	BOOL ret = FALSE;
#ifdef _M_ARM
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
	unsigned a, b, c, d;
	cpuid(1, &a, &b, &c, &d);

	switch (ProcessorFeature)
	{
		case PF_EX_LZCNT:
			{
				unsigned a81, b81, c81, d81;
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
#if defined(__GNUC__) && defined(__AVX__)

		case PF_EX_AVX:
		case PF_EX_FMA:
		case PF_EX_AVX_AES:
		case PF_EX_AVX_PCLMULQDQ:
			{
				/* Check for general AVX support */
				if ((c & C_BITS_AVX) != C_BITS_AVX)
					break;

				int e, f;
				xgetbv(0, e, f);

				/* XGETBV enabled for applications and XMM/YMM states enabled */
				if ((e & E_BITS_AVX) == E_BITS_AVX)
				{
					switch (ProcessorFeature)
					{
						case PF_EX_AVX:
							ret = TRUE;
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
					}
				}
			}
			break;
#endif //__AVX__

		default:
			break;
	}

#endif
	return ret;
}
