/**
 * WinPR: Windows Portable Runtime
 * Windows Data Types
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

#ifndef WINPR_WTYPES_H
#define WINPR_WTYPES_H

/* MSDN: Windows Data Types - http://msdn.microsoft.com/en-us/library/aa383751/ */
/* [MS-DTYP]: Windows Data Types - http://msdn.microsoft.com/en-us/library/cc230273/ */

#include <wchar.h>
#include <winpr/windows.h>

#include <winpr/spec.h>

#ifdef _WIN32
#include <wtypes.h>
#endif

#if defined(__OBJC__) && defined(__APPLE__)
#include <objc/objc.h>
#endif

#ifndef _WIN32

#define WINAPI
#define CDECL

#ifndef FAR
#define FAR
#endif

#ifndef NEAR
#define NEAR
#endif

#define __int8	char
#define __int16 short
#define __int32 int
#define __int64 long long

#if __x86_64__
#define __int3264 __int64
#else
#define __int3264 __int32
#endif

#ifndef __OBJC__
#if defined(__APPLE__)
typedef signed char BOOL;
#else
#ifndef XMD_H
typedef int BOOL;
#endif
#endif
#endif

typedef BOOL *PBOOL, *LPBOOL;

#if defined(__LP64__) || defined(__APPLE__)
typedef int LONG;
typedef unsigned int DWORD;
typedef unsigned int ULONG;
#else
typedef long LONG;
typedef unsigned long DWORD;
typedef unsigned long ULONG;
#endif

typedef unsigned char BYTE, *PBYTE, *LPBYTE;
typedef BYTE BOOLEAN, *PBOOLEAN;
typedef unsigned short WCHAR, *PWCHAR;
typedef WCHAR* BSTR;
typedef char CHAR, *PCHAR;
typedef DWORD *PDWORD, *LPDWORD;
typedef unsigned int DWORD32;
typedef unsigned __int64 DWORD64;
typedef unsigned __int64 ULONGLONG;
typedef ULONGLONG DWORDLONG, *PDWORDLONG;
typedef float FLOAT;
typedef unsigned char UCHAR, *PUCHAR;
typedef short SHORT;

#ifndef FALSE
#define FALSE			0
#endif

#ifndef TRUE
#define TRUE			1
#endif

#define CONST const
#define CALLBACK

typedef void* HANDLE, *PHANDLE, *LPHANDLE;
typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef HANDLE HWND;
typedef HANDLE HBITMAP;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HBRUSH;
typedef HANDLE HMENU;

typedef DWORD HCALL;
typedef int INT, *LPINT;
typedef signed char INT8;
typedef signed short INT16;
#ifndef XMD_H
typedef signed int INT32;
typedef signed __int64 INT64;
#endif
typedef const WCHAR* LMCSTR;
typedef WCHAR* LMSTR;
typedef LONG *PLONG, *LPLONG;
typedef signed __int64 LONGLONG;

typedef __int3264 LONG_PTR, *PLONG_PTR;
typedef unsigned __int3264 ULONG_PTR, *PULONG_PTR;

typedef signed int LONG32;

#ifndef XMD_H
typedef signed __int64 LONG64;
#endif

typedef CHAR *PSTR, *LPSTR, *LPCH;
typedef const CHAR *LPCSTR,*PCSTR;

typedef WCHAR *LPWSTR, *PWSTR, *LPWCH;
typedef const WCHAR *LPCWSTR,*PCWSTR;

typedef unsigned __int64 QWORD;

typedef unsigned int UINT;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned __int64 UINT64;
typedef ULONG *PULONG;

typedef ULONG HRESULT;
typedef ULONG SCODE;

typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;
typedef ULONG_PTR SIZE_T;
typedef unsigned int ULONG32;
typedef unsigned __int64 ULONG64;
typedef wchar_t UNICODE;
typedef unsigned short USHORT;
#define VOID void
typedef void *PVOID, *LPVOID;
typedef void *PVOID64, *LPVOID64;
typedef unsigned short WORD, *PWORD, *LPWORD;

#if __x86_64__
typedef __int64 INT_PTR;
typedef unsigned __int64 UINT_PTR;
#else
typedef int INT_PTR;
typedef unsigned int UINT_PTR;
#endif

typedef struct _GUID
{
	UINT32 Data1;
	UINT16 Data2;
	UINT16 Data3;
	BYTE Data4[8];
} GUID, UUID, *PGUID, *LPGUID, *LPCGUID;

typedef struct _LUID
{
	DWORD LowPart;
	LONG  HighPart;
} LUID, *PLUID;

typedef GUID IID;
typedef IID* REFIID;

#ifdef UNICODE
#define _T(x)	L ## x
#else
#define _T(x)	x
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

typedef union _ULARGE_INTEGER
{
	struct
	{
		DWORD LowPart;
		DWORD HighPart;
	};

	struct
	{
		DWORD LowPart;
		DWORD HighPart;
	} u;

	ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef union _LARGE_INTEGER
{
	struct
	{
		DWORD LowPart;
		LONG  HighPart;
	};

	struct
	{
		DWORD LowPart;
		LONG  HighPart;
	} u;

	LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _FILETIME
{
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME
{
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
} SYSTEMTIME,*PSYSTEMTIME,*LPSYSTEMTIME;

typedef struct _RPC_SID_IDENTIFIER_AUTHORITY
{
	BYTE Value[6];
} RPC_SID_IDENTIFIER_AUTHORITY;

typedef DWORD SECURITY_INFORMATION, *PSECURITY_INFORMATION;

typedef struct _RPC_SID
{
	unsigned char Revision;
	unsigned char SubAuthorityCount;
	RPC_SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
	unsigned long SubAuthority[];
} RPC_SID, *PRPC_SID, *PSID;

typedef struct _ACL
{
	unsigned char AclRevision;
	unsigned char Sbz1;
	unsigned short AclSize;
	unsigned short AceCount;
	unsigned short Sbz2;
} ACL, *PACL;

typedef struct _SECURITY_DESCRIPTOR
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

typedef struct _SECURITY_ATTRIBUTES
{
	DWORD nLength;
	LPVOID lpSecurityDescriptor;
	BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _PROCESS_INFORMATION
{
	HANDLE hProcess;
	HANDLE hThread;
	DWORD dwProcessId;
	DWORD dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef DWORD (*PTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

typedef void* FARPROC;

#endif

typedef BYTE byte;
typedef double DOUBLE;

typedef void* PCONTEXT_HANDLE;
typedef PCONTEXT_HANDLE* PPCONTEXT_HANDLE;

typedef unsigned long error_status_t;

#ifndef _NTDEF_
typedef LONG NTSTATUS;
typedef NTSTATUS *PNTSTATUS;
#endif

#ifndef _LPCBYTE_DEFINED
#define _LPCBYTE_DEFINED
typedef const BYTE *LPCBYTE;
#endif

#ifndef _LPCVOID_DEFINED
#define _LPCVOID_DEFINED
typedef const VOID *LPCVOID;
#endif

#ifndef _WIN32

typedef struct tagDEC
{
	USHORT wReserved;
	union {
		struct {
			BYTE scale;
			BYTE sign;
		} DUMMYSTRUCTNAME;
		USHORT signscale;
	} DUMMYUNIONNAME;
	ULONG Hi32;
	union {
		struct {
			ULONG Lo32;
			ULONG Mid32;
		} DUMMYSTRUCTNAME2;
		ULONGLONG Lo64;
	} DUMMYUNIONNAME2;
} DECIMAL;

typedef DECIMAL *LPDECIMAL;

#define DECIMAL_NEG		((BYTE) 0x80)
#define DECIMAL_SETZERO(dec)	{ (dec).Lo64 = 0; (dec).Hi32 = 0; (dec).signscale = 0; }

typedef char CCHAR;
typedef DWORD LCID;
typedef PDWORD PLCID;
typedef WORD LANGID;

#endif

#endif /* WINPR_WTYPES_H */
