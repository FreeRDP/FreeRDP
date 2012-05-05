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

#ifndef _WIN32

#define __int8	char
#define __int16 short
#define __int32 int
#define __int64 long long

#if __x86_64__
#define __int3264 __int64
#else
#define __int3264 __int32
#endif

typedef int BOOL, *PBOOL, *LPBOOL;
typedef unsigned char BYTE, *PBYTE, *LPBYTE;
typedef BYTE BOOLEAN, *PBOOLEAN;
typedef wchar_t WCHAR, *PWCHAR;
typedef WCHAR* BSTR;
typedef char CHAR, *PCHAR;
typedef unsigned long DWORD, *PDWORD, *LPDWORD;
typedef unsigned int DWORD32;
typedef unsigned __int64 DWORD64;
typedef unsigned __int64 ULONGLONG;
typedef ULONGLONG DWORDLONG, *PDWORDLONG;
typedef float FLOAT;
typedef unsigned char UCHAR, *PUCHAR;
typedef short SHORT;

typedef void* HANDLE;
typedef DWORD HCALL;
typedef int INT, *LPINT;
typedef signed char INT8;
typedef signed short INT16;
typedef signed int INT32;
typedef signed __int64 INT64;
typedef const wchar_t* LMCSTR;
typedef WCHAR* LMSTR;
typedef long LONG, *PLONG, *LPLONG;
typedef signed __int64 LONGLONG;
typedef LONG HRESULT;

typedef __int3264 LONG_PTR;
typedef unsigned __int3264 ULONG_PTR;

typedef signed int LONG32;
typedef signed __int64 LONG64;
typedef const char* LPCSTR;

typedef const wchar_t* LPCWSTR;
typedef char* PSTR, *LPSTR;

typedef wchar_t* LPWSTR, *PWSTR;
typedef long NTSTATUS;

typedef unsigned __int64 QWORD;
typedef UCHAR* STRING;

typedef unsigned int UINT;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned __int64 UINT64;
typedef unsigned long ULONG, *PULONG;

typedef ULONG_PTR DWORD_PTR;
typedef ULONG_PTR SIZE_T;
typedef unsigned int ULONG32;
typedef unsigned __int64 ULONG64;
typedef wchar_t UNICODE;
typedef unsigned short USHORT;
typedef void VOID, *PVOID, *LPVOID;
typedef const void *LPCVOID;
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
	unsigned long Data1;
	unsigned short Data2;
	unsigned short Data3;
	BYTE Data4[8];
} GUID, UUID, *PGUID;

#ifdef UNICODE
#define _T(x)	L ## x
#else
#define _T(x)	x
#endif

#ifdef UNICODE
typedef LPWSTR LPTSTR;
typedef LPCWSTR LPCTSTR;
#else
typedef LPSTR LPTSTR;
typedef LPCSTR LPCTSTR;
#endif

typedef struct _FILETIME
{
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

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

#endif

typedef BYTE byte;
typedef double DOUBLE;

typedef void* PCONTEXT_HANDLE;
typedef PCONTEXT_HANDLE* PPCONTEXT_HANDLE;

typedef unsigned long error_status_t;

#endif /* WINPR_WTYPES_H */
