/**
 * FreeRDP: A Remote Desktop Protocol client.
 * File System Virtual Channel
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2010-2011 Vic Lee
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

#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/unicode.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_constants.h"
#include "rdpdr_types.h"
#include "disk_file.h"

typedef struct _DISK_DEVICE DISK_DEVICE;
struct _DISK_DEVICE
{
	DEVICE device;

	char* path;
	LIST* files;

	LIST* irp_list;
	freerdp_thread* thread;
};


static uint32
disk_map_posix_err(int fs_errno)
{
	uint32 rc;

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
	DEBUG_SVC("errno 0x%x mapped to 0x%x\n", fs_errno, rc);
	return rc;
}

static DISK_FILE* disk_get_file_by_id(DISK_DEVICE* disk, uint32 id)
{
	LIST_ITEM* item;
	DISK_FILE* file;

	for (item = disk->files->head; item; item = item->next)
	{
		file = (DISK_FILE*)item->data;
		if (file->id == id)
			return file;
	}
	return NULL;
}

static void disk_process_irp_create(DISK_DEVICE* disk, IRP* irp)
{
	DISK_FILE* file;
	uint32 DesiredAccess;
	uint32 CreateDisposition;
	uint32 CreateOptions;
	uint32 PathLength;
	UNICONV* uniconv;
	char* path;
	uint32 FileId;
	uint8 Information;

	stream_read_uint32(irp->input, DesiredAccess);
	stream_seek(irp->input, 16); /* AllocationSize(8), FileAttributes(4), SharedAccess(4) */
	stream_read_uint32(irp->input, CreateDisposition);
	stream_read_uint32(irp->input, CreateOptions);
	stream_read_uint32(irp->input, PathLength);

	uniconv = freerdp_uniconv_new();
	path = freerdp_uniconv_in(uniconv, stream_get_tail(irp->input), PathLength);
	freerdp_uniconv_free(uniconv);

	FileId = irp->devman->id_sequence++;
	file = disk_file_new(disk->path, path, FileId,
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

		/* map errno to windows result*/
		irp->IoStatus = disk_map_posix_err(file->err);

		disk_file_free(file);
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

	stream_write_uint32(irp->output, FileId);
	stream_write_uint8(irp->output, Information);

	xfree(path);

	irp->Complete(irp);
}

static void disk_process_irp_close(DISK_DEVICE* disk, IRP* irp)
{
	DISK_FILE* file;

	file = disk_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else
	{
		DEBUG_SVC("%s(%d) closed.", file->fullpath, file->id);

		list_remove(disk->files, file);
		disk_file_free(file);
	}

	stream_write_zero(irp->output, 5); /* Padding(5) */

	irp->Complete(irp);
}

static void disk_process_irp_read(DISK_DEVICE* disk, IRP* irp)
{
	DISK_FILE* file;
	uint32 Length;
	uint64 Offset;
	uint8* buffer = NULL;

	stream_read_uint32(irp->input, Length);
	stream_read_uint64(irp->input, Offset);

	file = disk_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!disk_file_seek(file, Offset))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("seek %s(%d) failed.", file->fullpath, file->id);
	}
	else
	{
		buffer = (uint8*)xmalloc(Length);
		if (!disk_file_read(file, buffer, &Length))
		{
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			xfree(buffer);
			buffer = NULL;
			Length = 0;

			DEBUG_WARN("read %s(%d) failed.", file->fullpath, file->id);
		}
		else
		{
			DEBUG_SVC("read %llu-%llu from %s(%d).", Offset, Offset + Length, file->fullpath, file->id);
		}
	}

	stream_write_uint32(irp->output, Length);
	if (Length > 0)
	{
		stream_check_size(irp->output, Length);
		stream_write(irp->output, buffer, Length);
	}
	xfree(buffer);

	irp->Complete(irp);
}

static void disk_process_irp_write(DISK_DEVICE* disk, IRP* irp)
{
	DISK_FILE* file;
	uint32 Length;
	uint64 Offset;

	stream_read_uint32(irp->input, Length);
	stream_read_uint64(irp->input, Offset);
	stream_seek(irp->input, 20); /* Padding */

	file = disk_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!disk_file_seek(file, Offset))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("seek %s(%d) failed.", file->fullpath, file->id);
	}
	else if (!disk_file_write(file, stream_get_tail(irp->input), Length))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("write %s(%d) failed.", file->fullpath, file->id);
	}
	else
	{
		DEBUG_SVC("write %llu-%llu to %s(%d).", Offset, Offset + Length, file->fullpath, file->id);
	}

	stream_write_uint32(irp->output, Length);
	stream_write_uint8(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void disk_process_irp_query_information(DISK_DEVICE* disk, IRP* irp)
{
	DISK_FILE* file;
	uint32 FsInformationClass;

	stream_read_uint32(irp->input, FsInformationClass);

	file = disk_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!disk_file_query_information(file, FsInformationClass, irp->output))
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

static void disk_process_irp_set_information(DISK_DEVICE* disk, IRP* irp)
{
	DISK_FILE* file;
	uint32 FsInformationClass;
	uint32 Length;

	stream_read_uint32(irp->input, FsInformationClass);
	stream_read_uint32(irp->input, Length);
	stream_seek(irp->input, 24); /* Padding */

	file = disk_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!disk_file_set_information(file, FsInformationClass, Length, irp->input))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("FsInformationClass %d on %s(%d) failed.", FsInformationClass, file->fullpath, file->id);
	}
	else
	{
		DEBUG_SVC("FsInformationClass %d on %s(%d) ok.", FsInformationClass, file->fullpath, file->id);
	}

	stream_write_uint32(irp->output, Length);

	irp->Complete(irp);
}

static void disk_process_irp_query_volume_information(DISK_DEVICE* disk, IRP* irp)
{
	uint32 FsInformationClass;
	STREAM* output = irp->output;

	stream_read_uint32(irp->input, FsInformationClass);

	switch (FsInformationClass)
	{
		case FileFsVolumeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232108.aspx */
			stream_write_uint32(output, 34); /* Length */
			stream_check_size(output, 34);
			stream_write_uint64(output, 0); /* VolumeCreationTime */
			stream_write_uint32(output, 0); /* VolumeSerialNumber */
			stream_write_uint32(output, 16); /* VolumeLabelLength */
			stream_write_uint8(output, 0); /* SupportsObjects */
			stream_write_uint8(output, 0); /* Reserved */
			stream_write(output, "F\0R\0E\0E\0R\0D\0P\0\0\0", 16); /* VolumeLabel (Unicode) */
			break;

		case FileFsSizeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232107.aspx */
			stream_write_uint32(output, 24); /* Length */
			stream_check_size(output, 24);
			stream_write_uint64(output, 0x1000000); /* TotalAllocationUnits */
			stream_write_uint64(output, 0x800000); /* AvailableAllocationUnits */
			stream_write_uint32(output, 1); /* SectorsPerAllocationUnit */
			stream_write_uint32(output, 0x400); /* BytesPerSector */
			break;

		case FileFsAttributeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232101.aspx */
			stream_write_uint32(output, 22); /* Length */
			stream_check_size(output, 22);
			stream_write_uint32(output,
				FILE_CASE_SENSITIVE_SEARCH | 
				FILE_CASE_PRESERVED_NAMES | 
				FILE_UNICODE_ON_DISK); /* FileSystemAttributes */
			stream_write_uint32(output, 510); /* MaximumComponentNameLength */
			stream_write_uint32(output, 10); /* FileSystemNameLength */
			stream_write(output, "F\0A\0T\03\02\0", 10); /* FileSystemName */
			break;

		case FileFsFullSizeInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232104.aspx */
			stream_write_uint32(output, 32); /* Length */
			stream_check_size(output, 32);
			stream_write_uint64(output, 0x1000000); /* TotalAllocationUnits */
			stream_write_uint64(output, 0x800000); /* CallerAvailableAllocationUnits */
			stream_write_uint64(output, 0x800000); /* ActualAvailableAllocationUnits */
			stream_write_uint32(output, 1); /* SectorsPerAllocationUnit */
			stream_write_uint32(output, 0x400); /* BytesPerSector */
			break;

		case FileFsDeviceInformation:
			/* http://msdn.microsoft.com/en-us/library/cc232109.aspx */
			stream_write_uint32(output, 8); /* Length */
			stream_check_size(output, 8);
			stream_write_uint32(output, FILE_DEVICE_DISK); /* DeviceType */
			stream_write_uint32(output, 0); /* Characteristics */
			break;

		default:
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			stream_write_uint32(output, 0); /* Length */
			DEBUG_WARN("invalid FsInformationClass %d", FsInformationClass);
			break;
	}

	irp->Complete(irp);
}

static void disk_process_irp_query_directory(DISK_DEVICE* disk, IRP* irp)
{
	DISK_FILE* file;
	uint32 FsInformationClass;
	uint8 InitialQuery;
	uint32 PathLength;
	UNICONV* uniconv;
	char* path;

	stream_read_uint32(irp->input, FsInformationClass);
	stream_read_uint8(irp->input, InitialQuery);
	stream_read_uint32(irp->input, PathLength);
	stream_seek(irp->input, 23); /* Padding */

	uniconv = freerdp_uniconv_new();
	path = freerdp_uniconv_in(uniconv, stream_get_tail(irp->input), PathLength);
	freerdp_uniconv_free(uniconv);

	file = disk_get_file_by_id(disk, irp->FileId);

	if (file == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		stream_write_uint32(irp->output, 0); /* Length */
		DEBUG_WARN("FileId %d not valid.", irp->FileId);
	}
	else if (!disk_file_query_directory(file, FsInformationClass, InitialQuery, path, irp->output))
	{
		irp->IoStatus = STATUS_NO_MORE_FILES;
	}

	xfree(path);

	irp->Complete(irp);
}

static void disk_process_irp_directory_control(DISK_DEVICE* disk, IRP* irp)
{
	switch (irp->MinorFunction)
	{
		case IRP_MN_QUERY_DIRECTORY:
			disk_process_irp_query_directory(disk, irp);
			break;

		case IRP_MN_NOTIFY_CHANGE_DIRECTORY: /* TODO */
			irp->Discard(irp);
			break;

		default:
			DEBUG_WARN("MinorFunction 0x%X not supported", irp->MinorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			stream_write_uint32(irp->output, 0); /* Length */
			irp->Complete(irp);
			break;
	}
}

static void disk_process_irp_device_control(DISK_DEVICE* disk, IRP* irp)
{
	stream_write_uint32(irp->output, 0); /* OutputBufferLength */
	irp->Complete(irp);
}

static void disk_process_irp(DISK_DEVICE* disk, IRP* irp)
{
	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			disk_process_irp_create(disk, irp);
			break;

		case IRP_MJ_CLOSE:
			disk_process_irp_close(disk, irp);
			break;

		case IRP_MJ_READ:
			disk_process_irp_read(disk, irp);
			break;

		case IRP_MJ_WRITE:
			disk_process_irp_write(disk, irp);
			break;

		case IRP_MJ_QUERY_INFORMATION:
			disk_process_irp_query_information(disk, irp);
			break;

		case IRP_MJ_SET_INFORMATION:
			disk_process_irp_set_information(disk, irp);
			break;

		case IRP_MJ_QUERY_VOLUME_INFORMATION:
			disk_process_irp_query_volume_information(disk, irp);
			break;

		case IRP_MJ_DIRECTORY_CONTROL:
			disk_process_irp_directory_control(disk, irp);
			break;

		case IRP_MJ_DEVICE_CONTROL:
			disk_process_irp_device_control(disk, irp);
			break;

		default:
			DEBUG_WARN("MajorFunction 0x%X not supported", irp->MajorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void disk_process_irp_list(DISK_DEVICE* disk)
{
	IRP* irp;

	while (1)
	{
		if (freerdp_thread_is_stopped(disk->thread))
			break;

		freerdp_thread_lock(disk->thread);
		irp = (IRP*)list_dequeue(disk->irp_list);
		freerdp_thread_unlock(disk->thread);

		if (irp == NULL)
			break;

		disk_process_irp(disk, irp);
	}
}

static void* disk_thread_func(void* arg)
{
	DISK_DEVICE* disk = (DISK_DEVICE*)arg;

	while (1)
	{
		freerdp_thread_wait(disk->thread);

		if (freerdp_thread_is_stopped(disk->thread))
			break;

		freerdp_thread_reset(disk->thread);
		disk_process_irp_list(disk);
	}

	freerdp_thread_quit(disk->thread);

	return NULL;
}

static void disk_irp_request(DEVICE* device, IRP* irp)
{
	DISK_DEVICE* disk = (DISK_DEVICE*)device;

	freerdp_thread_lock(disk->thread);
	list_enqueue(disk->irp_list, irp);
	freerdp_thread_unlock(disk->thread);

	freerdp_thread_signal(disk->thread);
}

static void disk_free(DEVICE* device)
{
	DISK_DEVICE* disk = (DISK_DEVICE*)device;
	IRP* irp;
	DISK_FILE* file;

	freerdp_thread_stop(disk->thread);
	freerdp_thread_free(disk->thread);
	
	while ((irp = (IRP*)list_dequeue(disk->irp_list)) != NULL)
		irp->Discard(irp);
	list_free(disk->irp_list);

	while ((file = (DISK_FILE*)list_dequeue(disk->files)) != NULL)
		disk_file_free(file);
	list_free(disk->files);
	xfree(disk);
}

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	DISK_DEVICE* disk;
	char* name;
	char* path;
	int i, len;

	name = (char*)pEntryPoints->plugin_data->data[1];
	path = (char*)pEntryPoints->plugin_data->data[2];

	if (name[0] && path[0])
	{
		disk = xnew(DISK_DEVICE);

		disk->device.type = RDPDR_DTYP_FILESYSTEM;
		disk->device.name = name;
		disk->device.IRPRequest = disk_irp_request;
		disk->device.Free = disk_free;

		len = strlen(name);
		disk->device.data = stream_new(len + 1);
		for (i = 0; i <= len; i++)
			stream_write_uint8(disk->device.data, name[i] < 0 ? '_' : name[i]);

		disk->path = path;
		disk->files = list_new();

		disk->irp_list = list_new();
		disk->thread = freerdp_thread_new();

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*)disk);

		freerdp_thread_start(disk->thread, disk_thread_func, disk);
	}

	return 0;
}
