/**
 * WinPR: Windows Portable Runtime
 * Windows Native System Services
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_NT_H
#define WINPR_NT_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

typedef struct _PEB PEB;
typedef struct _PEB* PPEB;

typedef struct _TEB TEB;
typedef struct _TEB* PTEB;

/**
 * Process Environment Block
 */

struct _THREAD_BLOCK_ID
{
	DWORD ThreadId;
	TEB* ThreadEnvironmentBlock;
};
typedef struct _THREAD_BLOCK_ID THREAD_BLOCK_ID;

struct _PEB
{
	DWORD ThreadCount;
	DWORD ThreadArraySize;
	THREAD_BLOCK_ID* Threads;
};

/*
 * Thread Environment Block
 */

struct _TEB
{
	PEB* ProcessEnvironmentBlock;

	DWORD LastErrorValue;
	PVOID TlsSlots[64];
};

#define GENERIC_READ				0x80000000
#define GENERIC_WRITE				0x40000000
#define GENERIC_EXECUTE				0x20000000
#define GENERIC_ALL				0x10000000

#define DELETE					0x00010000
#define READ_CONTROL				0x00020000
#define WRITE_DAC				0x00040000
#define WRITE_OWNER				0x00080000
#define SYNCHRONIZE				0x00100000
#define STANDARD_RIGHTS_REQUIRED		0x000F0000
#define STANDARD_RIGHTS_READ			0x00020000
#define STANDARD_RIGHTS_WRITE			0x00020000
#define STANDARD_RIGHTS_EXECUTE			0x00020000
#define STANDARD_RIGHTS_ALL			0x001F0000
#define SPECIFIC_RIGHTS_ALL			0x0000FFFF
#define ACCESS_SYSTEM_SECURITY			0x01000000
#define MAXIMUM_ALLOWED				0x02000000

#define FILE_READ_DATA				0x0001
#define FILE_LIST_DIRECTORY			0x0001
#define FILE_WRITE_DATA				0x0002
#define FILE_ADD_FILE				0x0002
#define FILE_APPEND_DATA			0x0004
#define FILE_ADD_SUBDIRECTORY			0x0004
#define FILE_CREATE_PIPE_INSTANCE		0x0004
#define FILE_READ_EA				0x0008
#define FILE_WRITE_EA				0x0010
#define FILE_EXECUTE				0x0020
#define FILE_TRAVERSE				0x0020
#define FILE_DELETE_CHILD			0x0040
#define FILE_READ_ATTRIBUTES			0x0080
#define FILE_WRITE_ATTRIBUTES			0x0100

#define FILE_ALL_ACCESS		(STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x1FF)
#define FILE_GENERIC_READ	(STANDARD_RIGHTS_READ | FILE_READ_DATA | FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE)
#define FILE_GENERIC_WRITE	(STANDARD_RIGHTS_WRITE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_APPEND_DATA | SYNCHRONIZE)
#define FILE_GENERIC_EXECUTE	(STANDARD_RIGHTS_EXECUTE | FILE_READ_ATTRIBUTES | FILE_EXECUTE | SYNCHRONIZE)

#define FILE_SHARE_READ				0x00000001
#define FILE_SHARE_WRITE			0x00000002
#define FILE_SHARE_DELETE			0x00000004

#define FILE_DIRECTORY_FILE			0x00000001
#define FILE_WRITE_THROUGH			0x00000002
#define FILE_SEQUENTIAL_ONLY			0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING		0x00000008
#define FILE_SYNCHRONOUS_IO_ALERT		0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT		0x00000020
#define FILE_NON_DIRECTORY_FILE			0x00000040
#define FILE_CREATE_TREE_CONNECTION		0x00000080
#define FILE_COMPLETE_IF_OPLOCKED		0x00000100
#define FILE_NO_EA_KNOWLEDGE			0x00000200
#define FILE_OPEN_REMOTE_INSTANCE		0x00000400
#define FILE_RANDOM_ACCESS			0x00000800
#define FILE_DELETE_ON_CLOSE			0x00001000
#define FILE_OPEN_BY_FILE_ID			0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT		0x00004000
#define FILE_NO_COMPRESSION			0x00008000
#define FILE_OPEN_REQUIRING_OPLOCK		0x00010000
#define FILE_RESERVE_OPFILTER			0x00100000
#define FILE_OPEN_REPARSE_POINT			0x00200000
#define FILE_OPEN_NO_RECALL			0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY		0x00800000

#define FILE_VALID_OPTION_FLAGS			0x00FFFFFF
#define FILE_VALID_PIPE_OPTION_FLAGS		0x00000032
#define FILE_VALID_MAILSLOT_OPTION_FLAGS	0x00000032
#define FILE_VALID_SET_FLAGS			0x00000036

#define FILE_SUPERSEDED				0x00000000
#define FILE_OPENED				0x00000001
#define FILE_CREATED				0x00000002
#define FILE_OVERWRITTEN			0x00000003
#define FILE_EXISTS				0x00000004
#define FILE_DOES_NOT_EXIST			0x00000005

typedef DWORD ACCESS_MASK;
typedef ACCESS_MASK* PACCESS_MASK;

typedef CONST char *PCSZ;

typedef struct _STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PCHAR Buffer;
} STRING;
typedef STRING *PSTRING;

typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;
typedef PSTRING PCANSI_STRING;

typedef STRING OEM_STRING;
typedef PSTRING POEM_STRING;
typedef CONST STRING* PCOEM_STRING;

typedef struct _LSA_UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

#define OBJ_INHERIT				0x00000002L
#define OBJ_PERMANENT				0x00000010L
#define OBJ_EXCLUSIVE				0x00000020L
#define OBJ_CASE_INSENSITIVE			0x00000040L
#define OBJ_OPENIF				0x00000080L
#define OBJ_OPENLINK				0x00000100L
#define OBJ_KERNEL_HANDLE			0x00000200L
#define OBJ_FORCE_ACCESS_CHECK			0x00000400L
#define OBJ_VALID_ATTRIBUTES			0x000007F2L

typedef struct _OBJECT_ATTRIBUTES
{
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;
	PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

typedef struct _IO_STATUS_BLOCK
{
	union
	{
		NTSTATUS status;
		PVOID Pointer;
	} DUMMYUNIONNAME;

	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef VOID (*PIO_APC_ROUTINE)(PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, ULONG Reserved);

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API PTEB NtCurrentTeb(void);

WINPR_API VOID RtlInitAnsiString(PANSI_STRING DestinationString, PCSZ SourceString);

WINPR_API VOID RtlInitUnicodeString(PUNICODE_STRING DestinationString, PCWSTR SourceString);

WINPR_API NTSTATUS RtlAnsiStringToUnicodeString(PUNICODE_STRING DestinationString,
		PCANSI_STRING SourceString, BOOLEAN AllocateDestinationString);

WINPR_API VOID RtlFreeUnicodeString(PUNICODE_STRING UnicodeString);

WINPR_API VOID InitializeObjectAttributes(POBJECT_ATTRIBUTES InitializedAttributes,
		PUNICODE_STRING ObjectName, ULONG Attributes, HANDLE RootDirectory,
		PSECURITY_DESCRIPTOR SecurityDescriptor);

WINPR_API NTSTATUS NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
		ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength);

WINPR_API NTSTATUS NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG ShareAccess, ULONG OpenOptions);

WINPR_API NTSTATUS NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event,
		PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG IoControlCode, PVOID InputBuffer, ULONG InputBufferLength,
		PVOID OutputBuffer, ULONG OutputBufferLength);

WINPR_API NTSTATUS NtClose(HANDLE Handle);

#ifdef __cplusplus
}
#endif

#endif

#endif /* WINPR_NT_H */

