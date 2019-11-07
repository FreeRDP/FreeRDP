/**
 * WinPR: Windows Portable Runtime
 * .ini config file
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <errno.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>

#include <winpr/ini.h>

struct _wIniFileKey
{
	char* name;
	char* value;
};
typedef struct _wIniFileKey wIniFileKey;

struct _wIniFileSection
{
	char* name;
	size_t nKeys;
	size_t cKeys;
	wIniFileKey** keys;
};
typedef struct _wIniFileSection wIniFileSection;

struct _wIniFile
{
	FILE* fp;
	char* line;
	char* nextLine;
	size_t lineLength;
	char* tokctx;
	char* buffer;
	char* filename;
	BOOL readOnly;
	size_t nSections;
	size_t cSections;
	wIniFileSection** sections;
};

static BOOL IniFile_Load_NextLine(wIniFile* ini, char* str)
{
	size_t length = 0;
	ini->nextLine = strtok_s(str, "\n", &ini->tokctx);

	if (ini->nextLine)
		length = strlen(ini->nextLine);

	if (length > 0)
	{
		if (ini->nextLine[length - 1] == '\r')
		{
			ini->nextLine[length - 1] = '\0';
			length--;
		}

		if (length < 1)
			ini->nextLine = NULL;
	}

	return (ini->nextLine) ? TRUE : FALSE;
}

static BOOL IniFile_Load_String(wIniFile* ini, const char* iniString)
{
	size_t fileSize;
	if (!ini || !iniString)
		return FALSE;

	ini->line = NULL;
	ini->nextLine = NULL;
	ini->buffer = NULL;
	fileSize = strlen(iniString);

	if (fileSize < 1)
		return FALSE;

	ini->buffer = (char*)malloc(fileSize + 2);

	if (!ini->buffer)
		return FALSE;

	CopyMemory(ini->buffer, iniString, fileSize);
	ini->buffer[fileSize] = '\n';
	ini->buffer[fileSize + 1] = '\0';
	IniFile_Load_NextLine(ini, ini->buffer);
	return TRUE;
}

static BOOL IniFile_Open_File(wIniFile* ini, const char* filename)
{
	if (!ini || !filename)
		return FALSE;

	if (ini->readOnly)
		ini->fp = fopen(filename, "rb");
	else
		ini->fp = fopen(filename, "w+b");

	if (!ini->fp)
		return FALSE;

	return TRUE;
}

static BOOL IniFile_Load_File(wIniFile* ini, const char* filename)
{
	INT64 fileSize;

	if (!IniFile_Open_File(ini, filename))
		return FALSE;

	if (_fseeki64(ini->fp, 0, SEEK_END) < 0)
		goto out_file;

	fileSize = _ftelli64(ini->fp);

	if (fileSize < 0)
		goto out_file;

	if (_fseeki64(ini->fp, 0, SEEK_SET) < 0)
		goto out_file;

	ini->line = NULL;
	ini->nextLine = NULL;
	ini->buffer = NULL;

	if (fileSize < 1)
		goto out_file;

	ini->buffer = (char*)malloc((size_t)fileSize + 2);

	if (!ini->buffer)
		goto out_file;

	if (fread(ini->buffer, (size_t)fileSize, 1, ini->fp) != 1)
		goto out_buffer;

	fclose(ini->fp);
	ini->fp = NULL;
	ini->buffer[fileSize] = '\n';
	ini->buffer[fileSize + 1] = '\0';
	IniFile_Load_NextLine(ini, ini->buffer);
	return TRUE;
out_buffer:
	free(ini->buffer);
	ini->buffer = NULL;
out_file:
	fclose(ini->fp);
	ini->fp = NULL;
	return FALSE;
}

static void IniFile_Load_Finish(wIniFile* ini)
{
	if (!ini)
		return;

	if (ini->buffer)
	{
		free(ini->buffer);
		ini->buffer = NULL;
	}
}

static BOOL IniFile_Load_HasNextLine(wIniFile* ini)
{
	if (!ini)
		return FALSE;

	return (ini->nextLine) ? TRUE : FALSE;
}

static char* IniFile_Load_GetNextLine(wIniFile* ini)
{
	if (!ini)
		return NULL;

	ini->line = ini->nextLine;
	ini->lineLength = strlen(ini->line);
	IniFile_Load_NextLine(ini, NULL);
	return ini->line;
}

static wIniFileKey* IniFile_Key_New(const char* name, const char* value)
{
	wIniFileKey* key;

	if (!name || !value)
		return NULL;

	key = malloc(sizeof(wIniFileKey));

	if (key)
	{
		key->name = _strdup(name);
		key->value = _strdup(value);

		if (!key->name || !key->value)
		{
			free(key->name);
			free(key->value);
			free(key);
			return NULL;
		}
	}

	return key;
}

static void IniFile_Key_Free(wIniFileKey* key)
{
	if (!key)
		return;

	free(key->name);
	free(key->value);
	free(key);
}

static wIniFileSection* IniFile_Section_New(const char* name)
{
	wIniFileSection* section;

	if (!name)
		return NULL;

	section = malloc(sizeof(wIniFileSection));

	if (!section)
		return NULL;

	section->name = _strdup(name);

	if (!section->name)
	{
		free(section);
		return NULL;
	}

	section->nKeys = 0;
	section->cKeys = 64;
	section->keys = (wIniFileKey**)calloc(section->cKeys, sizeof(wIniFileKey*));

	if (!section->keys)
	{
		free(section->name);
		free(section);
		return NULL;
	}

	return section;
}

static void IniFile_Section_Free(wIniFileSection* section)
{
	size_t index;

	if (!section)
		return;

	free(section->name);

	for (index = 0; index < section->nKeys; index++)
	{
		IniFile_Key_Free(section->keys[index]);
	}

	free(section->keys);
	free(section);
}

static wIniFileSection* IniFile_GetSection(wIniFile* ini, const char* name)
{
	size_t index;
	wIniFileSection* section = NULL;

	if (!ini || !name)
		return NULL;

	for (index = 0; index < ini->nSections; index++)
	{
		if (_stricmp(name, ini->sections[index]->name) == 0)
		{
			section = ini->sections[index];
			break;
		}
	}

	return section;
}

static wIniFileSection* IniFile_AddSection(wIniFile* ini, const char* name)
{
	wIniFileSection* section;

	if (!ini || !name)
		return NULL;

	section = IniFile_GetSection(ini, name);

	if (!section)
	{
		if ((ini->nSections + 1) >= (ini->cSections))
		{
			size_t new_size;
			wIniFileSection** new_sect;
			new_size = ini->cSections * 2;
			new_sect =
			    (wIniFileSection**)realloc(ini->sections, sizeof(wIniFileSection*) * new_size);

			if (!new_sect)
				return NULL;

			ini->cSections = new_size;
			ini->sections = new_sect;
		}

		section = IniFile_Section_New(name);
		ini->sections[ini->nSections] = section;
		ini->nSections++;
	}

	return section;
}

static wIniFileKey* IniFile_GetKey(wIniFile* ini, wIniFileSection* section, const char* name)
{
	size_t index;
	wIniFileKey* key = NULL;

	if (!ini || !section || !name)
		return NULL;

	for (index = 0; index < section->nKeys; index++)
	{
		if (_stricmp(name, section->keys[index]->name) == 0)
		{
			key = section->keys[index];
			break;
		}
	}

	return key;
}

static wIniFileKey* IniFile_AddKey(wIniFile* ini, wIniFileSection* section, const char* name,
                                   const char* value)
{
	wIniFileKey* key;

	if (!section || !name || !value)
		return NULL;

	key = IniFile_GetKey(ini, section, name);

	if (!key)
	{
		if ((section->nKeys + 1) >= (section->cKeys))
		{
			size_t new_size;
			wIniFileKey** new_key;
			new_size = section->cKeys * 2;
			new_key = (wIniFileKey**)realloc(section->keys, sizeof(wIniFileKey*) * new_size);

			if (!new_key)
				return NULL;

			section->cKeys = new_size;
			section->keys = new_key;
		}

		key = IniFile_Key_New(name, value);

		if (!key)
			return NULL;

		section->keys[section->nKeys] = key;
		section->nKeys++;
	}
	else
	{
		free(key->value);
		key->value = _strdup(value);

		if (!key->value)
			return NULL;
	}

	return key;
}

static int IniFile_Load(wIniFile* ini)
{
	char* line;
	char* name;
	char* value;
	char* separator;
	char *beg, *end;
	wIniFileSection* section = NULL;

	if (!ini)
		return -1;

	while (IniFile_Load_HasNextLine(ini))
	{
		line = IniFile_Load_GetNextLine(ini);

		if (line[0] == ';')
			continue;

		if (line[0] == '[')
		{
			beg = &line[1];
			end = strchr(line, ']');

			if (!end)
				return -1;

			*end = '\0';
			IniFile_AddSection(ini, beg);
			section = ini->sections[ini->nSections - 1];
		}
		else
		{
			separator = strchr(line, '=');

			if (separator == NULL)
				return -1;

			end = separator;

			while ((&end[-1] > line) && ((end[-1] == ' ') || (end[-1] == '\t')))
				end--;

			*end = '\0';
			name = line;
			beg = separator + 1;

			while (*beg && ((*beg == ' ') || (*beg == '\t')))
				beg++;

			if (*beg == '"')
				beg++;

			end = &line[ini->lineLength];

			while ((end > beg) && ((end[-1] == ' ') || (end[-1] == '\t')))
				end--;

			if (end[-1] == '"')
				end[-1] = '\0';

			value = beg;

			if (!IniFile_AddKey(ini, section, name, value))
				return -1;
		}
	}

	IniFile_Load_Finish(ini);
	return 1;
}

int IniFile_ReadBuffer(wIniFile* ini, const char* buffer)
{
	BOOL status;
	if (!ini || !buffer)
		return -1;

	ini->readOnly = TRUE;
	ini->filename = NULL;
	status = IniFile_Load_String(ini, buffer);

	if (!status)
		return -1;

	return IniFile_Load(ini);
}

int IniFile_ReadFile(wIniFile* ini, const char* filename)
{
	ini->readOnly = TRUE;
	free(ini->filename);
	ini->filename = _strdup(filename);

	if (!ini->filename)
		return -1;

	if (!IniFile_Load_File(ini, filename))
		return -1;

	return IniFile_Load(ini);
}

char** IniFile_GetSectionNames(wIniFile* ini, int* count)
{
	char* p;
	size_t index;
	size_t length;
	size_t nameLength;
	char** sectionNames;
	wIniFileSection* section = NULL;

	if (!ini || !count)
		return NULL;

	if (ini->nSections > INT_MAX)
		return NULL;

	length = (sizeof(char*) * ini->nSections) + sizeof(char);

	for (index = 0; index < ini->nSections; index++)
	{
		section = ini->sections[index];
		nameLength = strlen(section->name);
		length += (nameLength + 1);
	}

	sectionNames = (char**)malloc(length);

	if (!sectionNames)
		return NULL;

	p = (char*)&((BYTE*)sectionNames)[sizeof(char*) * ini->nSections];

	for (index = 0; index < ini->nSections; index++)
	{
		sectionNames[index] = p;
		section = ini->sections[index];
		nameLength = strlen(section->name);
		CopyMemory(p, section->name, nameLength + 1);
		p += (nameLength + 1);
	}

	*p = '\0';
	*count = (int)ini->nSections;
	return sectionNames;
}

char** IniFile_GetSectionKeyNames(wIniFile* ini, const char* section, int* count)
{
	char* p;
	size_t index;
	size_t length;
	size_t nameLength;
	char** keyNames;
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;

	if (!ini || !section || !count)
		return NULL;

	pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		return NULL;

	if (pSection->nKeys > INT_MAX)
		return NULL;

	length = (sizeof(char*) * pSection->nKeys) + sizeof(char);

	for (index = 0; index < pSection->nKeys; index++)
	{
		pKey = pSection->keys[index];
		nameLength = strlen(pKey->name);
		length += (nameLength + 1);
	}

	keyNames = (char**)malloc(length);

	if (!keyNames)
		return NULL;

	p = (char*)&((BYTE*)keyNames)[sizeof(char*) * pSection->nKeys];

	for (index = 0; index < pSection->nKeys; index++)
	{
		keyNames[index] = p;
		pKey = pSection->keys[index];
		nameLength = strlen(pKey->name);
		CopyMemory(p, pKey->name, nameLength + 1);
		p += (nameLength + 1);
	}

	*p = '\0';
	*count = (int)pSection->nKeys;
	return keyNames;
}

const char* IniFile_GetKeyValueString(wIniFile* ini, const char* section, const char* key)
{
	const char* value = NULL;
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;
	pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		return NULL;

	pKey = IniFile_GetKey(ini, pSection, key);

	if (!pKey)
		return NULL;

	value = (const char*)pKey->value;
	return value;
}

int IniFile_GetKeyValueInt(wIniFile* ini, const char* section, const char* key)
{
	int err;
	long value = 0;
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;
	pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		return 0;

	pKey = IniFile_GetKey(ini, pSection, key);

	if (!pKey)
		return 0;

	err = errno;
	errno = 0;
	value = strtol(pKey->value, NULL, 0);
	if ((value < INT_MIN) || (value > INT_MAX) || (errno != 0))
	{
		errno = err;
		return 0;
	}
	return (int)value;
}

int IniFile_SetKeyValueString(wIniFile* ini, const char* section, const char* key,
                              const char* value)
{
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;
	pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		pSection = IniFile_AddSection(ini, section);

	if (!pSection)
		return -1;

	pKey = IniFile_AddKey(ini, pSection, key, value);

	if (!pKey)
		return -1;

	return 1;
}

int IniFile_SetKeyValueInt(wIniFile* ini, const char* section, const char* key, int value)
{
	char strVal[128];
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;
	sprintf_s(strVal, sizeof(strVal), "%d", value);
	pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		pSection = IniFile_AddSection(ini, section);

	if (!pSection)
		return -1;

	pKey = IniFile_AddKey(ini, pSection, key, strVal);

	if (!pKey)
		return -1;

	return 1;
}

char* IniFile_WriteBuffer(wIniFile* ini)
{
	size_t i, j;
	size_t offset;
	size_t size;
	char* buffer;
	wIniFileKey* key;
	wIniFileSection* section;
	size = 0;

	if (!ini)
		return NULL;

	for (i = 0; i < ini->nSections; i++)
	{
		section = ini->sections[i];
		size += (strlen(section->name) + 3);

		for (j = 0; j < section->nKeys; j++)
		{
			key = section->keys[j];
			size += (strlen(key->name) + strlen(key->value) + 2);
		}

		size += 1;
	}

	size += 1;
	buffer = malloc(size + 1);

	if (!buffer)
		return NULL;

	offset = 0;

	for (i = 0; i < ini->nSections; i++)
	{
		section = ini->sections[i];
		sprintf_s(&buffer[offset], size - offset, "[%s]\n", section->name);
		offset += (strlen(section->name) + 3);

		for (j = 0; j < section->nKeys; j++)
		{
			key = section->keys[j];
			sprintf_s(&buffer[offset], size - offset, "%s=%s\n", key->name, key->value);
			offset += (strlen(key->name) + strlen(key->value) + 2);
		}

		sprintf_s(&buffer[offset], size - offset, "\n");
		offset += 1;
	}

	buffer[offset] = '\0';
	return buffer;
}

int IniFile_WriteFile(wIniFile* ini, const char* filename)
{
	size_t length;
	char* buffer;
	int ret = 1;
	buffer = IniFile_WriteBuffer(ini);

	if (!buffer)
		return -1;

	length = strlen(buffer);
	ini->readOnly = FALSE;

	if (!filename)
		filename = ini->filename;

	if (!IniFile_Open_File(ini, filename))
	{
		free(buffer);
		return -1;
	}

	if (fwrite((void*)buffer, length, 1, ini->fp) != 1)
		ret = -1;

	fclose(ini->fp);
	free(buffer);
	return ret;
}

wIniFile* IniFile_New(void)
{
	wIniFile* ini = (wIniFile*)calloc(1, sizeof(wIniFile));

	if (ini)
	{
		ini->nSections = 0;
		ini->cSections = 64;
		ini->sections = (wIniFileSection**)calloc(ini->cSections, sizeof(wIniFileSection*));

		if (!ini->sections)
		{
			free(ini);
			return NULL;
		}
	}

	return ini;
}

void IniFile_Free(wIniFile* ini)
{
	size_t index;

	if (!ini)
		return;

	free(ini->filename);

	for (index = 0; index < ini->nSections; index++)
		IniFile_Section_Free(ini->sections[index]);

	free(ini->sections);
	free(ini);
}
