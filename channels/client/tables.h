/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Static Entry Point Tables
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

#include <freerdp/svc.h>

/* The 'entry' function pointers have variable arguments. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

typedef struct
{
	const char* name;
	UINT (*entry)();
} STATIC_ENTRY;

typedef struct
{
	const char* name;
	const STATIC_ENTRY* table;
} STATIC_ENTRY_TABLE;

typedef struct
{
	const char* name;
	const char* type;
	UINT (*entry)();
} STATIC_SUBSYSTEM_ENTRY;

typedef struct
{
	const char* name;
	const char* type;
	UINT (*entry)();
	const STATIC_SUBSYSTEM_ENTRY* table;
} STATIC_ADDIN_TABLE;

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
