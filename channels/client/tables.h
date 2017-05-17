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

struct _STATIC_ENTRY
{
	const char* name;
	UINT(*entry)();
};
typedef struct _STATIC_ENTRY STATIC_ENTRY;

struct _STATIC_ENTRY_TABLE
{
	const char* name;
	const STATIC_ENTRY* table;
};
typedef struct _STATIC_ENTRY_TABLE STATIC_ENTRY_TABLE;

struct _STATIC_SUBSYSTEM_ENTRY
{
	const char* name;
	const char* type;
	void (*entry)(void);
};
typedef struct _STATIC_SUBSYSTEM_ENTRY STATIC_SUBSYSTEM_ENTRY;

struct _STATIC_ADDIN_TABLE
{
	const char* name;
	UINT(*entry)();
	const STATIC_SUBSYSTEM_ENTRY* table;
};
typedef struct _STATIC_ADDIN_TABLE STATIC_ADDIN_TABLE;
