/**
 * WinPR: Windows Portable Runtime
 * Windows Data Types
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#ifndef WINPR_WTYPES_H
#define WINPR_WTYPES_H

#include <assert.h>

#include <winpr/config.h>
#include <winpr/platform.h>

/* MSDN: Windows Data Types - http://msdn.microsoft.com/en-us/library/aa383751/ */
/* [MS-DTYP]: Windows Data Types - http://msdn.microsoft.com/en-us/library/cc230273/ */

#include <wchar.h>
#include <winpr/windows.h>

#include <winpr/spec.h>

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <limits.h>

#if defined(_WIN32) || defined(__MINGW32__)
#include <wtypes.h>

/* Handle missing ssize_t on Windows */
#if defined(WINPR_HAVE_SSIZE_T)
typedef ssize_t SSIZE_T;
#elif !defined(WINPR_HAVE_WIN_SSIZE_T)
typedef intptr_t SSIZE_T;
#endif

#endif

#if defined(__OBJC__) && defined(__APPLE__)
#include <objc/objc.h>
#endif

#ifndef CONST
#define CONST const
#endif

#ifndef VOID
#define VOID void
#endif

WINPR_PRAGMA_DIAG_PUSH WINPR_PRAGMA_DIAG_IGNORED_RESERVED_ID_MACRO
    WINPR_PRAGMA_DIAG_IGNORED_RESERVED_IDENTIFIER

#if !defined(_WIN32) && !defined(__MINGW32__)

#define CALLBACK

#define WINAPI
#define CDECL

#ifndef FAR
#define FAR
#endif

#ifndef NEAR
#define NEAR
#endif

    typedef void *PVOID,
    *LPVOID, *PVOID64, *LPVOID64;

#ifndef XMD_H    /* X11/Xmd.h typedef collision with BOOL */
#ifndef __OBJC__ /* objc.h typedef collision with BOOL */
#ifndef __APPLE__
typedef int32_t BOOL;

#else /* __APPLE__ */
#include <TargetConditionals.h>

/* ensure compatibility with objc libraries */
#if (defined(TARGET_OS_IPHONE) && (TARGET_OS_IPHONE != 0) && defined(__LP64__)) || \
    (defined(TARGET_OS_WATCH) && (TARGET_OS_WATCH != 0))
typedef bool BOOL;
#else
typedef signed char BOOL;
#endif
#endif /* __APPLE__ */
#endif /* __OBJC__ */
#endif /* XMD_H */

typedef BOOL *PBOOL, *LPBOOL;

#ifndef FALSE
#define FALSE false
#endif

#ifndef TRUE
#define TRUE true
#endif

#ifndef XMD_H /* X11/Xmd.h typedef collision with BYTE */
typedef uint8_t BYTE;

#endif /* XMD_H */
typedef BYTE byte, *PBYTE, *LPBYTE;
typedef BYTE BOOLEAN, PBOOLEAN;

#if CHAR_BIT == 8
typedef char CHAR;
typedef unsigned char UCHAR;
#else
typedef int8_t CHAR;

typedef uint8_t UCHAR;

#endif
typedef CHAR CCHAR, *PCHAR, *LPCH, *PCH, *PSTR, *LPSTR;
typedef const CHAR *LPCCH, *PCCH, *LPCSTR, *PCSTR;
typedef UCHAR* PUCHAR;

typedef uint16_t WCHAR;

typedef WCHAR UNICODE, *PWCHAR, *LPWCH, *PWCH, *BSTR, *LMSTR, *LPWSTR, *PWSTR;
typedef const WCHAR *LPCWCH, *PCWCH, *LMCSTR, *LPCWSTR, *PCWSTR;

typedef int16_t SHORT, *PSHORT;

typedef int32_t INT, *PINT, *LPINT;

typedef int32_t LONG, *PLONG, *LPLONG;

typedef int64_t LONGLONG, *PLONGLONG;

typedef uint32_t UINT, *PUINT, *LPUINT;

typedef uint16_t USHORT, *PUSHORT;

typedef uint32_t ULONG, *PULONG;

typedef uint64_t ULONGLONG, *PULONGLONG;

#ifndef XMD_H /* X11/Xmd.h typedef collisions */
typedef int8_t INT8;

typedef int16_t INT16;

typedef int32_t INT32;

typedef int64_t INT64;

#endif
typedef INT8* PINT8;
typedef INT16* PINT16;
typedef INT32* PINT32;
typedef INT64* PINT64;

typedef int32_t LONG32, *PLONG32;

#ifndef LONG64 /* X11/Xmd.h uses/defines LONG64 */
typedef int64_t LONG64, *PLONG64;

#endif

typedef uint8_t UINT8, *PUINT8;

typedef uint16_t UINT16, *PUINT16;

typedef uint32_t UINT32, *PUINT32;

typedef uint64_t UINT64, *PUINT64;

typedef uint64_t ULONG64, *PULONG64;

typedef uint16_t WORD, *PWORD, *LPWORD;

typedef uint32_t DWORD, DWORD32, *PDWORD, *LPDWORD, *PDWORD32;

typedef uint64_t DWORD64, DWORDLONG, QWORD, *PDWORD64, *PDWORDLONG, *PQWORD;

typedef intptr_t INT_PTR, *PINT_PTR;

typedef uintptr_t UINT_PTR, *PUINT_PTR;

typedef intptr_t LONG_PTR, *PLONG_PTR;

typedef uintptr_t ULONG_PTR, *PULONG_PTR;

typedef uintptr_t DWORD_PTR, *PDWORD_PTR;

typedef ULONG_PTR SIZE_T, *PSIZE_T; /** deprecated */
#if defined(WINPR_HAVE_SSIZE_T)
#include <sys/types.h>
typedef ssize_t SSIZE_T;
#elif !defined(WINPR_HAVE_WIN_SSIZE_T)
typedef LONG_PTR SSIZE_T;
#endif

typedef float FLOAT;

typedef double DOUBLE;

typedef void* HANDLE;
typedef HANDLE *PHANDLE, *LPHANDLE;
typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef HANDLE HWND;
typedef HANDLE HBITMAP;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HBRUSH;
typedef HANDLE HMENU;

typedef DWORD HCALL;

typedef ULONG error_status_t;
typedef LONG HRESULT;
typedef LONG SCODE;
typedef SCODE* PSCODE;

typedef struct s_POINTL /* ptl  */
{
	LONG x;
	LONG y;
} POINTL, *PPOINTL;

typedef struct tagSIZE
{
	LONG cx;
	LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef SIZE SIZEL;

typedef struct s_GUID
{
	UINT32 Data1;
	UINT16 Data2;
	UINT16 Data3;
	BYTE Data4[8];
} GUID, UUID, *PGUID, *LPGUID, *LPCGUID;
typedef GUID CLSID;

typedef struct s_LUID
{
	DWORD LowPart;
	LONG HighPart;
} LUID, *PLUID;

typedef GUID IID;
typedef IID* REFIID;

#ifdef UNICODE
#define _T(x) u##x // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

#else
#define _T(x) x // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

#endif

#ifdef UNICODE
typedef LPWSTR PTSTR;
typedef LPWSTR LPTCH;
typedef LPWSTR LPTSTR;
typedef LPCWSTR LPCTSTR;
#else
typedef LPSTR PTSTR;
typedef LPSTR LPTCH;
typedef LPSTR LPTSTR;
typedef LPCSTR LPCTSTR;
#endif

typedef union u_ULARGE_INTEGER
{
	struct
	{
		DWORD LowPart;
		DWORD HighPart;
	} DUMMYSTRUCTNAME;

	struct
	{
		DWORD LowPart;
		DWORD HighPart;
	} u;

	ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef union u_LARGE_INTEGER
{
	struct
	{
		DWORD LowPart;
		LONG HighPart;
	} DUMMYSTRUCTNAME;

	struct
	{
		DWORD LowPart;
		LONG HighPart;
	} u;

	LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct s_FILETIME
{
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct s_SYSTEMTIME
{
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct s_RPC_SID_IDENTIFIER_AUTHORITY
{
	BYTE Value[6];
} RPC_SID_IDENTIFIER_AUTHORITY;

typedef DWORD SECURITY_INFORMATION, *PSECURITY_INFORMATION;

typedef struct s_RPC_SID
{
	UCHAR Revision;
	UCHAR SubAuthorityCount;
	RPC_SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
	ULONG SubAuthority[1];
} RPC_SID, *PRPC_SID, *PSID;

typedef struct s_ACL
{
	UCHAR AclRevision;
	UCHAR Sbz1;
	USHORT AclSize;
	USHORT AceCount;
	USHORT Sbz2;
} ACL, *PACL;

typedef struct s_SECURITY_DESCRIPTOR
{
	UCHAR Revision;
	UCHAR Sbz1;
	USHORT Control;
	PSID Owner;
	PSID Group;
	PACL Sacl;
	PACL Dacl;
} SECURITY_DESCRIPTOR, *PSECURITY_DESCRIPTOR;

typedef WORD SECURITY_DESCRIPTOR_CONTROL, *PSECURITY_DESCRIPTOR_CONTROL;

typedef struct s_SECURITY_ATTRIBUTES
{
	DWORD nLength;
	LPVOID lpSecurityDescriptor;
	BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct s_PROCESS_INFORMATION
{
	HANDLE hProcess;
	HANDLE hThread;
	DWORD dwProcessId;
	DWORD dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef DWORD (*PTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

typedef void* FARPROC;

typedef struct tagDEC
{
	USHORT wReserved;
	union
	{
		struct
		{
			BYTE scale;
			BYTE sign;
		} DUMMYSTRUCTNAME;
		USHORT signscale;
	} DUMMYUNIONNAME;
	ULONG Hi32;
	union
	{
		struct
		{
			ULONG Lo32;
			ULONG Mid32;
		} DUMMYSTRUCTNAME2;
		ULONGLONG Lo64;
	} DUMMYUNIONNAME2;
} DECIMAL;

typedef DECIMAL* LPDECIMAL;

#define DECIMAL_NEG ((BYTE)0x80)
#define DECIMAL_SETZERO(dec) \
	{                        \
		(dec).Lo64 = 0;      \
		(dec).Hi32 = 0;      \
		(dec).signscale = 0; \
	}

typedef DWORD LCID;
typedef PDWORD PLCID;
typedef WORD LANGID;

#endif /* _WIN32 not defined */

typedef void* PCONTEXT_HANDLE;
typedef PCONTEXT_HANDLE* PPCONTEXT_HANDLE;

#ifndef _NTDEF
typedef LONG NTSTATUS;
typedef NTSTATUS* PNTSTATUS;
#endif

#ifndef _LPCVOID_DEFINED // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

#define _LPCVOID_DEFINED // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

typedef const VOID* LPCVOID;
#endif

#ifndef _LPCBYTE_DEFINED // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

#define _LPCBYTE_DEFINED // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

typedef const BYTE* LPCBYTE;
#endif

#ifndef SSIZE_MAX
#if defined(_POSIX_SSIZE_MAX)
#define SSIZE_MAX _POSIX_SSIZE_MAX
#elif defined(_WIN64)
#define SSIZE_MAX _I64_MAX
#elif defined(_WIN32)
#define SSIZE_MAX LONG_MAX
#else
#define SSIZE_MAX INTPTR_MAX
#endif
#endif

#define PRIdz "zd"
#define PRIiz "zi"
#define PRIuz "zu"
#define PRIoz "zo"
#define PRIxz "zx"
#define PRIXz "zX"

#include <winpr/user.h>

#ifndef _WIN32
#include <stdio.h>

// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
static inline int _fseeki64(FILE* fp, INT64 offset, int origin)
{
	return fseeko(fp, offset, origin);
}

// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
static inline INT64 _ftelli64(FILE* fp)
{
	return ftello(fp);
}
#endif

WINPR_PRAGMA_DIAG_POP

#endif /* WINPR_WTYPES_H */
