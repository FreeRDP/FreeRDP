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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include "registry_reg.h"

#define WINPR_HKLM_HIVE		"/etc/winpr/HKLM.reg"

static void reg_print_key(Reg* reg, RegKey* key);
static void reg_print_value(Reg* reg, RegVal* value);

struct reg_data_type
{
	char* tag;
	int length;
	DWORD type;
};

static struct reg_data_type REG_DATA_TYPE_TABLE[] =
{
	{ "\"",		1,	REG_SZ		},
	{ "dword:",	6,	REG_DWORD	},
	{ "str:\"",	5,	REG_SZ		},
	{ "str(2):\"",	8,	REG_EXPAND_SZ	},
	{ "str(7):\"",	8,	REG_MULTI_SZ	},
	{ "hex:",	4,	REG_BINARY	},
	{ "hex(2):\"",	8,	REG_EXPAND_SZ	},
	{ "hex(7):\"",	8,	REG_MULTI_SZ	},
	{ "hex(b):\"",	8,	REG_QWORD	},
	{ NULL,		0,	0		}
};

static char* REG_DATA_TYPE_STRINGS[] =
{
	"REG_NONE",
	"REG_SZ",
	"REG_EXPAND_SZ",
	"REG_BINARY",
	"REG_DWORD",
	"REG_DWORD_BIG_ENDIAN",
	"REG_LINK",
	"REG_MULTI_SZ",
	"REG_RESOURCE_LIST",
	"REG_FULL_RESOURCE_DESCRIPTOR",
	"REG_RESOURCE_REQUIREMENTS_LIST",
	"REG_QWORD"
};

static void reg_load_start(Reg* reg)
{
	long int file_size;

	fseek(reg->fp, 0, SEEK_END);
	file_size = ftell(reg->fp);
	fseek(reg->fp, 0, SEEK_SET);

	reg->line = NULL;
	reg->next_line = NULL;
	reg->buffer = NULL;

	if (file_size < 1)
		return;

	reg->buffer = (char*) malloc(file_size + 2);

	if (fread(reg->buffer, file_size, 1, reg->fp) != 1)
	{
		free(reg->buffer);
		reg->buffer = NULL;
		return;
	}

	reg->buffer[file_size] = '\n';
	reg->buffer[file_size + 1] = '\0';

	reg->next_line = strtok(reg->buffer, "\n");
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

static RegVal* reg_load_value(Reg* reg, RegKey* key)
{
	int index;
	char* p[5];
	int length;
	char* name;
	char* type;
	char* data;
	RegVal* value;

	p[0] = reg->line + 1;
	p[1] = strstr(p[0], "\"=");
	p[2] = p[1] + 2;
	type = p[2];

	if (p[2][0] == '"')
		p[3] = p[2];
	else
		p[3] = strchr(p[2], ':');

	data = p[3] + 1;

	length = p[1] - p[0];
	name = (char*) malloc(length + 1);
	memcpy(name, p[0], length);
	name[length] = '\0';

	value = (RegVal*) malloc(sizeof(RegVal));

	value->name = name;
	value->type = REG_NONE;
	value->next = value->prev = NULL;

	for (index = 0; REG_DATA_TYPE_TABLE[index].length > 0; index++)
	{
		if (strncmp(type, REG_DATA_TYPE_TABLE[index].tag, REG_DATA_TYPE_TABLE[index].length) == 0)
		{
			value->type = REG_DATA_TYPE_TABLE[index].type;
			break;
		}
	}

	if (value->type == REG_DWORD)
	{
		value->data.dword = strtoul(data, NULL, 16);
	}
	else if (value->type == REG_SZ)
	{
		p[4] = strchr(data, '"');
		p[4][0] = '\0';
		value->data.string = _strdup(data);
	}
	else
	{
		fprintf(stderr, "unimplemented format: %s\n", REG_DATA_TYPE_STRINGS[value->type]);
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
	return reg->next_line;
}

static void reg_insert_key(Reg* reg, RegKey* key, RegKey* subkey)
{
	char* name;
	char* path;
	char* save;
	int length;

	path = _strdup(subkey->name);

	name = strtok_s(path, "\\", &save);

	while (name != NULL)
	{
		if (strcmp(key->name, name) == 0)
		{
			length = strlen(name);
			name += length + 1;
			subkey->subname = _strdup(name);
		}

		name = strtok_s(NULL, "\\", &save);
	}

	free(path);
}

static RegKey* reg_load_key(Reg* reg, RegKey* key)
{
	char* p[2];
	int length;
	char* line;
	RegKey* subkey;

	p[0] = reg->line + 1;
	p[1] = strrchr(p[0], ']');

	subkey = (RegKey*) malloc(sizeof(RegKey));

	subkey->values = NULL;
	subkey->prev = subkey->next = NULL;

	length = p[1] - p[0];
	subkey->name = (char*) malloc(length + 1);
	memcpy(subkey->name, p[0], length);
	subkey->name[length] = '\0';

	while (reg_load_has_next_line(reg))
	{
		line = reg_load_peek_next_line(reg);

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

void reg_load(Reg* reg)
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
	if (value->type == REG_DWORD)
	{

	}
	else if (value->type == REG_SZ)
	{
		free(value->data.string);
	}
	else
	{
		fprintf(stderr, "unimplemented format: %s\n", REG_DATA_TYPE_STRINGS[value->type]);
	}

	free(value);
}

static void reg_unload_key(Reg* reg, RegKey* key)
{
	RegVal* pValue;
	RegVal* pValueNext;

	pValue = key->values;

	while (pValue != NULL)
	{
		pValueNext = pValue->next;
		reg_unload_value(reg, pValue);
		pValue = pValueNext;
	}

	free(key->name);
	free(key);
}

void reg_unload(Reg* reg)
{
	RegKey* pKey;
	RegKey* pKeyNext;

	pKey = reg->root_key->subkeys;

	while (pKey != NULL)
	{
		pKeyNext = pKey->next;
		reg_unload_key(reg, pKey);
		pKey = pKeyNext;
	}

	free(reg->root_key);
}

Reg* reg_open(BOOL read_only)
{
	Reg* reg;

	reg = (Reg*) malloc(sizeof(Reg));

	if (reg)
	{
		reg->read_only = read_only;
		reg->filename = WINPR_HKLM_HIVE;

		if (reg->read_only)
		{
			reg->fp = fopen(reg->filename, "r");
		}
		else
		{
			reg->fp = fopen(reg->filename, "r+");

			if (!reg->fp)
				reg->fp = fopen(reg->filename, "w+");
		}

		if (!reg->fp)
		{
			free(reg);
			return NULL;
		}

		reg->root_key = (RegKey*) malloc(sizeof(RegKey));

		reg->root_key->values = NULL;
		reg->root_key->subkeys = NULL;
		reg->root_key->name = "HKEY_LOCAL_MACHINE";

		reg_load(reg);
	}

	return reg;
}

void reg_close(Reg* reg)
{
	if (reg)
	{
		reg_unload(reg);
		fclose(reg->fp);
		free(reg);
	}
}

void reg_print_value(Reg* reg, RegVal* value)
{
	fprintf(stderr, "\"%s\"=", value->name);

	if (value->type == REG_DWORD)
	{
		fprintf(stderr, "dword:%08lX\n", value->data.dword);
	}
	else if (value->type == REG_SZ)
	{
		fprintf(stderr, "%s\"\n", value->data.string);
	}
	else
	{
		fprintf(stderr, "unimplemented format: %s\n", REG_DATA_TYPE_STRINGS[value->type]);
	}
}

void reg_print_key(Reg* reg, RegKey* key)
{
	RegVal* pValue;

	pValue = key->values;

	fprintf(stderr, "[%s]\n", key->name);

	while (pValue != NULL)
	{
		reg_print_value(reg, pValue);
		pValue = pValue->next;
	}
}

void reg_print(Reg* reg)
{
	RegKey* pKey;

	pKey = reg->root_key->subkeys;

	while (pKey != NULL)
	{
		reg_print_key(reg, pKey);
		pKey = pKey->next;
	}
}
