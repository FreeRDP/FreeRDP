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
#include <errno.h>

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
	if (!lpTempPath)
		return NULL;
	lpPipePath = GetCombinedPath(lpTempPath, ".device");

	free(lpTempPath);

	return lpPipePath;
}

char* GetDeviceFileUnixDomainSocketFilePathA(LPCSTR lpName)
{
	char* lpPipePath = NULL;
	char* lpFileName = NULL;
	char* lpFilePath = NULL;

	lpPipePath = GetDeviceFileUnixDomainSocketBaseFilePathA();
	if (!lpPipePath)
		return NULL;

	lpFileName = GetDeviceFileNameWithoutPrefixA(lpName);
	if (!lpFileName)
	{
		free(lpFilePath);
		return NULL;
	}

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

	if (!DeviceBasePath)
		return STATUS_NO_MEMORY;

	if (!PathFileExistsA(DeviceBasePath))
	{
		if (mkdir(DeviceBasePath, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
		{
			free(DeviceBasePath);
			return STATUS_ACCESS_DENIED;
		}
	}
	free(DeviceBasePath);

	pDeviceObjectEx = (DEVICE_OBJECT_EX*) calloc(1, sizeof(DEVICE_OBJECT_EX));

	if (!pDeviceObjectEx)
		return STATUS_NO_MEMORY;

	ConvertFromUnicode(CP_UTF8, 0, DeviceName->Buffer, DeviceName->Length / 2, &(pDeviceObjectEx->DeviceName), 0, NULL, NULL);
	if (!pDeviceObjectEx->DeviceName)
	{
		free(pDeviceObjectEx);
		return STATUS_NO_MEMORY;
	}

	pDeviceObjectEx->DeviceFileName = GetDeviceFileUnixDomainSocketFilePathA(pDeviceObjectEx->DeviceName);
	if (!pDeviceObjectEx->DeviceFileName)
	{
		free(pDeviceObjectEx->DeviceName);
		free(pDeviceObjectEx);
		return STATUS_NO_MEMORY;
	}

	if (PathFileExistsA(pDeviceObjectEx->DeviceFileName))
	{
		if (unlink(pDeviceObjectEx->DeviceFileName) == -1)
		{
			free(pDeviceObjectEx->DeviceName);
			free(pDeviceObjectEx->DeviceFileName);
			free(pDeviceObjectEx);
			return STATUS_ACCESS_DENIED;
		}

	}

	status = mkfifo(pDeviceObjectEx->DeviceFileName, 0666);
	if (status != 0)
	{
		free(pDeviceObjectEx->DeviceName);
		free(pDeviceObjectEx->DeviceFileName);
		free(pDeviceObjectEx);
		switch (errno)
		{
			case EACCES:
				return STATUS_ACCESS_DENIED;
			case EEXIST:
				return STATUS_OBJECT_NAME_EXISTS;
			case ENAMETOOLONG:
				return STATUS_NAME_TOO_LONG;
			case ENOENT:
			case ENOTDIR:
				return STATUS_NOT_A_DIRECTORY;
			case ENOSPC:
				return STATUS_DISK_FULL;
			default:
				return STATUS_INTERNAL_ERROR;
		}
	}

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
