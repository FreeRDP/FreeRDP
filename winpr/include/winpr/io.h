/**
 * WinPR: Windows Portable Runtime
 * Asynchronous I/O Functions
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

#ifndef WINPR_IO_H
#define WINPR_IO_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifdef _WIN32

#include <winioctl.h>

#else

#include <winpr/nt.h>

typedef struct _OVERLAPPED
{
	ULONG_PTR Internal;
	ULONG_PTR InternalHigh;
	union
	{
		struct
		{
			DWORD Offset;
			DWORD OffsetHigh;
		};
		PVOID Pointer;
	};
	HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef struct _OVERLAPPED_ENTRY
{
	ULONG_PTR lpCompletionKey;
	LPOVERLAPPED lpOverlapped;
	ULONG_PTR Internal;
	DWORD dwNumberOfBytesTransferred;
} OVERLAPPED_ENTRY, *LPOVERLAPPED_ENTRY;

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait);

WINPR_API BOOL GetOverlappedResultEx(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, DWORD dwMilliseconds, BOOL bAlertable);

WINPR_API BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
		LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

WINPR_API HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads);

WINPR_API BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
		PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped, DWORD dwMilliseconds);

WINPR_API BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
		ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds, BOOL fAlertable);

WINPR_API BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL CancelIo(HANDLE hFile);

WINPR_API BOOL CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL CancelSynchronousIo(HANDLE hThread);

#ifdef __cplusplus
}
#endif

#define DEVICE_TYPE ULONG

#define FILE_DEVICE_BEEP			0x00000001
#define FILE_DEVICE_CD_ROM			0x00000002
#define FILE_DEVICE_CD_ROM_FILE_SYSTEM		0x00000003
#define FILE_DEVICE_CONTROLLER			0x00000004
#define FILE_DEVICE_DATALINK			0x00000005
#define FILE_DEVICE_DFS				0x00000006
#define FILE_DEVICE_DISK			0x00000007
#define FILE_DEVICE_DISK_FILE_SYSTEM		0x00000008
#define FILE_DEVICE_FILE_SYSTEM			0x00000009
#define FILE_DEVICE_INPORT_PORT			0x0000000a
#define FILE_DEVICE_KEYBOARD			0x0000000b
#define FILE_DEVICE_MAILSLOT			0x0000000c
#define FILE_DEVICE_MIDI_IN			0x0000000d
#define FILE_DEVICE_MIDI_OUT			0x0000000e
#define FILE_DEVICE_MOUSE			0x0000000f
#define FILE_DEVICE_MULTI_UNC_PROVIDER		0x00000010
#define FILE_DEVICE_NAMED_PIPE			0x00000011
#define FILE_DEVICE_NETWORK			0x00000012
#define FILE_DEVICE_NETWORK_BROWSER		0x00000013
#define FILE_DEVICE_NETWORK_FILE_SYSTEM		0x00000014
#define FILE_DEVICE_NULL			0x00000015
#define FILE_DEVICE_PARALLEL_PORT		0x00000016
#define FILE_DEVICE_PHYSICAL_NETCARD		0x00000017
#define FILE_DEVICE_PRINTER			0x00000018
#define FILE_DEVICE_SCANNER			0x00000019
#define FILE_DEVICE_SERIAL_MOUSE_PORT		0x0000001a
#define FILE_DEVICE_SERIAL_PORT			0x0000001b
#define FILE_DEVICE_SCREEN			0x0000001c
#define FILE_DEVICE_SOUND			0x0000001d
#define FILE_DEVICE_STREAMS			0x0000001e
#define FILE_DEVICE_TAPE			0x0000001f
#define FILE_DEVICE_TAPE_FILE_SYSTEM		0x00000020
#define FILE_DEVICE_TRANSPORT			0x00000021
#define FILE_DEVICE_UNKNOWN			0x00000022
#define FILE_DEVICE_VIDEO			0x00000023
#define FILE_DEVICE_VIRTUAL_DISK		0x00000024
#define FILE_DEVICE_WAVE_IN			0x00000025
#define FILE_DEVICE_WAVE_OUT			0x00000026
#define FILE_DEVICE_8042_PORT			0x00000027
#define FILE_DEVICE_NETWORK_REDIRECTOR		0x00000028
#define FILE_DEVICE_BATTERY			0x00000029
#define FILE_DEVICE_BUS_EXTENDER		0x0000002a
#define FILE_DEVICE_MODEM			0x0000002b
#define FILE_DEVICE_VDM				0x0000002c
#define FILE_DEVICE_MASS_STORAGE		0x0000002d
#define FILE_DEVICE_SMB				0x0000002e
#define FILE_DEVICE_KS				0x0000002f
#define FILE_DEVICE_CHANGER			0x00000030
#define FILE_DEVICE_SMARTCARD			0x00000031
#define FILE_DEVICE_ACPI			0x00000032
#define FILE_DEVICE_DVD				0x00000033
#define FILE_DEVICE_FULLSCREEN_VIDEO		0x00000034
#define FILE_DEVICE_DFS_FILE_SYSTEM		0x00000035
#define FILE_DEVICE_DFS_VOLUME			0x00000036
#define FILE_DEVICE_SERENUM			0x00000037
#define FILE_DEVICE_TERMSRV			0x00000038
#define FILE_DEVICE_KSEC			0x00000039
#define FILE_DEVICE_FIPS			0x0000003A
#define FILE_DEVICE_INFINIBAND			0x0000003B
#define FILE_DEVICE_VMBUS			0x0000003E
#define FILE_DEVICE_CRYPT_PROVIDER		0x0000003F
#define FILE_DEVICE_WPD				0x00000040
#define FILE_DEVICE_BLUETOOTH			0x00000041
#define FILE_DEVICE_MT_COMPOSITE		0x00000042
#define FILE_DEVICE_MT_TRANSPORT		0x00000043
#define FILE_DEVICE_BIOMETRIC			0x00000044
#define FILE_DEVICE_PMI				0x00000045

#define CTL_CODE(DeviceType, Function, Method, Access) \
	(((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

#define DEVICE_TYPE_FROM_CTL_CODE(ctrlCode)	(((DWORD)(ctrlCode & 0xFFFF0000)) >> 16)

#define METHOD_FROM_CTL_CODE(ctrlCode)		((DWORD)(ctrlCode & 3))

#define METHOD_BUFFERED				0
#define METHOD_IN_DIRECT			1
#define METHOD_OUT_DIRECT			2
#define METHOD_NEITHER				3

#define FILE_ANY_ACCESS				0
#define FILE_SPECIAL_ACCESS			(FILE_ANY_ACCESS)
#define FILE_READ_ACCESS			(0x0001)
#define FILE_WRITE_ACCESS			(0x0002)

/*
 * WinPR I/O Manager Custom API
 */

typedef HANDLE PDRIVER_OBJECT_EX;
typedef HANDLE PDEVICE_OBJECT_EX;

WINPR_API NTSTATUS _IoCreateDeviceEx(PDRIVER_OBJECT_EX DriverObject, ULONG DeviceExtensionSize, PUNICODE_STRING DeviceName,
		DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics, BOOLEAN Exclusive, PDEVICE_OBJECT_EX* DeviceObject);

WINPR_API VOID _IoDeleteDeviceEx(PDEVICE_OBJECT_EX DeviceObject);

#endif

/**
 * Extended API
 */

#define ACCESS_FROM_CTL_CODE(ctrlCode)		((DWORD)((ctrlCode >> 14) & 0x3))
#define FUNCTION_FROM_CTL_CODE(ctrlCode)	((DWORD)((ctrlCode >> 2) & 0xFFF))

#endif /* WINPR_IO_H */

