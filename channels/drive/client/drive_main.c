/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * File System Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef _WIN32
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <sys/time.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/interlocked.h>
#include <winpr/collections.h>

#include <freerdp/channels/rdpdr.h>

#include "drive_file.h"

typedef struct _DRIVE_DEVICE DRIVE_DEVICE;

struct _DRIVE_DEVICE
{
	DEVICE device;

	char* path;
	wListDictionary* files;

	HANDLE thread;
	wMessageQueue* IrpQueue;

	DEVMAN* devman;
};

static UINT32 drive_map_posix_err(int fs_errno)
{
	UINT32 rc;

	/* try to return NTSTATUS version of error code */

	switch (fs_errno)
	{
		case EPERM:
		case EACCES:
			rc = STATUS_ACCESS_DENIED;
			break;
		case ENOENT:
			rc = STATUS_NO_SUCH_FILE;
			break;
		case EBUSY:
			rc = STATUS_DEVICE_BUSY;
			break;
		case EEXIST:
			rc  = STATUS_OBJECT_NAME_COLLISION;
			break;
		case EISDIR:
			rc = STATUS_FILE_IS_A_DIRECTORY;
			break;

		default:
			rc = STATUS_UNSUCCESSFUL;
			break;
	}

	return rc;
}

static DRIVE_FILE* drive_get_file_by_id(DRIVE_DEVICE* drive, UINT32 id)
{
	DRIVE_FILE* file = NULL;
	void* key = (void*) (size_t) id;

	file = (DRIVE_FILE*) ListDictionary_GetItemValue(drive->files, key);

	return file;
}

static void drive_process_irp_create(DRIVE_DEVICE* drive, IRP* irp)
{
	int status;
	void* key;
	UINT32 FileId;
	DRIVE_FILE* file;
	BYTE Information;
	UINT32 DesiredAccess;
	UINT32 CreateDisposition;
	UINT32 CreateOptions;
	UINT32 PathLength;
	char* path = NULL;

	Stream_Read_UINT32(irp->input, DesiredAccess);
	Stream_Seek(irp->input, 16); /* AllocationSize(8), FileAttributes(4), SharedAccess(4) */
	Stream_Read_UINT32(irp->input, CreateDisposition);
	Stream_Read_UINT32(irp->input, CreateOptions);
	Stream_Read_UINT32(irp->input, PathLength);

	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(irp->input),
			PathLength / 2, &path, 0, NULL, NULL);

	if (status < 1)
		path = (char*) calloc(1, 1);

	FileId = irp->devman->id_sequence++;

	file = drive_file_new(drive->path, path, FileId,
		DesiredAccess, CreateDisposition, CreateOptions);

	if (!file)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		FileId = 0;
		Information = 0;
	}
	else if (file->err)
	{
		FileId = 0;
		Information = 0;

		/* map errno to windows result */
		irp->IoStatus = drive_map_posix_err(file->err);
		drive_file_free(file);
	}
	else
	{
		key = (void*) (size_t) file->id;
		ListDictionary_Add(drive->files, key, file);

		switch (CreateDisposition)
		{
			case FILE_SUPERSEDE:
			case FILE_OPEN:
			case FILE_CREATE:
			case FILE_OVERWRITE:
				Information = FILE_SUPERSEDED;
				break;
			case FILE_OPEN_IF:
				Information = FILE_OPENED;
				break;
			case FILE_OVERWRITE_IF:
				Information = FILE_OVERWRITTEN;
				break;
			default:
				Information = 0;
				break;
		}
	}

	Stream_Write_UINT32(irp->output, FileId);
	Stream_Write_UINT8(irp->output, Information);

	free(path);

	irp->Complete(irp);
}

static void drive_process_irp_close(DRIVE_DEVICE* drive, IRP* irp)
{
	void* key;
	DRIVE_FILE* file;

	file = drive_get_file_by_id(drive, irp->FileId);

	key = (void*) (size_t) irp->FileId;

	if (!file)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
	}
	else
	{
		ListDictionary_Remove(drive->files, key);
		drive_file_free(file);
	}

	Stream_Zero(irp->output, 5); /* Padding(5) */

	irp->Complete(irp);
}

static void drive_process_irp_read(DRIVE_DEVICE* drive, IRP* irp)
{
	DRIVE_FILE* file;
	UINT32 Length;
	UINT64 Offset;
	BYTE* buffer = NULL;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);

	file = drive_get_file_by_id(drive, irp->FileId);

	if (!file)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;
	}
	else if (!drive_file_seek(file, Offset))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;
	}
	else
	{
		buffer = (BYTE*) malloc(Length);

		if (!drive_file_read(file, buffer, &Length))
		{
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			free(buffer);
			buffer = NULL;
			Length = 0;
		}
		else
		{

		}
	}

	Stream_Write_UINT32(irp->output, Length);

	if (Length > 0)
	{
		Stream_EnsureRemainingCapacity(irp->output, (int) Length);
		Stream_Write(irp->output, buffer, Length);
	}

	free(buffer);

	irp->Complete(irp);
}

static void drive_process_irp_write(DRIVE_DEVICE* drive, IRP* irp)
{
	DRIVE_FILE* file;
	UINT32 Length;
	UINT64 Offset;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	Stream_Seek(irp->input, 20); /* Padding */

	file = drive_get_file_by_id(drive, irp->FileId);

	if (!file)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;
	}
	else if (!drive_file_seek(file, Offset))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;
	}
	else if (!drive_file_write(file, Stream_Pointer(irp->input), Length))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;
	}
	else
	{

	}

	Stream_Write_UINT32(irp->output, Length);
	Stream_Write_UINT8(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void drive_process_irp_query_information(DRIVE_DEVICE* drive, IRP* irp)
{
	DRIVE_FILE* file;
	UINT32 FsInformationClass;

	Stream_Read_UINT32(irp->input, FsInformationClass);

	file = drive_get_file_by_id(drive, irp->FileId);

	if (!file)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
	}
	else if (!drive_file_query_information(file, FsInformationClass, irp->output))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
	}
	else
	{

	}

	irp->Complete(irp);
}

static void drive_process_irp_set_information(DRIVE_DEVICE* drive, IRP* irp)
{
	DRIVE_FILE* file;
	UINT32 FsInformationClass;
	UINT32 Length;

	Stream_Read_UINT32(irp->input, FsInformationClass);
	Stream_Read_UINT32(irp->input, Length);
	Stream_Seek(irp->input, 24); /* Padding */

	file = drive_get_file_by_id(drive, irp->FileId);

	if (!file)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
	}
	else if (!drive_file_set_information(file, FsInformationClass, Length, irp->input))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
	}
	else
	{

	}

	Stream_Write_UINT32(irp->output, Length);

	irp->Complete(irp);
}

static void drive_process_irp_query_volume_information(DRIVE_DEVICE* drive, IRP* irp)
{
	UINT32 FsInformationClass;
	wStream* output = irp->output;
	struct STATVFS svfst;
	struct STAT st;
	char* volumeLabel = {"FREERDP"};
	char* diskType = {"FAT32"};
	WCHAR* outStr = NULL;
	int length;

	Stream_Read_UINT32(irp->input, FsInformationClass);

	STATVFS(drive->path, &svfst);
	STAT(drive->path, &st);

	switch (FsInformationClass)
	{
		case FileFsVolumeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232108.aspx */
			length = ConvertToUnicode(sys_code_page, 0, volumeLabel, -1, &outStr, 0) * 2;
			Stream_Write_UINT32(output, 17 + length); /* Length */
			Stream_EnsureRemainingCapacity(output, 17 + length);
			Stream_Write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* VolumeCreationTime */
#ifdef ANDROID
			Stream_Write_UINT32(output, svfst.f_fsid.__val[0]); /* VolumeSerialNumber */
#else
			Stream_Write_UINT32(output, svfst.f_fsid); /* VolumeSerialNumber */
#endif
			Stream_Write_UINT32(output, length); /* VolumeLabelLength */
			Stream_Write_UINT8(output, 0); /* SupportsObjects */
			/* Reserved(1), MUST NOT be added! */
			Stream_Write(output, outStr, length); /* VolumeLabel (Unicode) */
			free(outStr);
			break;

		case FileFsSizeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232107.aspx */
			Stream_Write_UINT32(output, 24); /* Length */
			Stream_EnsureRemainingCapacity(output, 24);
			Stream_Write_UINT64(output, svfst.f_blocks); /* TotalAllocationUnits */
			Stream_Write_UINT64(output, svfst.f_bavail); /* AvailableAllocationUnits */
			Stream_Write_UINT32(output, 1); /* SectorsPerAllocationUnit */
			Stream_Write_UINT32(output, svfst.f_bsize); /* BytesPerSector */
			break;

		case FileFsAttributeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232101.aspx */
			length = ConvertToUnicode(sys_code_page, 0, diskType, -1, &outStr, 0) * 2;
			Stream_Write_UINT32(output, 12 + length); /* Length */
			Stream_EnsureRemainingCapacity(output, 12 + length);
			Stream_Write_UINT32(output,
				FILE_CASE_SENSITIVE_SEARCH |
				FILE_CASE_PRESERVED_NAMES |
				FILE_UNICODE_ON_DISK); /* FileSystemAttributes */
#ifdef ANDROID
			Stream_Write_UINT32(output, 255); /* MaximumComponentNameLength */
#else
			Stream_Write_UINT32(output, svfst.f_namemax/*510*/); /* MaximumComponentNameLength */
#endif
			Stream_Write_UINT32(output, length); /* FileSystemNameLength */
			Stream_Write(output, outStr, length); /* FileSystemName (Unicode) */
			free(outStr);
			break;

		case FileFsFullSizeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232104.aspx */
			Stream_Write_UINT32(output, 32); /* Length */
			Stream_EnsureRemainingCapacity(output, 32);
			Stream_Write_UINT64(output, svfst.f_blocks); /* TotalAllocationUnits */
			Stream_Write_UINT64(output, svfst.f_bavail); /* CallerAvailableAllocationUnits */
			Stream_Write_UINT64(output, svfst.f_bfree); /* AvailableAllocationUnits */
			Stream_Write_UINT32(output, 1); /* SectorsPerAllocationUnit */
			Stream_Write_UINT32(output, svfst.f_bsize); /* BytesPerSector */
			break;

		case FileFsDeviceInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232109.aspx */
			Stream_Write_UINT32(output, 8); /* Length */
			Stream_EnsureRemainingCapacity(output, 8);
			Stream_Write_UINT32(output, FILE_DEVICE_DISK); /* DeviceType */
			Stream_Write_UINT32(output, 0); /* Characteristics */
			break;

		default:
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			Stream_Write_UINT32(output, 0); /* Length */
			break;
	}

	irp->Complete(irp);
}

/* http://msdn.microsoft.com/en-us/library/cc241518.aspx */

static void drive_process_irp_silent_ignore(DRIVE_DEVICE* drive, IRP* irp)
{
	UINT32 FsInformationClass;
	wStream* output = irp->output;

	Stream_Read_UINT32(irp->input, FsInformationClass);

	Stream_Write_UINT32(output, 0); /* Length */

	irp->Complete(irp);
}

static void drive_process_irp_query_directory(DRIVE_DEVICE* drive, IRP* irp)
{
	char* path = NULL;
	int status;
	DRIVE_FILE* file;
	BYTE InitialQuery;
	UINT32 PathLength;
	UINT32 FsInformationClass;

	Stream_Read_UINT32(irp->input, FsInformationClass);
	Stream_Read_UINT8(irp->input, InitialQuery);
	Stream_Read_UINT32(irp->input, PathLength);
	Stream_Seek(irp->input, 23); /* Padding */

	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(irp->input),
			PathLength / 2, &path, 0, NULL, NULL);

	if (status < 1)
		path = (char*) calloc(1, 1);

	file = drive_get_file_by_id(drive, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Stream_Write_UINT32(irp->output, 0); /* Length */
	}
	else if (!drive_file_query_directory(file, FsInformationClass, InitialQuery, path, irp->output))
	{
		irp->IoStatus = STATUS_NO_MORE_FILES;
	}

	free(path);

	irp->Complete(irp);
}

static void drive_process_irp_directory_control(DRIVE_DEVICE* drive, IRP* irp)
{
	switch (irp->MinorFunction)
	{
		case IRP_MN_QUERY_DIRECTORY:
			drive_process_irp_query_directory(drive, irp);
			break;

		case IRP_MN_NOTIFY_CHANGE_DIRECTORY: /* TODO */
			irp->Discard(irp);
			break;

		default:
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			Stream_Write_UINT32(irp->output, 0); /* Length */
			irp->Complete(irp);
			break;
	}
}

static void drive_process_irp_device_control(DRIVE_DEVICE* drive, IRP* irp)
{
	Stream_Write_UINT32(irp->output, 0); /* OutputBufferLength */
	irp->Complete(irp);
}

static void drive_process_irp(DRIVE_DEVICE* drive, IRP* irp)
{
	irp->IoStatus = STATUS_SUCCESS;

	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			drive_process_irp_create(drive, irp);
			break;

		case IRP_MJ_CLOSE:
			drive_process_irp_close(drive, irp);
			break;

		case IRP_MJ_READ:
			drive_process_irp_read(drive, irp);
			break;

		case IRP_MJ_WRITE:
			drive_process_irp_write(drive, irp);
			break;

		case IRP_MJ_QUERY_INFORMATION:
			drive_process_irp_query_information(drive, irp);
			break;

		case IRP_MJ_SET_INFORMATION:
			drive_process_irp_set_information(drive, irp);
			break;

		case IRP_MJ_QUERY_VOLUME_INFORMATION:
			drive_process_irp_query_volume_information(drive, irp);
			break;

		case IRP_MJ_LOCK_CONTROL:
			drive_process_irp_silent_ignore(drive, irp);
			break;

		case IRP_MJ_DIRECTORY_CONTROL:
			drive_process_irp_directory_control(drive, irp);
			break;

		case IRP_MJ_DEVICE_CONTROL:
			drive_process_irp_device_control(drive, irp);
			break;

		default:
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void* drive_thread_func(void* arg)
{
	IRP* irp;
	wMessage message;
	DRIVE_DEVICE* drive = (DRIVE_DEVICE*) arg;

	while (1)
	{
		if (!MessageQueue_Wait(drive->IrpQueue))
			break;

		if (!MessageQueue_Peek(drive->IrpQueue, &message, TRUE))
			break;

		if (message.id == WMQ_QUIT)
			break;

		irp = (IRP*) message.wParam;

		if (irp)
			drive_process_irp(drive, irp);
	}

	ExitThread(0);
	return NULL;
}

static void drive_irp_request(DEVICE* device, IRP* irp)
{
	DRIVE_DEVICE* drive = (DRIVE_DEVICE*) device;
	MessageQueue_Post(drive->IrpQueue, NULL, 0, (void*) irp, NULL);
}

static void drive_free(DEVICE* device)
{
	DRIVE_DEVICE* drive = (DRIVE_DEVICE*) device;

	MessageQueue_PostQuit(drive->IrpQueue, 0);
	WaitForSingleObject(drive->thread, INFINITE);

	CloseHandle(drive->thread);

	ListDictionary_Free(drive->files);

	free(drive);
}

void drive_register_drive_path(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints, char* name, char* path)
{
	int i, length;
	DRIVE_DEVICE* drive;

#ifdef WIN32
	/*
	 * We cannot enter paths like c:\ because : is an arg separator
	 * thus, paths are entered as c+\ and the + is substituted here
	 */
	if (path[1] == '+')
	{
		if ((path[0]>='a' && path[0]<='z') || (path[0]>='A' && path[0]<='Z'))
		{
			path[1] = ':';
		}
	}
#endif

	if (name[0] && path[0])
	{
		drive = (DRIVE_DEVICE*) malloc(sizeof(DRIVE_DEVICE));
		ZeroMemory(drive, sizeof(DRIVE_DEVICE));

		drive->device.type = RDPDR_DTYP_FILESYSTEM;
		drive->device.name = name;
		drive->device.IRPRequest = drive_irp_request;
		drive->device.Free = drive_free;

		length = (int) strlen(name);
		drive->device.data = Stream_New(NULL, length + 1);

		for (i = 0; i <= length; i++)
			Stream_Write_UINT8(drive->device.data, name[i] < 0 ? '_' : name[i]);

		drive->path = path;

		drive->files = ListDictionary_New(TRUE);
		ListDictionary_ValueObject(drive->files)->fnObjectFree = (OBJECT_FREE_FN) drive_file_free;

		drive->IrpQueue = MessageQueue_New(NULL);
		drive->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) drive_thread_func, drive, CREATE_SUSPENDED, NULL);

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) drive);

		ResumeThread(drive->thread);
	}
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	drive_DeviceServiceEntry
#endif

UINT sys_code_page = 0;

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	RDPDR_DRIVE* drive;
#ifdef WIN32
	char* dev;
	int len;
	char devlist[512], buf[512];
#endif

	drive = (RDPDR_DRIVE*) pEntryPoints->device;

#ifndef WIN32

	sys_code_page = CP_UTF8;
	if (strcmp(drive->Path, "*") == 0)
	{
		/* all drives */

		free(drive->Path);
		drive->Path = _strdup("/");
	}
	else if (strcmp(drive->Path, "%") == 0)
	{
		char* home_env = NULL;

		/* home directory */

		home_env = getenv("HOME");
		free(drive->Path);

		if (home_env)
			drive->Path = _strdup(home_env);
		else
			drive->Path = _strdup("/");
	}

	drive_register_drive_path(pEntryPoints, drive->Name, drive->Path);

#else
	sys_code_page = GetACP();
	/* Special case: path[0] == '*' -> export all drives */
	/* Special case: path[0] == '%' -> user home dir */
	if (strcmp(drive->Path, "%") == 0)
	{
		_snprintf(buf, sizeof(buf), "%s\\", getenv("USERPROFILE"));
		drive_register_drive_path(pEntryPoints, drive->Name, _strdup(buf));
	}
	else if (strcmp(drive->Path, "*") == 0)
	{
		int i;

		/* Enumerate all devices: */
		GetLogicalDriveStringsA(sizeof(devlist) - 1, devlist);

		for (dev = devlist, i = 0; *dev; dev += 4, i++)
		{
			if (*dev > 'B')
			{
				/* Suppress disk drives A and B to avoid pesty messages */
				len = _snprintf(buf, sizeof(buf) - 4, "%s", drive->Name);
				buf[len] = '_';
				buf[len + 1] = dev[0];
				buf[len + 2] = 0;
				buf[len + 3] = 0;
				drive_register_drive_path(pEntryPoints, _strdup(buf), _strdup(dev));
			}
		}
	}
	else
	{
		drive_register_drive_path(pEntryPoints, drive->Name, drive->Path);
	}
#endif

	return 0;
}
