/**
 * WinPR: Windows Portable Runtime
 * Windows Native System Services
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/library.h>
#include <winpr/wlog.h>
#include <winpr/nt.h>

#include "../log.h"
#define TAG WINPR_TAG("nt")


/**
 * NtXxx Routines:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff557720/
 */

/**
 * InitializeObjectAttributes macro
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff547804/
 */

VOID _InitializeObjectAttributes(POBJECT_ATTRIBUTES InitializedAttributes,
		PUNICODE_STRING ObjectName, ULONG Attributes, HANDLE RootDirectory,
		PSECURITY_DESCRIPTOR SecurityDescriptor)
{
#if defined(_WIN32) && !defined(_UWP)
	InitializeObjectAttributes(InitializedAttributes, ObjectName,
		Attributes, RootDirectory, SecurityDescriptor);
#else
	InitializedAttributes->Length = sizeof(OBJECT_ATTRIBUTES);
	InitializedAttributes->ObjectName = ObjectName;
	InitializedAttributes->Attributes = Attributes;
	InitializedAttributes->RootDirectory = RootDirectory;
	InitializedAttributes->SecurityDescriptor = SecurityDescriptor;
	InitializedAttributes->SecurityQualityOfService = NULL;
#endif
}

#ifndef _WIN32


#include <pthread.h>
#include <winpr/crt.h>

#include "../handle/handle.h"

struct winpr_nt_file
{
	WINPR_HANDLE_DEF();

	ACCESS_MASK DesiredAccess;
	OBJECT_ATTRIBUTES ObjectAttributes;
	ULONG FileAttributes;
	ULONG ShareAccess;
	ULONG CreateDisposition;
	ULONG CreateOptions;
};
typedef struct winpr_nt_file WINPR_NT_FILE;

static pthread_once_t _TebOnceControl = PTHREAD_ONCE_INIT;
static pthread_key_t  _TebKey;

static void _TebDestruct(void *teb)
{
	free(teb);
}

static void _TebInitOnce(void)
{
	pthread_key_create(&_TebKey, _TebDestruct);
}

PTEB NtCurrentTeb(void)
{
	PTEB teb = NULL;

	if (pthread_once(&_TebOnceControl, _TebInitOnce) == 0)
	{
		if ((teb = pthread_getspecific(_TebKey)) == NULL)
		{
			teb = calloc(1, sizeof(TEB));
			if (teb)
				pthread_setspecific(_TebKey, teb);
		}
	}
	return teb;
}

/**
 * RtlInitAnsiString routine:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff561918/
 */

VOID _RtlInitAnsiString(PANSI_STRING DestinationString, PCSZ SourceString)
{
	DestinationString->Buffer = (PCHAR) SourceString;

	if (!SourceString)
	{
		DestinationString->Length = 0;
		DestinationString->MaximumLength = 0;
	}
	else
	{
		USHORT length = (USHORT) strlen(SourceString);
		DestinationString->Length = length;
		DestinationString->MaximumLength = length + 1;
	}
}

/**
 * RtlInitUnicodeString routine:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff561934/
 */

VOID _RtlInitUnicodeString(PUNICODE_STRING DestinationString, PCWSTR SourceString)
{
	DestinationString->Buffer = (PWSTR) SourceString;

	if (!SourceString)
	{
		DestinationString->Length = 0;
		DestinationString->MaximumLength = 0;
	}
	else
	{
		USHORT length = (USHORT) _wcslen(SourceString);
		DestinationString->Length = length * 2;
		DestinationString->MaximumLength = (length + 1) * 2;
	}
}

/**
 * RtlAnsiStringToUnicodeString function:
 * http://msdn.microsoft.com/en-us/library/ms648413/
 */

NTSTATUS _RtlAnsiStringToUnicodeString(PUNICODE_STRING DestinationString,
		PCANSI_STRING SourceString, BOOLEAN AllocateDestinationString)
{
	int index;

	if (!SourceString)
		return STATUS_INVALID_PARAMETER;

	if (AllocateDestinationString)
	{
		PWSTR wbuf = NULL;

		if (SourceString->MaximumLength)
		{
			if (!(wbuf = (PWSTR) malloc(SourceString->MaximumLength * 2)))
				return STATUS_NO_MEMORY;
		}

		DestinationString->MaximumLength = SourceString->MaximumLength * 2;
		DestinationString->Buffer = wbuf;
	}
	else
	{
		if (DestinationString->MaximumLength < SourceString->MaximumLength * 2)
			return STATUS_BUFFER_OVERFLOW;
	}

	for (index = 0; index < SourceString->MaximumLength; index++)
	{
		DestinationString->Buffer[index] = (WCHAR) SourceString->Buffer[index];
	}

	DestinationString->Length = SourceString->Length * 2;

	return STATUS_SUCCESS;
}

/**
 * RtlFreeUnicodeString function:
 * http://msdn.microsoft.com/en-us/library/ms648418/
 */

VOID _RtlFreeUnicodeString(PUNICODE_STRING UnicodeString)
{
	if (UnicodeString)
	{
		free(UnicodeString->Buffer);

		UnicodeString->Length = 0;
		UnicodeString->MaximumLength = 0;
	}
}

/**
 * RtlNtStatusToDosError function:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms680600/
 */

ULONG _RtlNtStatusToDosError(NTSTATUS status)
{
	return status;
}

/**
 * NtCreateFile function:
 * http://msdn.microsoft.com/en-us/library/bb432380/
 */

NTSTATUS _NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
		ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength)
{
#if 1
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	return STATUS_NOT_SUPPORTED;
#else
	WINPR_NT_FILE* pFileHandle;

	pFileHandle = (WINPR_NT_FILE*) calloc(1, sizeof(WINPR_NT_FILE));
	if (!pFileHandle)
		return STATUS_NO_MEMORY;

	pFileHandle->DesiredAccess = DesiredAccess;
	pFileHandle->FileAttributes = FileAttributes;
	pFileHandle->ShareAccess = ShareAccess;
	pFileHandle->CreateDisposition = CreateDisposition;
	pFileHandle->CreateOptions = CreateOptions;

	*((PULONG_PTR) FileHandle) = (ULONG_PTR) pFileHandle;

	//STATUS_ACCESS_DENIED
	//STATUS_OBJECT_NAME_INVALID
	//STATUS_OBJECT_PATH_NOT_FOUND
	//STATUS_OBJECT_NAME_NOT_FOUND

	return STATUS_SUCCESS;
#endif
}

/**
 * NtOpenFile function:
 * http://msdn.microsoft.com/en-us/library/bb432381/
 */

NTSTATUS _NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG ShareAccess, ULONG OpenOptions)
{
#if 1
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	return STATUS_NOT_SUPPORTED;
#else
	WINPR_NT_FILE* pFileHandle;

	pFileHandle = (WINPR_NT_FILE*) calloc(1, sizeof(WINPR_NT_FILE));

	if (!pFileHandle)
		return STATUS_NO_MEMORY;

	pFileHandle->DesiredAccess = DesiredAccess;
	pFileHandle->ShareAccess = ShareAccess;

	*((PULONG_PTR) FileHandle) = (ULONG_PTR) pFileHandle;

	return STATUS_SUCCESS;
#endif
}

/**
 * NtReadFile function:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff567072/
 */

NTSTATUS _NtReadFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
#if 1
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	return STATUS_NOT_SUPPORTED;
#else
	return STATUS_SUCCESS;
#endif
}

/**
 * NtWriteFile function:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff567121/
 */

NTSTATUS _NtWriteFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
#if 1
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	return STATUS_NOT_SUPPORTED;
#else
	return STATUS_SUCCESS;
#endif
}

/**
 * NtDeviceIoControlFile function:
 * http://msdn.microsoft.com/en-us/library/ms648411/
 */

NTSTATUS _NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event,
		PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG IoControlCode, PVOID InputBuffer, ULONG InputBufferLength,
		PVOID OutputBuffer, ULONG OutputBufferLength)
{
#if 1
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	return STATUS_NOT_SUPPORTED;
#else
	return STATUS_SUCCESS;
#endif
}

/**
 * NtClose function:
 * http://msdn.microsoft.com/en-us/library/ms648410/
 */

NTSTATUS _NtClose(HANDLE Handle)
{
#if 1
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	return STATUS_NOT_SUPPORTED;
#else
	WINPR_NT_FILE* pFileHandle;

	if (!Handle)
		return STATUS_SUCCESS;

	pFileHandle = (WINPR_NT_FILE*) Handle;

	free(pFileHandle);

	return STATUS_SUCCESS;
#endif
}

/**
 * NtWaitForSingleObject function:
 * http://msdn.microsoft.com/en-us/library/ms648412/
 */

NTSTATUS _NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout)
{
#if 1
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	return STATUS_NOT_SUPPORTED;
#else
	return STATUS_SUCCESS;
#endif
}

#else

#include <winpr/synch.h>

typedef VOID (WINAPI * RTL_INIT_ANSI_STRING_FN)(PANSI_STRING DestinationString, PCSZ SourceString);

typedef VOID (WINAPI * RTL_INIT_UNICODE_STRING_FN)(PUNICODE_STRING DestinationString, PCWSTR SourceString);

typedef NTSTATUS (WINAPI * RTL_ANSI_STRING_TO_UNICODE_STRING_FN)(PUNICODE_STRING DestinationString,
		PCANSI_STRING SourceString, BOOLEAN AllocateDestinationString);

typedef VOID (WINAPI * RTL_FREE_UNICODE_STRING_FN)(PUNICODE_STRING UnicodeString);

typedef ULONG (WINAPI * RTL_NT_STATUS_TO_DOS_ERROR_FN)(NTSTATUS status);

typedef NTSTATUS (WINAPI * NT_CREATE_FILE_FN)(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
		ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength);

typedef NTSTATUS (WINAPI * NT_OPEN_FILE_FN)(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG ShareAccess, ULONG OpenOptions);

typedef NTSTATUS (WINAPI * NT_READ_FILE_FN)(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key);

typedef NTSTATUS (WINAPI * NT_WRITE_FILE_FN)(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key);

typedef NTSTATUS (WINAPI * NT_DEVICE_IO_CONTROL_FILE_FN)(HANDLE FileHandle, HANDLE Event,
		PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG IoControlCode, PVOID InputBuffer, ULONG InputBufferLength,
		PVOID OutputBuffer, ULONG OutputBufferLength);

typedef NTSTATUS (WINAPI * NT_CLOSE_FN)(HANDLE Handle);

typedef NTSTATUS (WINAPI * NT_WAIT_FOR_SINGLE_OBJECT_FN)(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout);

static RTL_INIT_ANSI_STRING_FN pRtlInitAnsiString = NULL;
static RTL_INIT_UNICODE_STRING_FN pRtlInitUnicodeString = NULL;
static RTL_ANSI_STRING_TO_UNICODE_STRING_FN pRtlAnsiStringToUnicodeString = NULL;
static RTL_FREE_UNICODE_STRING_FN pRtlFreeUnicodeString = NULL;
static RTL_NT_STATUS_TO_DOS_ERROR_FN pRtlNtStatusToDosError = NULL;
static NT_CREATE_FILE_FN pNtCreateFile = NULL;
static NT_OPEN_FILE_FN pNtOpenFile = NULL;
static NT_READ_FILE_FN pNtReadFile = NULL;
static NT_WRITE_FILE_FN pNtWriteFile = NULL;
static NT_DEVICE_IO_CONTROL_FILE_FN pNtDeviceIoControlFile = NULL;
static NT_CLOSE_FN pNtClose = NULL;
static NT_WAIT_FOR_SINGLE_OBJECT_FN pNtWaitForSingleObject = NULL;

static INIT_ONCE ntdllInitOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK NtdllModuleInit(PINIT_ONCE once, PVOID param, PVOID *context)
{
	HMODULE NtdllModule = LoadLibraryA("ntdll.dll");

	if (NtdllModule)
	{
		pRtlInitAnsiString = (RTL_INIT_ANSI_STRING_FN)GetProcAddress(NtdllModule, "RtlInitAnsiString");
		pRtlInitUnicodeString = (RTL_INIT_UNICODE_STRING_FN)GetProcAddress(NtdllModule, "RtlInitUnicodeString");
		pRtlAnsiStringToUnicodeString = (RTL_ANSI_STRING_TO_UNICODE_STRING_FN)GetProcAddress(NtdllModule, "RtlAnsiStringToUnicodeString");
		pRtlFreeUnicodeString = (RTL_FREE_UNICODE_STRING_FN)GetProcAddress(NtdllModule, "RtlFreeUnicodeString");
		pRtlNtStatusToDosError = (RTL_NT_STATUS_TO_DOS_ERROR_FN)GetProcAddress(NtdllModule, "RtlNtStatusToDosError");
		pNtCreateFile = (NT_CREATE_FILE_FN)GetProcAddress(NtdllModule, "NtCreateFile");
		pNtOpenFile = (NT_OPEN_FILE_FN)GetProcAddress(NtdllModule, "NtOpenFile");
		pNtReadFile = (NT_READ_FILE_FN)GetProcAddress(NtdllModule, "NtReadFile");
		pNtWriteFile = (NT_WRITE_FILE_FN)GetProcAddress(NtdllModule, "NtWriteFile");
		pNtDeviceIoControlFile = (NT_DEVICE_IO_CONTROL_FILE_FN)GetProcAddress(NtdllModule, "NtDeviceIoControlFile");
		pNtClose = (NT_CLOSE_FN)GetProcAddress(NtdllModule, "NtClose");
		pNtWaitForSingleObject = (NT_WAIT_FOR_SINGLE_OBJECT_FN)GetProcAddress(NtdllModule, "NtWaitForSingleObject");
	}
	return TRUE;
}

VOID _RtlInitAnsiString(PANSI_STRING DestinationString, PCSZ SourceString)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pRtlInitAnsiString)
		return;

	pRtlInitAnsiString(DestinationString, SourceString);
}

VOID _RtlInitUnicodeString(PUNICODE_STRING DestinationString, PCWSTR SourceString)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pRtlInitUnicodeString)
		return;

	pRtlInitUnicodeString(DestinationString, SourceString);
}

NTSTATUS _RtlAnsiStringToUnicodeString(PUNICODE_STRING DestinationString,
		PCANSI_STRING SourceString, BOOLEAN AllocateDestinationString)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pRtlAnsiStringToUnicodeString)
		return STATUS_INTERNAL_ERROR;

	return pRtlAnsiStringToUnicodeString(DestinationString,
		SourceString, AllocateDestinationString);
}

VOID _RtlFreeUnicodeString(PUNICODE_STRING UnicodeString)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pRtlFreeUnicodeString)
		return;

	pRtlFreeUnicodeString(UnicodeString);
}

ULONG _RtlNtStatusToDosError(NTSTATUS status)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pRtlNtStatusToDosError)
		return status;

	return pRtlNtStatusToDosError(status);
}

NTSTATUS _NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
		ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pNtCreateFile)
		return STATUS_INTERNAL_ERROR;

	return pNtCreateFile(FileHandle, DesiredAccess, ObjectAttributes,
		IoStatusBlock, AllocationSize, FileAttributes, ShareAccess,
		CreateDisposition, CreateOptions, EaBuffer, EaLength);
}

NTSTATUS _NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG ShareAccess, ULONG OpenOptions)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pNtOpenFile)
		return STATUS_INTERNAL_ERROR;

	return pNtOpenFile(FileHandle, DesiredAccess, ObjectAttributes,
		IoStatusBlock, ShareAccess, OpenOptions);
}

NTSTATUS _NtReadFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pNtReadFile)
		return STATUS_INTERNAL_ERROR;

	return pNtReadFile(FileHandle, Event, ApcRoutine, ApcContext,
		IoStatusBlock, Buffer, Length, ByteOffset, Key);
}

NTSTATUS _NtWriteFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pNtWriteFile)
		return STATUS_INTERNAL_ERROR;

	return pNtWriteFile(FileHandle, Event, ApcRoutine, ApcContext,
		IoStatusBlock, Buffer, Length, ByteOffset, Key);
}

NTSTATUS _NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event,
		PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG IoControlCode, PVOID InputBuffer, ULONG InputBufferLength,
		PVOID OutputBuffer, ULONG OutputBufferLength)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pNtDeviceIoControlFile)
		return STATUS_INTERNAL_ERROR;

	return pNtDeviceIoControlFile(FileHandle, Event,
		ApcRoutine, ApcContext, IoStatusBlock, IoControlCode,
		InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
}

NTSTATUS _NtClose(HANDLE Handle)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pNtClose)
		return STATUS_INTERNAL_ERROR;

	return pNtClose(Handle);
}

NTSTATUS _NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout)
{
	InitOnceExecuteOnce(&ntdllInitOnce, NtdllModuleInit, NULL, NULL);

	if (!pNtWaitForSingleObject)
		return STATUS_INTERNAL_ERROR;

	return pNtWaitForSingleObject(Handle, Alertable, Timeout);
}

#endif

