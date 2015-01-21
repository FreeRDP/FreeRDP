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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/io.h>

#ifndef _WIN32

#include "io.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>

/**
 * I/O Manager Routines
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551797/
 *
 * These routines are only accessible to kernel drivers, but we need
 * similar functionality in WinPR in user space.
 *
 * This is a best effort non-conflicting port of this API meant for
 * non-Windows, WinPR usage only.
 *
 * References:
 *
 * Device Objects and Device Stacks:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff543153/
 *
 * Driver Development Part 1: Introduction to Drivers:
 * http://www.codeproject.com/Articles/9504/Driver-Development-Part-1-Introduction-to-Drivers/
 */

#define DEVICE_FILE_PREFIX_PATH		"\\Device\\"

char* GetDeviceFileNameWithoutPrefixA(LPCSTR lpName)
{
	char* lpFileName;

	if (!lpName)
		return NULL;

	if (strncmp(lpName, DEVICE_FILE_PREFIX_PATH, sizeof(DEVICE_FILE_PREFIX_PATH) - 1) != 0)
		return NULL;

	lpFileName = _strdup(&lpName[strlen(DEVICE_FILE_PREFIX_PATH)]);

	return lpFileName;
}

char* GetDeviceFileUnixDomainSocketBaseFilePathA()
{
	char* lpTempPath;
	char* lpPipePath;

	lpTempPath = GetKnownPath(KNOWN_PATH_TEMP);
	lpPipePath = GetCombinedPath(lpTempPath, ".device");

	free(lpTempPath);

	return lpPipePath;
}

char* GetDeviceFileUnixDomainSocketFilePathA(LPCSTR lpName)
{
	char* lpPipePath;
	char* lpFileName;
	char* lpFilePath;

	lpPipePath = GetDeviceFileUnixDomainSocketBaseFilePathA();

	lpFileName = GetDeviceFileNameWithoutPrefixA(lpName);
	lpFilePath = GetCombinedPath(lpPipePath, (char*) lpFileName);

	free(lpPipePath);
	free(lpFileName);

	return lpFilePath;
}

/**
 * IoCreateDevice:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff548397/
 */

NTSTATUS _IoCreateDeviceEx(PDRIVER_OBJECT_EX DriverObject, ULONG DeviceExtensionSize, PUNICODE_STRING DeviceName,
		DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics, BOOLEAN Exclusive, PDEVICE_OBJECT_EX* DeviceObject)
{
	int status;
	char* DeviceBasePath;
	DEVICE_OBJECT_EX* pDeviceObjectEx;

	DeviceBasePath = GetDeviceFileUnixDomainSocketBaseFilePathA();

	(void)DriverObject;
	(void)DeviceExtensionSize;
	(void)DeviceType;
	(void)DeviceCharacteristics;
	(void)Exclusive;

	if (!PathFileExistsA(DeviceBasePath))
	{
		if (!mkdir(DeviceBasePath, S_IRUSR | S_IWUSR | S_IXUSR))
		{
			free(DeviceBasePath);
			return STATUS_ACCESS_DENIED;
		}
	}

	pDeviceObjectEx = (DEVICE_OBJECT_EX*) malloc(sizeof(DEVICE_OBJECT_EX));

	if (!pDeviceObjectEx)
	{
		return STATUS_NO_MEMORY;
	}

	ZeroMemory(pDeviceObjectEx, sizeof(DEVICE_OBJECT_EX));

	ConvertFromUnicode(CP_UTF8, 0, DeviceName->Buffer, DeviceName->Length / 2, &(pDeviceObjectEx->DeviceName), 0, NULL, NULL);

	pDeviceObjectEx->DeviceFileName = GetDeviceFileUnixDomainSocketFilePathA(pDeviceObjectEx->DeviceName);

	if (PathFileExistsA(pDeviceObjectEx->DeviceFileName))
	{
		unlink(pDeviceObjectEx->DeviceFileName);
	}

	status = mkfifo(pDeviceObjectEx->DeviceFileName, 0666);
	if (status)
		return STATUS_ACCESS_DENIED;

	*((ULONG_PTR*) (DeviceObject)) = (ULONG_PTR) pDeviceObjectEx;

	return STATUS_SUCCESS;
}

/**
 * IoDeleteDevice:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff549083/
 */

VOID _IoDeleteDeviceEx(PDEVICE_OBJECT_EX DeviceObject)
{
	DEVICE_OBJECT_EX* pDeviceObjectEx;

	pDeviceObjectEx = (DEVICE_OBJECT_EX*) DeviceObject;

	if (!pDeviceObjectEx)
		return;

	unlink(pDeviceObjectEx->DeviceFileName);

	free(pDeviceObjectEx->DeviceName);
	free(pDeviceObjectEx->DeviceFileName);

	free(pDeviceObjectEx);
}

/**
 * IoCreateSymbolicLink:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff549043/
 */

NTSTATUS _IoCreateSymbolicLinkEx(PUNICODE_STRING SymbolicLinkName, PUNICODE_STRING DeviceName)
{
	return STATUS_SUCCESS;
}

/**
 * IoDeleteSymbolicLink:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff549085/
 */

NTSTATUS _IoDeleteSymbolicLinkEx(PUNICODE_STRING SymbolicLinkName)
{
	return STATUS_SUCCESS;
}

#endif
