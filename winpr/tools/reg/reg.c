/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Registry API Tool
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/tchar.h>
#include <winpr/wtypes.h>

#include <winpr/print.h>
#include <winpr/registry.h>

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

#define WINPR_HKLM_HIVE		"/etc/winpr/HKLM.reg"

void QueryKey(HKEY hKey)
{
	DWORD i;
	DWORD status;
	DWORD cbName;
	DWORD cSubKeys = 0;
	DWORD cbMaxSubKey;
	DWORD cchMaxClass;
	DWORD cValues;
	DWORD cchMaxValue; 
	DWORD cbMaxValueData;
	DWORD cbSecurityDescriptor;
	FILETIME ftLastWriteTime;
	TCHAR achKey[MAX_KEY_LENGTH];
	TCHAR achClass[256] = _T("");
	DWORD cchClassName = 256;
	TCHAR achValue[MAX_VALUE_NAME];
	DWORD cchValue = MAX_VALUE_NAME;
 
	status = RegQueryInfoKey(hKey, achClass, &cchClassName, NULL,
		&cSubKeys, &cbMaxSubKey, &cchMaxClass, &cValues, &cchMaxValue,
		&cbMaxValueData, &cbSecurityDescriptor, &ftLastWriteTime);
	
	if (cSubKeys)
	{
		printf("\nNumber of subkeys: %d\n", (int) cSubKeys);

		for (i = 0; i < cSubKeys; i++)
		{ 
			cbName = MAX_KEY_LENGTH;

			status = RegEnumKeyEx(hKey, i, achKey, &cbName,
				NULL, NULL, NULL, &ftLastWriteTime);

			if (status == ERROR_SUCCESS)
				tprintf(_T("(%d) %s\n"), (int) i + 1, achKey);
		}
	}

	if (cValues)
	{
		printf("\nNumber of values: %d\n", (int) cValues);

		for (i = 0, status = ERROR_SUCCESS; i < cValues; i++)
		{
			achValue[0] = '\0';
			cchValue = MAX_VALUE_NAME;

			status = RegEnumValue(hKey, i, achValue, &cchValue, NULL, NULL, NULL, NULL);
 
			if (status == ERROR_SUCCESS)
				tprintf(_T("(%d) %s\n"), (int) i + 1, achValue);
		}
	}
}

#define TEST_DWORD_VALUE	5

typedef struct _reg Reg;
typedef struct _reg_key RegKey;
typedef struct _reg_val RegVal;

struct _reg
{
	FILE* fp;
	char* line;
	char* next_line;
	int line_length;
	char* buffer;
	char* filename;
	BOOL read_only;
	RegKey* root_key;
};

struct _reg_val
{
	char* name;
	DWORD type;
	RegVal* prev;
	RegVal* next;

	union reg_data
	{
		DWORD dword;
		char* string;
	} data;
};

struct _reg_key
{
	char* name;
	DWORD type;
	RegKey* prev;
	RegKey* next;

	RegVal* values;
	RegKey* subkeys;
};

void reg_load_start(Reg* reg)
{
	long int file_size;

	fseek(reg->fp, 0, SEEK_END);
	file_size = ftell(reg->fp);
	fseek(reg->fp, 0, SEEK_SET);

	if (file_size < 1)
		return;

	reg->buffer = (char*) malloc(file_size + 2);

	if (fread(reg->buffer, file_size, 1, reg->fp) != 1)
	{
		free(reg->buffer);
		return;
	}

	reg->buffer[file_size] = '\n';
	reg->buffer[file_size + 1] = '\0';

	reg->line = NULL;
	reg->next_line = strtok(reg->buffer, "\n");
}

void reg_load_finish(Reg* reg)
{
	free(reg->buffer);
	reg->buffer = NULL;
	reg->line = NULL;
}

struct reg_data_type
{
	char* tag;
	int length;
	DWORD type;
};

struct reg_data_type REG_DATA_TYPE_TABLE[] =
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

char* REG_DATA_TYPE_STRINGS[] =
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

RegVal* reg_load_value(Reg* reg, RegKey* key)
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
		value->data.dword = strtoul(data, NULL, 0);
	}
	else if (value->type == REG_SZ)
	{
		p[4] = strchr(data, '"');
		p[4][0] = '\0';
		value->data.string = strdup(data);
	}
	else
	{
		printf("unimplemented format: %s\n", REG_DATA_TYPE_STRINGS[value->type]);
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

BOOL reg_load_has_next_line(Reg* reg)
{
	return (reg->next_line != NULL) ? 1 : 0;
}

char* reg_load_get_next_line(Reg* reg)
{
	reg->line = reg->next_line;
	reg->next_line = strtok(NULL, "\n");
	reg->line_length = strlen(reg->line);

	return reg->line;
}

char* reg_load_peek_next_line(Reg* reg)
{
	return reg->next_line;
}

RegKey* reg_load_key(Reg* reg, RegKey* key)
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

void reg_unload_value(Reg* reg, RegVal* value)
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
		printf("unimplemented format: %s\n", REG_DATA_TYPE_STRINGS[value->type]);
	}

	free(value);
}

void reg_unload_key(Reg* reg, RegKey* key)
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

		reg->root_key = (RegKey*) malloc(sizeof(RegKey));

		reg->root_key->values = NULL;
		reg->root_key->subkeys = NULL;

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
	printf("\"%s\"=", value->name);

	if (value->type == REG_DWORD)
	{
		printf("dword:%08lX\n", value->data.dword);
	}
	else if (value->type == REG_SZ)
	{
		printf("%s\"\n", value->data.string);
	}
	else
	{
		printf("unimplemented format: %s\n", REG_DATA_TYPE_STRINGS[value->type]);
	}
}

void reg_print_key(Reg* reg, RegKey* key)
{
	RegVal* pValue;

	pValue = key->values;

	printf("[%s]\n", key->name);

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

#if 1

int main(int argc, char* argv[])
{
	Reg* reg;

	reg = reg_open(1);

	reg_print(reg);

	reg_close(reg);

	return 0;
}

#else

int main(int argc, char* argv[])
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		_T("SYSTEM\\CurrentControlSet\\Control\\Lsa"),
		0, KEY_READ, &hKey);

	if (status != ERROR_SUCCESS)
	{
		tprintf(_T("RegOpenKeyEx error: 0x%08lX\n"), status);
		return 0;
	}

	//QueryKey(hKey);

	RegCloseKey(hKey);

	status = RegCreateKeyEx(HKEY_CURRENT_USER,
		_T("SOFTWARE\\FreeRDP"), 0, 0, REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS, 0, &hKey, 0);

	if (status != ERROR_SUCCESS)
	{
		tprintf(_T("RegCreateKeyEx error: 0x%08lX\n"), status);
		return 0;
	}

	dwValue = TEST_DWORD_VALUE;
	status = RegSetValueEx(hKey, _T("test"), 0, REG_DWORD, (BYTE*) &dwValue, sizeof(dwValue));

	if (status != ERROR_SUCCESS)
	{
		tprintf(_T("RegSetValueEx error: 0x%08lX\n"), status);
		return 0;
	}

	RegCloseKey(hKey);

	status = RegCreateKeyEx(HKEY_CURRENT_USER,
		_T("SOFTWARE\\FreeRDP"), 0, 0, REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS, 0, &hKey, 0);

	if (status != ERROR_SUCCESS)
	{
		tprintf(_T("RegCreateKeyEx error: 0x%08lX\n"), status);
		return 0;
	}

	dwValue = 0;
	status = RegQueryValueEx(hKey, _T("test"), NULL, &dwType, (BYTE*) &dwValue, &dwSize);

	if (status != ERROR_SUCCESS)
	{
		tprintf(_T("RegQueryValueEx error: 0x%08lX\n"), status);
		return 0;
	}

	if (dwValue != TEST_DWORD_VALUE)
	{
		tprintf(_T("test value mismatch: actual:%d expected:%d\n"), (int) dwValue, TEST_DWORD_VALUE);
	}

	RegCloseKey(hKey);

	return 0;
}

#endif
