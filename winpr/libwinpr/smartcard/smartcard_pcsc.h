/**
 * WinPR: Windows Portable Runtime
 * Smart Card API
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2020 Armin Novak <armin.novak@thincast.com>
 * Copyright 2020 Thincast Technologies GmbH
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

#ifndef WINPR_SMARTCARD_PCSC_PRIVATE_H
#define WINPR_SMARTCARD_PCSC_PRIVATE_H

#ifndef _WIN32

#include <winpr/platform.h>
#include <winpr/smartcard.h>

/**
 * On Windows, DWORD and ULONG are defined to unsigned long.
 * However, 64-bit Windows uses the LLP64 model which defines
 * unsigned long as a 4-byte type, while most non-Windows
 * systems use the LP64 model where unsigned long is 8 bytes.
 *
 * WinPR correctly defines DWORD and ULONG to be 4-byte types
 * regardless of LLP64/LP64, but this has the side effect of
 * breaking compatibility with the broken pcsc-lite types.
 *
 * To make matters worse, pcsc-lite correctly defines
 * the data types on OS X, but not on other platforms.
 */

#ifdef __APPLE__

#include <stdint.h>

#ifndef BYTE
typedef uint8_t PCSC_BYTE;
#endif
typedef uint8_t PCSC_UCHAR;
typedef PCSC_UCHAR* PCSC_PUCHAR;
typedef uint16_t PCSC_USHORT;

#ifndef __COREFOUNDATION_CFPLUGINCOM__
typedef uint32_t PCSC_ULONG;
typedef void* PCSC_LPVOID;
typedef int16_t PCSC_BOOL;
#endif

typedef PCSC_ULONG* PCSC_PULONG;
typedef const void* PCSC_LPCVOID;
typedef uint32_t PCSC_DWORD;
typedef PCSC_DWORD* PCSC_PDWORD;
typedef uint16_t PCSC_WORD;
typedef int32_t PCSC_LONG;
typedef const char* PCSC_LPCSTR;
typedef const PCSC_BYTE* PCSC_LPCBYTE;
typedef PCSC_BYTE* PCSC_LPBYTE;
typedef PCSC_DWORD* PCSC_LPDWORD;
typedef char* PCSC_LPSTR;

#else

#ifndef BYTE
typedef unsigned char PCSC_BYTE;
#endif
typedef unsigned char PCSC_UCHAR;
typedef PCSC_UCHAR* PCSC_PUCHAR;
typedef unsigned short PCSC_USHORT;

#ifndef __COREFOUNDATION_CFPLUGINCOM__
typedef unsigned long PCSC_ULONG;
typedef void* PCSC_LPVOID;
#endif

typedef const void* PCSC_LPCVOID;
typedef unsigned long PCSC_DWORD;
typedef PCSC_DWORD* PCSC_PDWORD;
typedef long PCSC_LONG;
typedef const char* PCSC_LPCSTR;
typedef const PCSC_BYTE* PCSC_LPCBYTE;
typedef PCSC_BYTE* PCSC_LPBYTE;
typedef PCSC_DWORD* PCSC_LPDWORD;
typedef char* PCSC_LPSTR;

/* these types were deprecated but still used by old drivers and
 * applications. So just declare and use them. */
typedef PCSC_LPSTR PCSC_LPTSTR;
typedef PCSC_LPCSTR PCSC_LPCTSTR;

/* types unused by pcsc-lite */
typedef short PCSC_BOOL;
typedef unsigned short PCSC_WORD;
typedef PCSC_ULONG* PCSC_PULONG;

#endif

#define PCSC_SCARD_UNKNOWN 0x0001
#define PCSC_SCARD_ABSENT 0x0002
#define PCSC_SCARD_PRESENT 0x0004
#define PCSC_SCARD_SWALLOWED 0x0008
#define PCSC_SCARD_POWERED 0x0010
#define PCSC_SCARD_NEGOTIABLE 0x0020
#define PCSC_SCARD_SPECIFIC 0x0040

#define PCSC_SCARD_PROTOCOL_RAW 0x00000004u
#define PCSC_SCARD_PROTOCOL_T15 0x00000008u

#define PCSC_MAX_BUFFER_SIZE 264
#define PCSC_MAX_BUFFER_SIZE_EXTENDED (4 + 3 + (1 << 16) + 3 + 2)

#define PCSC_MAX_ATR_SIZE 33

#define PCSC_SCARD_AUTOALLOCATE (PCSC_DWORD)(-1)

#define PCSC_SCARD_CTL_CODE(code) (0x42000000 + (code))
#define PCSC_CM_IOCTL_GET_FEATURE_REQUEST PCSC_SCARD_CTL_CODE(3400)

/**
 * pcsc-lite defines SCARD_READERSTATE, SCARD_IO_REQUEST as packed
 * on Mac OS X only and uses default packing everywhere else.
 */

#ifdef __APPLE__
#pragma pack(push, 1)
#endif

typedef struct
{
	LPCSTR szReader;
	LPVOID pvUserData;
	PCSC_DWORD dwCurrentState;
	PCSC_DWORD dwEventState;
	PCSC_DWORD cbAtr;
	BYTE rgbAtr[PCSC_MAX_ATR_SIZE]; /* WinSCard: 36, PCSC: 33 */
} PCSC_SCARD_READERSTATE;

typedef struct
{
	PCSC_DWORD dwProtocol;
	PCSC_DWORD cbPciLength;
} PCSC_SCARD_IO_REQUEST;

#ifdef __APPLE__
#pragma pack(pop)
#endif

#pragma pack(push, 1)

typedef struct
{
	BYTE tag;
	BYTE length;
	UINT32 value;
} PCSC_TLV_STRUCTURE;

#pragma pack(pop)

int PCSC_InitializeSCardApi(void);
const SCardApiFunctionTable* PCSC_GetSCardApiFunctionTable(void);

#endif

#endif /* WINPR_SMARTCARD_PCSC_PRIVATE_H */
