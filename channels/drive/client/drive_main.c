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
#include <winpr/interlocked.h>

#include <freerdp/utils/list.h>
#include <winpr/stream.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/utils/svc_plugin.h>

#include "drive_file.h"

typedef struct _DRIVE_DEVICE DRIVE_DEVICE;

struct _DRIVE_DEVICE
{
	DEVICE device;

	char* path;
	LIST* files;

	HANDLE thread;
	HANDLE irpEvent;
	HANDLE stopEvent;
	PSLIST_HEADER pIrpList;

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

	DEBUG_SVC("errno 0x%x mapped to 0x%x", fs_errno, rc);

	return rc;
}

static DRIVE_FILE* drive_get_file_by_id(DRIVE_DEVICE* disk, UINT32 id)
{
	LIST_ITEM* item;
	DRIVE_FILE* file;

	for (item = disk->files->head; item; item = item->next)
	{
		file = (DRIVE_FILE*) item->data;

		if (file->id == id)
			return file;
	}

	return NULL;
}

static void drive_process_irp_create(DRIVE_DEVICE* disk, IRP* irp)
{
	char* path;
	int status;
	UINT32 FileId;
	DRIVE_FILE* file;
	BYTE Information;
	UINT32 DesiredAccess;
	UINT32 CreateDisposition;
	UINT32 CreateOptions;
	UINT32 PathLength;

	stream_read_UINT32(irp->input, DesiredAccess);
	stream_seek(irp->input, 16); /* AllocationSize(8), FileAttributes(4), SharedAccess(4) */
	stream_read_UINT32(irp->input, CreateDisposition);
	stream_read_UINT32(irp->input, CreateOptions);
	stream_read_UINT32(irp->input, PathLength);

	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) stream_get_tail(irp->input),
			PathLength / 2, &path, 0, NULL, NULL);

	if (status < 1)
		path = (char*) calloc(1, 1);

	FileId = irp->devman->id_sequence++;

	file = drive_file_new(disk->path, path, FileId,
		DesiredAccess, CreateDisposition, CreateOptions);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		FileId = 0;
		Information = 0;

		DEBUG_WARN("failed to create %s.", path);
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
		list_enqueue(disk->files, file);

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
		DEBUG_SVC("%s(%d) created.", file->fullpath, file->id);
	}

	stream_write_UINT32(irp->output, FileId);
	stream_write_BYTE(irp->output, Information);

	free(path);

	irp->Complete(irp);
}

static void drive_process_irp_close(DRIVE_DEVICE* disk, IRP* irp)
{
	DRIVE_FILE* file;

	file = drive_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else
	{
		DEBUG_SVC("%s(%d) closed.", file->fullpath, file->id);

		list_remove(disk->files, file);
		drive_file_free(file);
	}

	stream_write_zero(irp->output, 5); /* Padding(5) */

	irp->Complete(irp);
}

static void drive_process_irp_read(DRIVE_DEVICE* disk, IRP* irp)
{
	DRIVE_FILE* file;
	UINT32 Length;
	UINT64 Offset;
	BYTE* buffer = NULL;

	stream_read_UINT32(irp->input, Length);
	stream_read_UINT64(irp->input, Offset);

	file = drive_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!drive_file_seek(file, Offset))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("seek %s(%d) failed.", file->fullpath, file->id);
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

			DEBUG_WARN("read %s(%d) failed.", file->fullpath, file->id);
		}
		else
		{
			DEBUG_SVC("read %llu-%llu from %s(%d).", Offset, Offset + Length, file->fullpath, file->id);
		}
	}

	stream_write_UINT32(irp->output, Length);

	if (Length > 0)
	{
		stream_check_size(irp->output, (int) Length);
		stream_write(irp->output, buffer, Length);
	}

	free(buffer);

	irp->Complete(irp);
}

static void drive_process_irp_write(DRIVE_DEVICE* disk, IRP* irp)
{
	DRIVE_FILE* file;
	UINT32 Length;
	UINT64 Offset;

	stream_read_UINT32(irp->input, Length);
	stream_read_UINT64(irp->input, Offset);
	stream_seek(irp->input, 20); /* Padding */

	file = drive_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!drive_file_seek(file, Offset))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("seek %s(%d) failed.", file->fullpath, file->id);
	}
	else if (!drive_file_write(file, stream_get_tail(irp->input), Length))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("write %s(%d) failed.", file->fullpath, file->id);
	}
	else
	{
		DEBUG_SVC("write %llu-%llu to %s(%d).", Offset, Offset + Length, file->fullpath, file->id);
	}

	stream_write_UINT32(irp->output, Length);
	stream_write_BYTE(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void drive_process_irp_query_information(DRIVE_DEVICE* disk, IRP* irp)
{
	DRIVE_FILE* file;
	UINT32 FsInformationClass;

	stream_read_UINT32(irp->input, FsInformationClass);

	file = drive_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!drive_file_query_information(file, FsInformationClass, irp->output))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FsInformationClass %d on %s(%d) failed.", FsInformationClass, file->fullpath, file->id);
	}
	else
	{
		DEBUG_SVC("FsInformationClass %d on %s(%d).", FsInformationClass, file->fullpath, file->id);
	}

	irp->Complete(irp);
}

static void drive_process_irp_set_information(DRIVE_DEVICE* disk, IRP* irp)
{
	DRIVE_FILE* file;
	UINT32 FsInformationClass;
	UINT32 Length;

	stream_read_UINT32(irp->input, FsInformationClass);
	stream_read_UINT32(irp->input, Length);
	stream_seek(irp->input, 24); /* Padding */

	file = drive_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!drive_file_set_information(file, FsInformationClass, Length, irp->input))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FsInformationClass %d on %s(%d) failed.", FsInformationClass, file->fullpath, file->id);
	}
	else
	{
		DEBUG_SVC("FsInformationClass %d on %s(%d) ok.", FsInformationClass, file->fullpath, file->id);
	}

	stream_write_UINT32(irp->output, Length);

	irp->Complete(irp);
}

static void drive_process_irp_query_volume_information(DRIVE_DEVICE* disk, IRP* irp)
{
	UINT32 FsInformationClass;
	wStream* output = irp->output;
	struct STATVFS svfst;
	struct STAT st;
	char* volumeLabel = {"FREERDP"};
	char* diskType = {"FAT32"};
	WCHAR* outStr = NULL;
	int length;

	stream_read_UINT32(irp->input, FsInformationClass);

	STATVFS(disk->path, &svfst);
	STAT(disk->path, &st);

	switch (FsInformationClass)
	{
		case FileFsVolumeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232108.aspx */
			length = ConvertToUnicode(CP_UTF8, 0, volumeLabel, -1, &outStr, 0) * 2;
			stream_write_UINT32(output, 17 + length); /* Length */
			stream_check_size(output, 17 + length);
			stream_write_UINT64(output, FILE_TIME_SYSTEM_TO_RDP(st.st_ctime)); /* VolumeCreationTime */
			stream_write_UINT32(output, svfst.f_fsid); /* VolumeSerialNumber */
			stream_write_UINT32(output, length); /* VolumeLabelLength */
			stream_write_BYTE(output, 0); /* SupportsObjects */
			/* Reserved(1), MUST NOT be added! */
			stream_write(output, outStr, length); /* VolumeLabel (Unicode) */
			free(outStr);
			break;

		case FileFsSizeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232107.aspx */
			stream_write_UINT32(output, 24); /* Length */
			stream_check_size(output, 24);
			stream_write_UINT64(output, svfst.f_blocks); /* TotalAllocationUnits */
			stream_write_UINT64(output, svfst.f_bavail); /* AvailableAllocationUnits */
			stream_write_UINT32(output, 1); /* SectorsPerAllocationUnit */
			stream_write_UINT32(output, svfst.f_bsize); /* BytesPerSector */
			break;

		case FileFsAttributeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232101.aspx */
			length = ConvertToUnicode(CP_UTF8, 0, diskType, -1, &outStr, 0) * 2;
			stream_write_UINT32(output, 12 + length); /* Length */
			stream_check_size(output, 12 + length);
			stream_write_UINT32(output,
				FILE_CASE_SENSITIVE_SEARCH |
				FILE_CASE_PRESERVED_NAMES |
				FILE_UNICODE_ON_DISK); /* FileSystemAttributes */
			stream_write_UINT32(output, svfst.f_namemax/*510*/); /* MaximumComponentNameLength */
			stream_write_UINT32(output, length); /* FileSystemNameLength */
			stream_write(output, outStr, length); /* FileSystemName (Unicode) */
			free(outStr);
			break;

		case FileFsFullSizeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232104.aspx */
			stream_write_UINT32(output, 32); /* Length */
			stream_check_size(output, 32);
			stream_write_UINT64(output, svfst.f_blocks); /* TotalAllocationUnits */
			stream_write_UINT64(output, svfst.f_bavail); /* CallerAvailableAllocationUnits */
			stream_write_UINT64(output, svfst.f_bfree); /* AvailableAllocationUnits */
			stream_write_UINT32(output, 1); /* SectorsPerAllocationUnit */
			stream_write_UINT32(output, svfst.f_bsize); /* BytesPerSector */
			break;

		case FileFsDeviceInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232109.aspx */
			stream_write_UINT32(output, 8); /* Length */
			stream_check_size(output, 8);
			stream_write_UINT32(output, FILE_DEVICE_DISK); /* DeviceType */
			stream_write_UINT32(output, 0); /* Characteristics */
			break;

		default:
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			stream_write_UINT32(output, 0); /* Length */
			DEBUG_WARN("invalid FsInformationClass %d", FsInformationClass);
			break;
	}

	irp->Complete(irp);
}

/* http://msdn.microsoft.com/en-us/library/cc241518.aspx */

static void drive_process_irp_silent_ignore(DRIVE_DEVICE* disk, IRP* irp)
{
	UINT32 FsInformationClass;
	wStream* output = irp->output;

	stream_read_UINT32(irp->input, FsInformationClass);

	DEBUG_SVC("FsInformationClass %d in drive_process_irp_silent_ignore", FsInformationClass);
	stream_write_UINT32(output, 0); /* Length */

	irp->Complete(irp);
}

static void drive_process_irp_query_directory(DRIVE_DEVICE* disk, IRP* irp)
{
	char* path = NULL;
	int status;
	DRIVE_FILE* file;
	BYTE InitialQuery;
	UINT32 PathLength;
	UINT32 FsInformationClass;

	stream_read_UINT32(irp->input, FsInformationClass);
	stream_read_BYTE(irp->input, InitialQuery);
	stream_read_UINT32(irp->input, PathLength);
	stream_seek(irp->input, 23); /* Padding */

	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) stream_get_tail(irp->input),
			PathLength / 2, &path, 0, NULL, NULL);

	if (status < 1)
		path = (char*) calloc(1, 1);

	file = drive_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		stream_write_UINT32(irp->output, 0); /* Length */
		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!drive_file_query_directory(file, FsInformationClass, InitialQuery, path, irp->output))
	{
		irp->IoStatus = STATUS_NO_MORE_FILES;
	}

	free(path);

	irp->Complete(irp);
}

static void drive_process_irp_directory_control(DRIVE_DEVICE* disk, IRP* irp)
{
	switch (irp->MinorFunction)
	{
		case IRP_MN_QUERY_DIRECTORY:
			drive_process_irp_query_directory(disk, irp);
			break;

		case IRP_MN_NOTIFY_CHANGE_DIRECTORY: /* TODO */
			irp->Discard(irp);
			break;

		default:
			DEBUG_WARN("MinorFunction 0x%X not supported", irp->MinorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			stream_write_UINT32(irp->output, 0); /* Length */
			irp->Complete(irp);
			break;
	}
}

static void drive_process_irp_device_control(DRIVE_DEVICE* disk, IRP* irp)
{
	stream_write_UINT32(irp->output, 0); /* OutputBufferLength */
	irp->Complete(irp);
}

static void drive_process_irp(DRIVE_DEVICE* disk, IRP* irp)
{
	irp->IoStatus = STATUS_SUCCESS;

	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			drive_process_irp_create(disk, irp);
			break;

		case IRP_MJ_CLOSE:
			drive_process_irp_close(disk, irp);
			break;

		case IRP_MJ_READ:
			drive_process_irp_read(disk, irp);
			break;

		case IRP_MJ_WRITE:
			drive_process_irp_write(disk, irp);
			break;

		case IRP_MJ_QUERY_INFORMATION:
			drive_process_irp_query_information(disk, irp);
			break;

		case IRP_MJ_SET_INFORMATION:
			drive_process_irp_set_information(disk, irp);
			break;

		case IRP_MJ_QUERY_VOLUME_INFORMATION:
			drive_process_irp_query_volume_information(disk, irp);
			break;

		case IRP_MJ_LOCK_CONTROL :
			DEBUG_WARN("MajorFunction IRP_MJ_LOCK_CONTROL silent ignored");
			drive_process_irp_silent_ignore(disk, irp);
			break;

		case IRP_MJ_DIRECTORY_CONTROL:
			drive_process_irp_directory_control(disk, irp);
			break;

		case IRP_MJ_DEVICE_CONTROL:
			drive_process_irp_device_control(disk, irp);
			break;

		default:
			DEBUG_WARN("MajorFunction 0x%X not supported", irp->MajorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void drive_process_irp_list(DRIVE_DEVICE* disk)
{
	IRP* irp;

	while (1)
	{
		if (WaitForSingleObject(disk->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		irp = (IRP*) InterlockedPopEntrySList(disk->pIrpList);

		if (irp == NULL)
			break;

		drive_process_irp(disk, irp);
	}
}

static void* drive_thread_func(void* arg)
{
	DRIVE_DEVICE* disk = (DRIVE_DEVICE*) arg;

	while (1)
	{
		WaitForSingleObject(disk->irpEvent, INFINITE);

		if (WaitForSingleObject(disk->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		ResetEvent(disk->irpEvent);
		drive_process_irp_list(disk);
	}

	return NULL;
}

static void drive_irp_request(DEVICE* device, IRP* irp)
{
	DRIVE_DEVICE* disk = (DRIVE_DEVICE*) device;

	InterlockedPushEntrySList(disk->pIrpList, &(irp->ItemEntry));

	SetEvent(disk->irpEvent);
}

static void drive_free(DEVICE* device)
{
	IRP* irp;
	DRIVE_FILE* file;
	DRIVE_DEVICE* disk = (DRIVE_DEVICE*) device;

	SetEvent(disk->stopEvent);
	CloseHandle(disk->thread);
	CloseHandle(disk->irpEvent);

	while ((irp = (IRP*) InterlockedPopEntrySList(disk->pIrpList)) != NULL)
		irp->Discard(irp);

	_aligned_free(disk->pIrpList);

	while ((file = (DRIVE_FILE*) list_dequeue(disk->files)) != NULL)
		drive_file_free(file);

	list_free(disk->files);

	free(disk);
}

void drive_register_drive_path(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints, char* name, char* path)
{
	int i, length;
	DRIVE_DEVICE* disk;

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
		disk = (DRIVE_DEVICE*) malloc(sizeof(DRIVE_DEVICE));
		ZeroMemory(disk, sizeof(DRIVE_DEVICE));

		disk->device.type = RDPDR_DTYP_FILESYSTEM;
		disk->device.name = name;
		disk->device.IRPRequest = drive_irp_request;
		disk->device.Free = drive_free;

		length = strlen(name);
		disk->device.data = stream_new(length + 1);

		for (i = 0; i <= length; i++)
			stream_write_BYTE(disk->device.data, name[i] < 0 ? '_' : name[i]);

		disk->path = path;
		disk->files = list_new();

		disk->pIrpList = (PSLIST_HEADER) _aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
		InitializeSListHead(disk->pIrpList);

		disk->irpEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		disk->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		disk->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) drive_thread_func, disk, CREATE_SUSPENDED, NULL);

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) disk);

                ResumeThread(disk->thread);
	}
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	drive_DeviceServiceEntry
#endif

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
				_snprintf(buf, sizeof(buf) - 4, "%s", drive->Name);
				len = strlen(buf);
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
