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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <fnmatch.h>
#include <utime.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/unicode.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_constants.h"
#include "rdpdr_types.h"

typedef struct _FILE_INFO FILE_INFO;
struct _FILE_INFO
{
	uint32 file_id;
	uint32 file_attr;
	boolean is_dir;
	int fd;
	DIR* dir;
	char* fullpath;
	char* pattern;
	boolean delete_pending;
};

typedef struct _DISK_DEVICE DISK_DEVICE;
struct _DISK_DEVICE
{
	DEVICE device;

	char* path;
	LIST* files;
};

void disk_irp_request(DEVICE* device, IRP* irp)
{
	DISK_DEVICE* disk = (DISK_DEVICE*)device;

	irp->Discard(irp);
}

void disk_free(DEVICE* device)
{
	DISK_DEVICE* disk = (DISK_DEVICE*)device;

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

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*)disk);
	}

	return 0;
}
