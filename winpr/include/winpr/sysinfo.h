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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32

#define PROCESSOR_ARCHITECTURE_INTEL			0
#define PROCESSOR_ARCHITECTURE_MIPS			1
#define PROCESSOR_ARCHITECTURE_ALPHA			2
#define PROCESSOR_ARCHITECTURE_PPC			3
#define PROCESSOR_ARCHITECTURE_SHX			4
#define PROCESSOR_ARCHITECTURE_ARM			5
#define PROCESSOR_ARCHITECTURE_IA64			6
#define PROCESSOR_ARCHITECTURE_ALPHA64			7
#define PROCESSOR_ARCHITECTURE_MSIL			8
#define PROCESSOR_ARCHITECTURE_AMD64			9
#define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64		10
#define PROCESSOR_ARCHITECTURE_NEUTRAL			11
#define PROCESSOR_ARCHITECTURE_UNKNOWN			0xFFFF

#define PROCESSOR_INTEL_386				386
#define PROCESSOR_INTEL_486				486
#define PROCESSOR_INTEL_PENTIUM				586
#define PROCESSOR_INTEL_IA64				2200
#define PROCESSOR_AMD_X8664				8664
#define PROCESSOR_MIPS_R4000				4000
#define PROCESSOR_ALPHA_21064				21064
#define PROCESSOR_PPC_601				601
#define PROCESSOR_PPC_603				603
#define PROCESSOR_PPC_604				604
#define PROCESSOR_PPC_620				620
#define PROCESSOR_HITACHI_SH3				10003
#define PROCESSOR_HITACHI_SH3E				10004
#define PROCESSOR_HITACHI_SH4				10005
#define PROCESSOR_MOTOROLA_821				821
#define PROCESSOR_SHx_SH3				103
#define PROCESSOR_SHx_SH4				104
#define PROCESSOR_STRONGARM				2577
#define PROCESSOR_ARM720				1824
#define PROCESSOR_ARM820				2080
#define PROCESSOR_ARM920				2336
#define PROCESSOR_ARM_7TDMI				70001
#define PROCESSOR_OPTIL					0x494F

typedef struct _SYSTEM_INFO
{
	union
	{
		DWORD dwOemId;

		struct
		{
			WORD wProcessorArchitecture;
			WORD wReserved;
		};
	};

	DWORD dwPageSize;
	LPVOID lpMinimumApplicationAddress;
	LPVOID lpMaximumApplicationAddress;
	DWORD_PTR dwActiveProcessorMask;
	DWORD dwNumberOfProcessors;
	DWORD dwProcessorType;
	DWORD dwAllocationGranularity;
	WORD wProcessorLevel;
	WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

WINPR_API void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
WINPR_API void GetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo);

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

WINPR_API void GetSystemTime(LPSYSTEMTIME lpSystemTime);
WINPR_API BOOL SetSystemTime(CONST SYSTEMTIME* lpSystemTime);
WINPR_API VOID GetLocalTime(LPSYSTEMTIME lpSystemTime);
WINPR_API BOOL SetLocalTime(CONST SYSTEMTIME* lpSystemTime);

WINPR_API VOID GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);
WINPR_API BOOL GetSystemTimeAdjustment(PDWORD lpTimeAdjustment, PDWORD lpTimeIncrement, PBOOL lpTimeAdjustmentDisabled);

WINPR_API BOOL IsProcessorFeaturePresent(DWORD ProcessorFeature);

#define PF_FLOATING_POINT_PRECISION_ERRATA		0
#define PF_FLOATING_POINT_EMULATED			1
#define PF_COMPARE_EXCHANGE_DOUBLE			2
#define PF_MMX_INSTRUCTIONS_AVAILABLE   3
#define PF_PPC_MOVEMEM_64BIT_OK				4
#define PF_XMMI_INSTRUCTIONS_AVAILABLE			6 /* SSE */
#define PF_3DNOW_INSTRUCTIONS_AVAILABLE			7
#define PF_RDTSC_INSTRUCTION_AVAILABLE			8
#define PF_PAE_ENABLED					9
#define PF_XMMI64_INSTRUCTIONS_AVAILABLE		10 /* SSE2 */
#define PF_SSE_DAZ_MODE_AVAILABLE			11
#define PF_NX_ENABLED					12
#define PF_SSE3_INSTRUCTIONS_AVAILABLE			13
#define PF_COMPARE_EXCHANGE128				14
#define PF_COMPARE64_EXCHANGE128			15
#define PF_CHANNELS_ENABLED				16
#define PF_XSAVE_ENABLED				17
#define PF_ARM_VFP_32_REGISTERS_AVAILABLE		18
#define PF_ARM_NEON_INSTRUCTIONS_AVAILABLE		19
#define PF_SECOND_LEVEL_ADDRESS_TRANSLATION		20
#define PF_VIRT_FIRMWARE_ENABLED			21
#define PF_RDWRFSGSBASE_AVAILABLE			22
#define PF_FASTFAIL_AVAILABLE				23
#define PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE		24
#define PF_ARM_64BIT_LOADSTORE_ATOMIC			25
#define PF_ARM_EXTERNAL_CACHE_AVAILABLE			26
#define PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE		27

#define PF_ARM_V4                       0x80000001
#define PF_ARM_V5                       0x80000002
#define PF_ARM_V6                       0x80000003
#define PF_ARM_V7                       0x80000004
#define PF_ARM_THUMB                    0x80000005
#define PF_ARM_JAZELLE                  0x80000006
#define PF_ARM_DSP                      0x80000007
#define PF_ARM_MOVE_CP                  0x80000008
#define PF_ARM_VFP10                    0x80000009
#define PF_ARM_MPU                      0x8000000A
#define PF_ARM_WRITE_BUFFER             0x8000000B
#define PF_ARM_MBX                      0x8000000C
#define PF_ARM_L2CACHE                  0x8000000D
#define PF_ARM_PHYSICALLY_TAGGED_CACHE  0x8000000E
#define PF_ARM_VFP_SINGLE_PRECISION     0x8000000F
#define PF_ARM_VFP_DOUBLE_PRECISION     0x80000010
#define PF_ARM_ITCM                     0x80000011
#define PF_ARM_DTCM                     0x80000012
#define PF_ARM_UNIFIED_CACHE            0x80000013
#define PF_ARM_WRITE_BACK_CACHE         0x80000014
#define PF_ARM_CACHE_CAN_BE_LOCKED_DOWN 0x80000015
#define PF_ARM_L2CACHE_MEMORY_MAPPED    0x80000016
#define PF_ARM_L2CACHE_COPROC           0x80000017
#define PF_ARM_THUMB2                   0x80000018
#define PF_ARM_T2EE                     0x80000019
#define PF_ARM_VFP3                     0x8000001A
#define PF_ARM_NEON                     0x8000001B
#define PF_ARM_UNALIGNED_ACCESS         0x8000001C

#define PF_ARM_INTEL_XSCALE             0x80010001
#define PF_ARM_INTEL_PMU                0x80010002
#define PF_ARM_INTEL_WMMX               0x80010003

#endif

#if !defined(_WIN32) || defined(_UWP)

WINPR_API BOOL GetVersionExA(LPOSVERSIONINFOA lpVersionInformation);
WINPR_API BOOL GetVersionExW(LPOSVERSIONINFOW lpVersionInformation);

#ifdef UNICODE
#define GetVersionEx	GetVersionExW
#else
#define GetVersionEx	GetVersionExA
#endif

#endif

#if !defined(_WIN32) || defined(_UWP)

WINPR_API DWORD GetTickCount(void);

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

#define MAX_COMPUTERNAME_LENGTH 31

WINPR_API BOOL GetComputerNameExA(COMPUTER_NAME_FORMAT NameType, LPSTR lpBuffer, LPDWORD lpnSize);
WINPR_API BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD lpnSize);

#ifdef UNICODE
#define GetComputerNameEx	GetComputerNameExW
#else
#define GetComputerNameEx	GetComputerNameExA
#endif

#endif

#if (!defined(_WIN32)) || (defined(_WIN32) && (_WIN32_WINNT < 0x0600))

WINPR_API ULONGLONG winpr_GetTickCount64(void);
#define GetTickCount64 winpr_GetTickCount64

#endif

WINPR_API DWORD GetTickCountPrecise(void);

WINPR_API BOOL IsProcessorFeaturePresentEx(DWORD ProcessorFeature);

/* extended flags */
#define PF_EX_LZCNT			1
#define PF_EX_3DNOW_PREFETCH		2
#define PF_EX_SSSE3			3
#define PF_EX_SSE41			4
#define PF_EX_SSE42			5
#define PF_EX_AVX			6
#define PF_EX_FMA			7
#define PF_EX_AVX_AES			8
#define PF_EX_AVX2			9
#define PF_EX_ARM_VFP1			10
#define PF_EX_ARM_VFP3D16		11
#define PF_EX_ARM_VFP4			12
#define PF_EX_ARM_IDIVA			13
#define PF_EX_ARM_IDIVT			14
#define PF_EX_AVX_PCLMULQDQ		15

/*
 * some "aliases" for the standard defines
 * to be more clear
 */
#define PF_SSE_INSTRUCTIONS_AVAILABLE	PF_XMMI_INSTRUCTIONS_AVAILABLE
#define PF_SSE2_INSTRUCTIONS_AVAILABLE	PF_XMMI64_INSTRUCTIONS_AVAILABLE

#ifdef __cplusplus
}
#endif

#endif /* WINPR_SYSINFO_H */
