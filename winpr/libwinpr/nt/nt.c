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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/nt.h>

/**
 * NtXxx Routines:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff557720/
 */

#ifndef _WIN32

#include <pthread.h>

#include <winpr/crt.h>

/**
 * The current implementation of NtCurrentTeb() is not the most efficient
 * but it's a starting point. Beware of potential performance bottlenecks
 * caused by multithreaded usage of SetLastError/GetLastError.
 */

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static PPEB g_ProcessEnvironmentBlock = NULL;

static void NtThreadEnvironmentBlockFree(PTEB teb);
static void NtProcessEnvironmentBlockFree(PPEB peb);

static PTEB NtThreadEnvironmentBlockNew()
{
	PTEB teb = NULL;
	pthread_key_t key;

	teb = (PTEB) malloc(sizeof(TEB));

	if (teb)
	{
		ZeroMemory(teb, sizeof(TEB));

		/**
		 * We are not really using the key, but it provides an automatic way
		 * of calling NtThreadEnvironmentBlockFree on thread termination for
		 * the current Thread Environment Block.
		 */

		pthread_key_create(&key, (void (*)(void*)) NtThreadEnvironmentBlockFree);
		pthread_setspecific(key, (void*) teb);
	}

	return teb;
}

static void NtThreadEnvironmentBlockFree(PTEB teb)
{
	DWORD index;
	PPEB peb = NULL;

	peb = teb->ProcessEnvironmentBlock;

	pthread_mutex_lock(&mutex);

	for (index = 0; index < peb->ThreadArraySize; index++)
	{
		if (peb->Threads[index].ThreadEnvironmentBlock == teb)
		{
			peb->Threads[index].ThreadId = 0;
			peb->Threads[index].ThreadEnvironmentBlock = NULL;
			peb->ThreadCount--;
			break;
		}
	}

	if (!peb->ThreadCount)
	{
		NtProcessEnvironmentBlockFree(peb);
	}

	pthread_mutex_unlock(&mutex);

	free(teb);
}

static PPEB NtProcessEnvironmentBlockNew()
{
	PPEB peb = NULL;

	peb = (PPEB) malloc(sizeof(PEB));

	if (peb)
	{
		ZeroMemory(peb, sizeof(PEB));

		peb->ThreadCount = 0;
		peb->ThreadArraySize = 64;
		peb->Threads = (THREAD_BLOCK_ID*) malloc(sizeof(THREAD_BLOCK_ID) * peb->ThreadArraySize);

		if (peb->Threads)
		{
			ZeroMemory(peb->Threads, sizeof(THREAD_BLOCK_ID) * peb->ThreadArraySize);
		}
	}

	return peb;
}

static void NtProcessEnvironmentBlockFree(PPEB peb)
{
	if (peb)
	{
		free(peb->Threads);
		free(peb);
	}

	g_ProcessEnvironmentBlock = NULL;
}

PPEB NtCurrentPeb(void)
{
	PPEB peb = NULL;

	pthread_mutex_lock(&mutex);

	if (!g_ProcessEnvironmentBlock)
		g_ProcessEnvironmentBlock = NtProcessEnvironmentBlockNew();

	peb = g_ProcessEnvironmentBlock;

	pthread_mutex_unlock(&mutex);

	return peb;
}

PTEB NtCurrentTeb(void)
{
	DWORD index;
	int freeIndex;
	DWORD ThreadId;
	PPEB peb = NULL;
	PTEB teb = NULL;

	peb = NtCurrentPeb();

	ThreadId = (DWORD) pthread_self();

	freeIndex = -1;

	pthread_mutex_lock(&mutex);

	for (index = 0; index < peb->ThreadArraySize; index++)
	{
		if (!peb->Threads[index].ThreadId)
		{
			if (freeIndex < 0)
				freeIndex = (int) index;
		}

		if (peb->Threads[index].ThreadId == ThreadId)
		{
			teb = peb->Threads[index].ThreadEnvironmentBlock;
			break;
		}
	}

	if (!teb)
	{
		if (freeIndex >= 0)
		{
			teb = NtThreadEnvironmentBlockNew();
			peb->Threads[freeIndex].ThreadEnvironmentBlock = teb;
			peb->Threads[freeIndex].ThreadId = ThreadId;
			peb->ThreadCount++;

			teb->ProcessEnvironmentBlock = peb;
			teb->LastErrorValue = 0;
		}
	}

	pthread_mutex_unlock(&mutex);

	return teb;
}

/**
 * RtlInitAnsiString routine:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff561918/
 */

VOID RtlInitAnsiString(PANSI_STRING DestinationString, PCSZ SourceString)
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

VOID RtlInitUnicodeString(PUNICODE_STRING DestinationString, PCWSTR SourceString)
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

NTSTATUS RtlAnsiStringToUnicodeString(PUNICODE_STRING DestinationString,
		PCANSI_STRING SourceString, BOOLEAN AllocateDestinationString)
{
	int index;

	if (!SourceString)
	{
		RtlInitUnicodeString(DestinationString, NULL);
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

VOID RtlFreeUnicodeString(PUNICODE_STRING UnicodeString)
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

ULONG RtlNtStatusToDosError(NTSTATUS status)
{
	return status;
}

/**
 * InitializeObjectAttributes macro
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff547804/
 */

VOID InitializeObjectAttributes(POBJECT_ATTRIBUTES InitializedAttributes,
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

/**
 * NtCreateFile function:
 * http://msdn.microsoft.com/en-us/library/bb432380/
 */

NTSTATUS NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
		ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength)
{
	return 0;
}

/**
 * NtOpenFile function:
 * http://msdn.microsoft.com/en-us/library/bb432381/
 */

NTSTATUS NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
		ULONG ShareAccess, ULONG OpenOptions)
{
	return 0;
}

/**
 * NtDeviceIoControlFile function:
 * http://msdn.microsoft.com/en-us/library/ms648411/
 */

NTSTATUS NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event,
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

NTSTATUS NtClose(HANDLE Handle)
{
	return 0;
}

/**
 * NtWaitForSingleObject function:
 * http://msdn.microsoft.com/en-us/library/ms648412/
 */

NTSTATUS NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout)
{
	return 0;
}

#endif

