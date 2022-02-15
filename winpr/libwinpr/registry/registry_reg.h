/**
 * WinPR: Windows Portable Runtime
 * Windows Registry (.reg file format)
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
#ifndef REGISTRY_REG_H_
#define REGISTRY_REG_H_

#include <winpr/registry.h>

typedef struct s_reg Reg;
typedef struct s_reg_key RegKey;
typedef struct s_reg_val RegVal;

struct s_reg
{
	FILE* fp;
	char* line;
	char* next_line;
	size_t line_length;
	char* buffer;
	char* filename;
	BOOL read_only;
	RegKey* root_key;
};

struct s_reg_val
{
	char* name;
	DWORD type;
	RegVal* prev;
	RegVal* next;

	union reg_data
	{
		DWORD dword;
		UINT64 qword;
		char* string;
	} data;
};

struct s_reg_key
{
	char* name;
	DWORD type;
	RegKey* prev;
	RegKey* next;

	char* subname;
	RegVal* values;
	RegKey* subkeys;
};

Reg* reg_open(BOOL read_only);
void reg_close(Reg* reg);

const char* reg_type_string(DWORD type);

#endif
