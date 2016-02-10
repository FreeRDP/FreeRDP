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

#include <winpr/crt.h>

#include <winpr/ini.h>

BOOL IniFile_Load_NextLine(wIniFile* ini, char* str)
{
	int length = 0;

	ini->nextLine = strtok_s(str, "\n", &ini->tokctx);

	if (ini->nextLine)
		length = (int) strlen(ini->nextLine);

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

int IniFile_Load_String(wIniFile* ini, const char* iniString)
{
	long int fileSize;

	ini->line = NULL;
	ini->nextLine = NULL;
	ini->buffer = NULL;
	
	fileSize = (long int) strlen(iniString);

	if (fileSize < 1)
		return -1;

	ini->buffer = (char*) malloc(fileSize + 2);

	if (!ini->buffer)
		return -1;

	CopyMemory(ini->buffer, iniString, fileSize);
	
	ini->buffer[fileSize] = '\n';
	ini->buffer[fileSize + 1] = '\0';

	IniFile_Load_NextLine(ini, ini->buffer);
	
	return 1;
}

int IniFile_Open_File(wIniFile* ini, const char* filename)
{
	if (ini->readOnly)
		ini->fp = fopen(filename, "rb");
	else
		ini->fp = fopen(filename, "w+b");

	if (!ini->fp)
		return -1;

	return 1;
}

int IniFile_Load_File(wIniFile* ini, const char* filename)
{
	int fileSize;

	if (IniFile_Open_File(ini, filename) < 0)
		return -1;

	if (fseek(ini->fp, 0, SEEK_END) < 0)
		goto out_file;

	fileSize = ftell(ini->fp);
	
	if (fileSize < 0)
		goto out_file;
	
	if (fseek(ini->fp, 0, SEEK_SET) < 0)
		goto out_file;

	ini->line = NULL;
	ini->nextLine = NULL;
	ini->buffer = NULL;

	if (fileSize < 1)
		goto out_file;

	ini->buffer = (char*) malloc(fileSize + 2);

	if (!ini->buffer)
		goto out_file;

	if (fread(ini->buffer, fileSize, 1, ini->fp) != 1)
		goto out_buffer;

	fclose(ini->fp);
	ini->fp = NULL;

	ini->buffer[fileSize] = '\n';
	ini->buffer[fileSize + 1] = '\0';

	IniFile_Load_NextLine(ini, ini->buffer);

	return 1;

out_buffer:
	free(ini->buffer);
	ini->buffer = NULL;
out_file:
	fclose(ini->fp);
	ini->fp = NULL;
	return -1;
}

void IniFile_Load_Finish(wIniFile* ini)
{
	if (!ini)
		return;

	if (ini->buffer)
	{
		free(ini->buffer);
		ini->buffer = NULL;
	}
}

BOOL IniFile_Load_HasNextLine(wIniFile* ini)
{
	if (!ini)
		return FALSE;

	return (ini->nextLine) ? TRUE : FALSE;
}

char* IniFile_Load_GetNextLine(wIniFile* ini)
{
	if (!ini)
		return NULL;

	ini->line = ini->nextLine;
	ini->lineLength = (int) strlen(ini->line);

	IniFile_Load_NextLine(ini, NULL);

	return ini->line;
}

wIniFileKey* IniFile_Key_New(const char* name, const char* value)
{
	wIniFileKey* key = malloc(sizeof(wIniFileKey));

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

void IniFile_Key_Free(wIniFileKey* key)
{
	if (!key)
		return;

	free(key->name);
	free(key->value);
	free(key);
}

wIniFileSection* IniFile_Section_New(const char* name)
{
	wIniFileSection* section = malloc(sizeof(wIniFileSection));

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
	section->keys = (wIniFileKey**) malloc(sizeof(wIniFileKey*) * section->cKeys);

	if (!section->keys)
	{
		free(section->name);
		free(section);
		return NULL;
	}

	return section;
}

void IniFile_Section_Free(wIniFileSection* section)
{
	int index;

	if (!section)
		return;

	for (index = 0; index < section->nKeys; index++)
	{
		IniFile_Key_Free(section->keys[index]);
	}

	free(section);
}

wIniFileSection* IniFile_GetSection(wIniFile* ini, const char* name)
{
	int index;
	wIniFileSection* section = NULL;

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

wIniFileSection* IniFile_AddSection(wIniFile* ini, const char* name)
{
	wIniFileSection* section;

	if (!name)
		return NULL;

	section = IniFile_GetSection(ini, name);

	if (!section)
	{
		if ((ini->nSections + 1) >= (ini->cSections))
		{
			int new_size;
			wIniFileSection** new_sect;

			new_size = ini->cSections * 2;
			new_sect = (wIniFileSection**) realloc(ini->sections, sizeof(wIniFileSection*) * new_size);
			
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

wIniFileKey* IniFile_GetKey(wIniFile* ini, wIniFileSection* section, const char* name)
{
	int index;
	wIniFileKey* key = NULL;

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

wIniFileKey* IniFile_AddKey(wIniFile* ini, wIniFileSection* section, const char* name, const char* value)
{
	wIniFileKey* key;

	if (!section || !name)
		return NULL;
	
	key = IniFile_GetKey(ini, section, name);

	if (!key)
	{
		if ((section->nKeys + 1) >= (section->cKeys))
		{
			int new_size;
			wIniFileKey** new_key;

			new_size = section->cKeys * 2;
			new_key = (wIniFileKey**) realloc(section->keys, sizeof(wIniFileKey*) * new_size);
			
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

int IniFile_Load(wIniFile* ini)
{
	char* line;
	char* name;
	char* value;
	char* separator;
	char *beg, *end;
	wIniFileKey* key = NULL;
	wIniFileSection* section = NULL;

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
			{
				return -1;
			}

			key = NULL;
			if (section && section->keys)
				key = section->keys[section->nKeys - 1];
		}
	}

	IniFile_Load_Finish(ini);

	return 1;
}

int IniFile_ReadBuffer(wIniFile* ini, const char* buffer)
{
	int status;
	
	ini->readOnly = TRUE;
	ini->filename = NULL;
	
	status = IniFile_Load_String(ini, buffer);
	
	if (status < 0)
		return status;
	
	status = IniFile_Load(ini);

	return status;
}

int IniFile_ReadFile(wIniFile* ini, const char* filename)
{
	int status;
	
	ini->readOnly = TRUE;

	free(ini->filename);
	ini->filename = _strdup(filename);

	if (!ini->filename)
		return -1;

	status = IniFile_Load_File(ini, filename);
	
	if (status < 0)
		return status;
	
	status = IniFile_Load(ini);

	return status;
}

char** IniFile_GetSectionNames(wIniFile* ini, int* count)
{
	char* p;
	int index;
	int length;
	int nameLength;
	char** sectionNames;
	wIniFileSection* section = NULL;
	
	length = (sizeof(char*) * ini->nSections) + sizeof(char);
	
	for (index = 0; index < ini->nSections; index++)
	{
		section = ini->sections[index];
		nameLength = (int) strlen(section->name);
		length += (nameLength + 1);
	}
	
	sectionNames = (char**) malloc(length);

	if (!sectionNames)
		return NULL;

	p = (char*) &((BYTE*) sectionNames)[sizeof(char*) * ini->nSections];
	
	for (index = 0; index < ini->nSections; index++)
	{
		sectionNames[index] = p;
		section = ini->sections[index];
		nameLength = (int) strlen(section->name);
		CopyMemory(p, section->name, nameLength + 1);
		p += (nameLength + 1);
	}

	*p = '\0';

	*count = ini->nSections;
	
	return sectionNames;
}

char** IniFile_GetSectionKeyNames(wIniFile* ini, const char* section, int* count)
{
	char* p;
	int index;
	int length;
	int nameLength;
	char** keyNames;
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;
	
	pSection = IniFile_GetSection(ini, section);
	
	if (!pSection)
		return NULL;
	
	length = (sizeof(char*) * pSection->nKeys) + sizeof(char);
	
	for (index = 0; index < pSection->nKeys; index++)
	{
		pKey = pSection->keys[index];
		nameLength = (int) strlen(pKey->name);
		length += (nameLength + 1);
	}
	
	keyNames = (char**) malloc(length);
	if (!keyNames)
		return NULL;

	p = (char*) &((BYTE*) keyNames)[sizeof(char*) * pSection->nKeys];
	
	for (index = 0; index < pSection->nKeys; index++)
	{
		keyNames[index] = p;
		pKey = pSection->keys[index];
		nameLength = (int) strlen(pKey->name);
		CopyMemory(p, pKey->name, nameLength + 1);
		p += (nameLength + 1);
	}

	*p = '\0';

	*count = pSection->nKeys;
	
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
	
	value = (const char*) pKey->value;
	
	return value;
}

int IniFile_GetKeyValueInt(wIniFile* ini, const char* section, const char* key)
{
	int value = 0;
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;
	
	pSection = IniFile_GetSection(ini, section);
	
	if (!pSection)
		return 0;
	
	pKey = IniFile_GetKey(ini, pSection, key);
	
	if (!pKey)
		return 0;
	
	value = atoi(pKey->value);
	
	return value;
}

int IniFile_SetKeyValueString(wIniFile* ini, const char* section, const char* key, const char* value)
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
	int i, j;
	int offset;
	int size;
	char* buffer;
	wIniFileKey* key;
	wIniFileSection* section;

	size = 0;

	for (i = 0; i < ini->nSections; i++)
	{
		section = ini->sections[i];
		size += (int) (strlen(section->name) + 3);

		for (j = 0; j < section->nKeys; j++)
		{
			key = section->keys[j];
			size += (int) (strlen(key->name) + strlen(key->value) + 2);
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
		offset += (int) (strlen(section->name) + 3);

		for (j = 0; j < section->nKeys; j++)
		{
			key = section->keys[j];
			sprintf_s(&buffer[offset], size - offset, "%s=%s\n", key->name, key->value);
			offset += (int) (strlen(key->name) + strlen(key->value) + 2);
		}

		sprintf_s(&buffer[offset], size - offset, "\n");
		offset += 1;
	}

	buffer[offset] = '\0';
	size += 1;

	return buffer;
}

int IniFile_WriteFile(wIniFile* ini, const char* filename)
{
	int length;
	char* buffer;
	int ret = 1;

	buffer = IniFile_WriteBuffer(ini);

	if (!buffer)
		return -1;

	length = (int) strlen(buffer);

	ini->readOnly = FALSE;

	if (!filename)
		filename = ini->filename;

	if (IniFile_Open_File(ini, filename) < 0)
	{
		free(buffer);
		return -1;
	}

	if (fwrite((void*) buffer, length, 1, ini->fp) != 1)
		ret = -1;

	fclose(ini->fp);

	free(buffer);

	return ret;
}

wIniFile* IniFile_New()
{
	wIniFile* ini = (wIniFile*) calloc(1, sizeof(wIniFile));

	if (ini)
	{
		ini->nSections = 0;
		ini->cSections = 64;
		ini->sections = (wIniFileSection**) calloc(ini->cSections, sizeof(wIniFileSection*));

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
	int index;

	if (!ini)
		return;

	free(ini->filename);

	for (index = 0; index < ini->nSections; index++)
	{
		IniFile_Section_Free(ini->sections[index]);
	}

	free(ini->sections);

	free(ini);
}
