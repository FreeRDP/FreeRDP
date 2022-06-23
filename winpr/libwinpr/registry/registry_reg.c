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

#include <winpr/config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/file.h>

#include "registry_reg.h"

#include "../log.h"
#define TAG WINPR_TAG("registry")

#define WINPR_HKLM_HIVE "/etc/winpr/HKLM.reg"

struct reg_data_type
{
	char* tag;
	size_t length;
	DWORD type;
};

static struct reg_data_type REG_DATA_TYPE_TABLE[] = { { "\"", 1, REG_SZ },
	                                                  { "dword:", 6, REG_DWORD },
	                                                  { "str:\"", 5, REG_SZ },
	                                                  { "str(2):\"", 8, REG_EXPAND_SZ },
	                                                  { "str(7):\"", 8, REG_MULTI_SZ },
	                                                  { "hex:", 4, REG_BINARY },
	                                                  { "hex(2):\"", 8, REG_EXPAND_SZ },
	                                                  { "hex(7):\"", 8, REG_MULTI_SZ },
	                                                  { "hex(b):\"", 8, REG_QWORD } };

static char* reg_data_type_string(DWORD type)
{
	switch (type)
	{
		case REG_NONE:
			return "REG_NONE";
		case REG_SZ:
			return "REG_SZ";
		case REG_EXPAND_SZ:
			return "REG_EXPAND_SZ";
		case REG_BINARY:
			return "REG_BINARY";
		case REG_DWORD:
			return "REG_DWORD";
		case REG_DWORD_BIG_ENDIAN:
			return "REG_DWORD_BIG_ENDIAN";
		case REG_LINK:
			return "REG_LINK";
		case REG_MULTI_SZ:
			return "REG_MULTI_SZ";
		case REG_RESOURCE_LIST:
			return "REG_RESOURCE_LIST";
		case REG_FULL_RESOURCE_DESCRIPTOR:
			return "REG_FULL_RESOURCE_DESCRIPTOR";
		case REG_RESOURCE_REQUIREMENTS_LIST:
			return "REG_RESOURCE_REQUIREMENTS_LIST";
		case REG_QWORD:
			return "REG_QWORD";
		default:
			return "REG_UNKNOWN";
	}
}

static BOOL reg_load_start(Reg* reg)
{
	char* buffer;
	INT64 file_size;

	WINPR_ASSERT(reg);
	WINPR_ASSERT(reg->fp);

	_fseeki64(reg->fp, 0, SEEK_END);
	file_size = _ftelli64(reg->fp);
	_fseeki64(reg->fp, 0, SEEK_SET);
	reg->line = NULL;
	reg->next_line = NULL;

	if (file_size < 1)
		return FALSE;

	buffer = (char*)realloc(reg->buffer, (size_t)file_size + 2);

	if (!buffer)
		return FALSE;
	reg->buffer = buffer;

	if (fread(reg->buffer, (size_t)file_size, 1, reg->fp) != 1)
		return FALSE;

	reg->buffer[file_size] = '\n';
	reg->buffer[file_size + 1] = '\0';
	reg->next_line = strtok(reg->buffer, "\n");
	return TRUE;
}

static void reg_load_finish(Reg* reg)
{
	if (!reg)
		return;

	if (reg->buffer)
	{
		free(reg->buffer);
		reg->buffer = NULL;
	}
}

static RegVal* reg_load_value(const Reg* reg, RegKey* key)
{
	size_t index;
	const char* p[5] = { 0 };
	size_t length;
	char* name = NULL;
	const char* type;
	const char* data;
	RegVal* value = NULL;

	WINPR_ASSERT(reg);
	WINPR_ASSERT(key);
	WINPR_ASSERT(reg->line);

	p[0] = reg->line + 1;
	p[1] = strstr(p[0], "\"=");
	if (!p[1])
		return NULL;

	p[2] = p[1] + 2;
	type = p[2];

	if (p[2][0] == '"')
		p[3] = p[2];
	else
		p[3] = strchr(p[2], ':');

	if (!p[3])
		return NULL;

	data = p[3] + 1;
	length = (size_t)(p[1] - p[0]);
	if (length < 1)
		goto fail;

	name = (char*)calloc(length + 1, sizeof(char));

	if (!name)
		goto fail;

	memcpy(name, p[0], length);
	value = (RegVal*)calloc(1, sizeof(RegVal));

	if (!value)
		goto fail;

	value->name = name;
	value->type = REG_NONE;

	for (index = 0; index < ARRAYSIZE(REG_DATA_TYPE_TABLE); index++)
	{
		const struct reg_data_type* current = &REG_DATA_TYPE_TABLE[index];
		WINPR_ASSERT(current->tag);
		WINPR_ASSERT(current->length > 0);
		WINPR_ASSERT(current->type != REG_NONE);

		if (strncmp(type, current->tag, current->length) == 0)
		{
			value->type = current->type;
			break;
		}
	}

	switch (value->type)
	{
		case REG_DWORD:
		{
			unsigned long val;
			errno = 0;
			val = strtoul(data, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
			{
				WLog_WARN(TAG, "%s::%s value %s invalid", key->name, value->name, data);
				goto fail;
			}
			value->data.dword = (DWORD)val;
		}
		break;
		case REG_QWORD:
		{
			unsigned long long val;
			errno = 0;
			val = strtoull(data, NULL, 0);

			if ((errno != 0) || (val > UINT64_MAX))
			{
				WLog_WARN(TAG, "%s::%s value %s invalid", key->name, value->name, data);
				goto fail;
			}

			value->data.qword = (UINT64)val;
		}
		break;
		case REG_SZ:
		{
			size_t len, cmp;
			char* end;
			char* start = strchr(data, '"');
			if (!start)
				goto fail;

			/* Check for terminating quote, check it is the last symbol */
			len = strlen(start);
			end = strchr(start + 1, '"');
			if (!end)
				goto fail;
			cmp = end - start + 1;
			if (len != cmp)
				goto fail;
			if (start[0] == '"')
				start++;
			if (end[0] == '"')
				end[0] = '\0';
			value->data.string = _strdup(start);

			if (!value->data.string)
				goto fail;
		}
		break;
		default:
			WLog_ERR(TAG, "[%s] %s unimplemented format: %s", key->name, value->name,
			         reg_data_type_string(value->type));
			break;
	}

	if (!key->values)
	{
		key->values = value;
	}
	else
	{
		RegVal* pValue = key->values;

		while (pValue->next != NULL)
		{
			pValue = pValue->next;
		}

		pValue->next = value;
		value->prev = pValue;
	}

	return value;

fail:
	free(value);
	free(name);
	return NULL;
}

static BOOL reg_load_has_next_line(Reg* reg)
{
	if (!reg)
		return 0;

	return (reg->next_line != NULL) ? 1 : 0;
}

static char* reg_load_get_next_line(Reg* reg)
{
	if (!reg)
		return NULL;

	reg->line = reg->next_line;
	reg->next_line = strtok(NULL, "\n");
	reg->line_length = strlen(reg->line);
	return reg->line;
}

static char* reg_load_peek_next_line(Reg* reg)
{
	WINPR_ASSERT(reg);
	return reg->next_line;
}

static void reg_insert_key(Reg* reg, RegKey* key, RegKey* subkey)
{
	char* name = NULL;
	char* path = NULL;
	char* save = NULL;

	WINPR_ASSERT(reg);
	WINPR_ASSERT(key);
	WINPR_ASSERT(subkey);
	WINPR_ASSERT(subkey->name);

	path = _strdup(subkey->name);

	if (!path)
		return;

	name = strtok_s(path, "\\", &save);

	while (name != NULL)
	{
		if (strcmp(key->name, name) == 0)
		{
			if (save)
				subkey->subname = _strdup(save);

			/* TODO: free allocated memory in error case */
			if (!subkey->subname)
			{
				free(path);
				return;
			}
		}

		name = strtok_s(NULL, "\\", &save);
	}

	free(path);
}

static RegKey* reg_load_key(Reg* reg, RegKey* key)
{
	char* p[2];
	size_t length;
	RegKey* subkey;

	WINPR_ASSERT(reg);
	WINPR_ASSERT(key);

	WINPR_ASSERT(reg->line);
	p[0] = reg->line + 1;
	p[1] = strrchr(p[0], ']');
	if (!p[1])
		return NULL;

	subkey = (RegKey*)calloc(1, sizeof(RegKey));

	if (!subkey)
		return NULL;

	length = (size_t)(p[1] - p[0]);
	subkey->name = (char*)malloc(length + 1);

	if (!subkey->name)
	{
		free(subkey);
		return NULL;
	}

	memcpy(subkey->name, p[0], length);
	subkey->name[length] = '\0';

	while (reg_load_has_next_line(reg))
	{
		char* line = reg_load_peek_next_line(reg);

		if (line[0] == '[')
			break;

		reg_load_get_next_line(reg);

		if (reg->line[0] == '"')
		{
			reg_load_value(reg, subkey);
		}
	}

	reg_insert_key(reg, key, subkey);

	if (!key->subkeys)
	{
		key->subkeys = subkey;
	}
	else
	{
		RegKey* pKey = key->subkeys;

		while (pKey->next != NULL)
		{
			pKey = pKey->next;
		}

		pKey->next = subkey;
		subkey->prev = pKey;
	}

	return subkey;
}

static void reg_load(Reg* reg)
{
	reg_load_start(reg);

	while (reg_load_has_next_line(reg))
	{
		reg_load_get_next_line(reg);

		if (reg->line[0] == '[')
		{
			reg_load_key(reg, reg->root_key);
		}
	}

	reg_load_finish(reg);
}

static void reg_unload_value(Reg* reg, RegVal* value)
{
	WINPR_ASSERT(reg);
	WINPR_ASSERT(value);

	switch (value->type)
	{
		case REG_SZ:
			free(value->data.string);
			break;
		default:
			break;
	}

	free(value);
}

static void reg_unload_key(Reg* reg, RegKey* key)
{
	RegVal* pValue;

	WINPR_ASSERT(reg);
	WINPR_ASSERT(key);

	pValue = key->values;

	while (pValue != NULL)
	{
		RegVal* pValueNext = pValue->next;
		reg_unload_value(reg, pValue);
		pValue = pValueNext;
	}

	free(key->name);
	free(key);
}

static void reg_unload(Reg* reg)
{
	RegKey* pKey;

	WINPR_ASSERT(reg);
	if (reg->root_key)
	{
		pKey = reg->root_key->subkeys;

		while (pKey != NULL)
		{
			RegKey* pKeyNext = pKey->next;
			reg_unload_key(reg, pKey);
			pKey = pKeyNext;
		}

		free(reg->root_key);
	}
}

Reg* reg_open(BOOL read_only)
{
	Reg* reg = (Reg*)calloc(1, sizeof(Reg));

	if (!reg)
		return NULL;

	reg->read_only = read_only;
	reg->filename = WINPR_HKLM_HIVE;

	if (reg->read_only)
		reg->fp = winpr_fopen(reg->filename, "r");
	else
	{
		reg->fp = winpr_fopen(reg->filename, "r+");

		if (!reg->fp)
			reg->fp = winpr_fopen(reg->filename, "w+");
	}

	if (!reg->fp)
		goto fail;

	reg->root_key = (RegKey*)calloc(1, sizeof(RegKey));

	if (!reg->root_key)
		goto fail;

	reg->root_key->values = NULL;
	reg->root_key->subkeys = NULL;
	reg->root_key->name = "HKEY_LOCAL_MACHINE";
	reg_load(reg);
	return reg;

fail:
	reg_close(reg);
	return NULL;
}

void reg_close(Reg* reg)
{
	if (reg)
	{
		reg_unload(reg);
		if (reg->fp)
			fclose(reg->fp);
		free(reg);
	}
}

const char* reg_type_string(DWORD type)
{
	switch (type)
	{
		case REG_NONE:
			return "REG_NONE";
		case REG_SZ:
			return "REG_SZ";
		case REG_EXPAND_SZ:
			return "REG_EXPAND_SZ";
		case REG_BINARY:
			return "REG_BINARY";
		case REG_DWORD:
			return "REG_DWORD";
		case REG_DWORD_BIG_ENDIAN:
			return "REG_DWORD_BIG_ENDIAN";
		case REG_LINK:
			return "REG_LINK";
		case REG_MULTI_SZ:
			return "REG_MULTI_SZ";
		case REG_RESOURCE_LIST:
			return "REG_RESOURCE_LIST";
		case REG_FULL_RESOURCE_DESCRIPTOR:
			return "REG_FULL_RESOURCE_DESCRIPTOR";
		case REG_RESOURCE_REQUIREMENTS_LIST:
			return "REG_RESOURCE_REQUIREMENTS_LIST";
		case REG_QWORD:
			return "REG_QWORD";
		default:
			return "REG_UNKNOWN";
	}
}
