/**
 * WinPR: Windows Portable Runtime
 * Windows Native System Services
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Thinstuff Technologies GmbH
 * Copyright 2013 Norbert Federa <nfedera@thinstuff.at>
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

#include <winpr/nt.h>

/**
 * NtXxx Routines:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff557720/
 */

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
	{
		_RtlInitUnicodeString(DestinationString, NULL);
		return 0;
	}

	if (AllocateDestinationString)
	{
		DestinationString->Length = SourceString->Length * 2;
		DestinationString->MaximumLength = SourceString->MaximumLength * 2;

		DestinationString->Buffer = (PWSTR) malloc(DestinationString->MaximumLength);

		for (index = 0; index < SourceString->MaximumLength; index++)
		{
			DestinationString->Buffer[index] = (WCHAR) SourceString->Buffer[index];
		}
	}
	else
	{

	}

	return 0;
}

/**
 * RtlFreeUnicodeString function:
 * http://msdn.microsoft.com/en-us/library/ms648418/
 */

VOID _RtlFreeUnicodeString(PUNICODE_STRING UnicodeString)
{
	if (UnicodeString)
	{
		if (UnicodeString->Buffer)
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
 * InitializeObjectAttributes macro
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff547804/
 */

VOID _InitializeObjectAttributes(POBJECT_ATTRIBUTES InitializedAttributes,
		PUNICODE_STRING ObjectName, ULONG Attributes, HANDLE RootDirectory,
		PSECURITY_DESCRIPTOR SecurityDescriptor)
{
	InitializedAttributes->Length = sizeof(OBJECT_ATTRIBUTES);
	InitializedAttributes->ObjectName = ObjectName;
	InitializedAttributes->Attributes = Attributes;
	InitializedAttributes->RootDirectory = RootDirectory;
	InitializedAttributes->SecurityDescriptor = SecurityDescriptor;
	InitializedAttributes->SecurityQualityOfService = NULL;
}

#ifndef _WIN32

#include "nt.h"

#include <pthread.h>

#include <winpr/crt.h>

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
			teb = malloc(sizeof(TEB));
			if (teb)
			{
				ZeroMemory(teb, sizeof(TEB));
				pthread_setspecific(_TebKey, teb);
			}
		}
	}
	return teb;
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
	WINPR_FILE* pFileHandle;

	pFileHandle = (WINPR_FILE*) malloc(sizeof(WINPR_FILE));

	if (!pFileHandle)
		return 0;

	ZeroMemory(pFileHandle, sizeof(WINPR_FILE));

	pFileHandle->DesiredAccess = DesiredAccess;
	pFileHandle->FileAttributes = FileAttributes;
	pFileHandle->ShareAccess = ShareAccess;
	pFileHandle->CreateDisposition = CreateDisposition;
	pFileHandle->CreateOptions = CreateOptions;

	*((PULONG_PTR) FileHandle) = (ULONG_PTR) pFileHandle;

	return STATUS_SUCCESS;
}

/**
 * NtOpenFile function:
 * http://msdn.microsoft.com/en-us/library/bb432381/
 */

NTSTATUS _NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG ShareAccess, ULONG OpenOptions)
{
	WINPR_FILE* pFileHandle;

	pFileHandle = (WINPR_FILE*) malloc(sizeof(WINPR_FILE));

	if (!pFileHandle)
		return 0;

	ZeroMemory(pFileHandle, sizeof(WINPR_FILE));

	pFileHandle->DesiredAccess = DesiredAccess;
	pFileHandle->ShareAccess = ShareAccess;

	*((PULONG_PTR) FileHandle) = (ULONG_PTR) pFileHandle;

	return STATUS_SUCCESS;
}

/**
 * NtReadFile function:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff567072/
 */

NTSTATUS _NtReadFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
	return STATUS_SUCCESS;
}

/**
 * NtWriteFile function:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff567121/
 */

NTSTATUS _NtWriteFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
	return STATUS_SUCCESS;
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
	return 0;
}

/**
 * NtClose function:
 * http://msdn.microsoft.com/en-us/library/ms648410/
 */

NTSTATUS _NtClose(HANDLE Handle)
{
	WINPR_FILE* pFileHandle;

	if (!Handle)
		return 0;

	pFileHandle = (WINPR_FILE*) Handle;

	free(pFileHandle);

	return STATUS_SUCCESS;
}

/**
 * NtWaitForSingleObject function:
 * http://msdn.microsoft.com/en-us/library/ms648412/
 */

NTSTATUS _NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout)
{
	return 0;
}

#else

NTSTATUS _NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
		ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength)
{
	return STATUS_SUCCESS;
}

NTSTATUS _NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG ShareAccess, ULONG OpenOptions)
{
	return STATUS_SUCCESS;
}

NTSTATUS _NtReadFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
	return STATUS_SUCCESS;
}

NTSTATUS _NtWriteFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
	return STATUS_SUCCESS;
}

NTSTATUS _NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event,
		PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG IoControlCode, PVOID InputBuffer, ULONG InputBufferLength,
		PVOID OutputBuffer, ULONG OutputBufferLength)
{
	return STATUS_SUCCESS;
}

NTSTATUS _NtClose(HANDLE Handle)
{
	return STATUS_SUCCESS;
}

#endif

