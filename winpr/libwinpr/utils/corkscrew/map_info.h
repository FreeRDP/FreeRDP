/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Process memory map. */

#ifndef _CORKSCREW_MAP_INFO_H
#define _CORKSCREW_MAP_INFO_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct map_info {
    struct map_info* next;
    uintptr_t start;
    uintptr_t end;
    bool is_readable;
    bool is_writable;
    bool is_executable;
    void* data; // arbitrary data associated with the map by the user, initially NULL
    char name[];
} map_info_t;

/* Loads memory map from /proc/<tid>/maps. */
map_info_t* load_map_info_list(pid_t tid);

/* Frees memory map. */
void free_map_info_list(map_info_t* milist);

/* Finds the memory map that contains the specified address. */
const map_info_t* find_map_info(const map_info_t* milist, uintptr_t addr);

/* Returns true if the addr is in a readable map. */
bool is_readable_map(const map_info_t* milist, uintptr_t addr);
/* Returns true if the addr is in a writable map. */
bool is_writable_map(const map_info_t* milist, uintptr_t addr);
/* Returns true if the addr is in an executable map. */
bool is_executable_map(const map_info_t* milist, uintptr_t addr);

/* Acquires a reference to the memory map for this process.
 * The result is cached and refreshed automatically.
 * Make sure to release the map info when done. */
map_info_t* acquire_my_map_info_list();

/* Releases a reference to the map info for this process that was
 * previous acquired using acquire_my_map_info_list(). */
void release_my_map_info_list(map_info_t* milist);

/* Flushes the cached memory map so the next call to
 * acquire_my_map_info_list() gets fresh data. */
void flush_my_map_info_list();

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_MAP_INFO_H
