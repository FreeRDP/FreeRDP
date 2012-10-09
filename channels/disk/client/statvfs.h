/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * statvfs emulation for windows
 *
 * Copyright 2012 Gerald Richter
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

#ifndef RDPDR_DISK_STATVFS_H
#define RDPDR_DISK_STATVFS_H

#ifdef __cplusplus
extern "C" { 
#endif 

typedef unsigned long long fsblkcnt_t;
typedef unsigned long long fsfilcnt_t;

struct statvfs {
    unsigned long  f_bsize;    /* file system block size */
    unsigned long  f_frsize;   /* fragment size */
    fsblkcnt_t     f_blocks;   /* size of fs in f_frsize units */
    fsblkcnt_t     f_bfree;    /* # free blocks */
    fsblkcnt_t     f_bavail;   /* # free blocks for unprivileged users */
    fsfilcnt_t     f_files;    /* # inodes */
    fsfilcnt_t     f_ffree;    /* # free inodes */
    fsfilcnt_t     f_favail;   /* # free inodes for unprivileged users */
    unsigned long  f_fsid;     /* file system ID */
    unsigned long  f_flag;     /* mount flags */
    unsigned long  f_namemax;  /* maximum filename length */
};

int statvfs(const char *path, struct statvfs *buf);

#ifdef __cplusplus
}
#endif 

#endif /* RDPDR_DISK_STATVFS_H */
