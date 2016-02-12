/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * statvfs emulation for Windows
 *
 * Copyright 2012 Gerald Richter
 * Copyright 2016 Inuvika Inc.
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#include <string.h>
#include <malloc.h>

#include <winpr/crt.h>
#include <winpr/windows.h>

#include "statvfs.h"

int statvfs(const char *path, struct statvfs *buf)
{
	BOOL res;
	int len;
	LPWSTR unicodestr = NULL;
	DWORD lpSectorsPerCluster;
	DWORD lpBytesPerSector;
	DWORD lpNumberOfFreeClusters;
	DWORD lpTotalNumberOfClusters;

	len = ConvertToUnicode(CP_ACP, 0, path, -1, &unicodestr, 0);
	if (len <= 0)
		return -1;

	res = GetDiskFreeSpaceW(unicodestr, &lpSectorsPerCluster, &lpBytesPerSector, &lpNumberOfFreeClusters, &lpTotalNumberOfClusters);
	free(unicodestr);

	buf->f_bsize = lpBytesPerSector; /* file system block size */
	buf->f_frsize = 0; /* fragment size */
	buf->f_blocks = lpTotalNumberOfClusters; /* size of fs in f_frsize units */
	buf->f_bfree = lpNumberOfFreeClusters; /* # free blocks */
	buf->f_bavail = lpNumberOfFreeClusters; /* # free blocks for unprivileged users */
	buf->f_files = 0; /* # inodes */
	buf->f_ffree = 0; /* # free inodes */
	buf->f_favail = 0; /* # free inodes for unprivileged users */
	buf->f_fsid = lpNumberOfFreeClusters & 0xffff; /* file system ID */
	buf->f_flag = 0; /* mount flags */
	buf->f_namemax = 250; /* maximum filename length */
	
	return res;
}
