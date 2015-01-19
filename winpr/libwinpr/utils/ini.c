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

int IniFile_Load_String(wIniFile* ini, const char* iniString)
{
	long int fileSize;

	ini->line = NULL;
	ini->nextLine = NULL;
	ini->buffer = NULL;
	
	fileSize = strlen(iniString);

	if (fileSize < 1)
		return -1;

	ini->buffer = (char*) malloc(fileSize + 2);
	CopyMemory(ini->buffer, iniString, fileSize);
	
	ini->buffer[fileSize] = '\n';
	ini->buffer[fileSize + 1] = '\0';

	ini->nextLine = strtok(ini->buffer, "\n");
	
	return 1;
}

int IniFile_Load_File(wIniFile* ini, const char* filename)
{
	long int fileSize;
	
	if (ini->readOnly)
	{
		ini->fp = fopen(filename, "r");
	}
	else
	{
		ini->fp = fopen(filename, "r+");

		if (!ini->fp)
			ini->fp = fopen(filename, "w+");
	}

	if (!ini->fp)
		return -1;

	fseek(ini->fp, 0, SEEK_END);
	fileSize = ftell(ini->fp);
	fseek(ini->fp, 0, SEEK_SET);

	ini->line = NULL;
	ini->nextLine = NULL;
	ini->buffer = NULL;

	if (fileSize < 1)
		return -1;

	ini->buffer = (char*) malloc(fileSize + 2);

	if (fread(ini->buffer, fileSize, 1, ini->fp) != 1)
	{
		free(ini->buffer);
		ini->buffer = NULL;
		return -1;
	}

	ini->buffer[fileSize] = '\n';
	ini->buffer[fileSize + 1] = '\0';

	ini->nextLine = strtok(ini->buffer, "\n");
	
	return 1;
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
	ini->nextLine = strtok(NULL, "\n");
	ini->lineLength = strlen(ini->line);

	return ini->line;
}

wIniFileKey* IniFile_Key_New(const char* name, const char* value)
{
	wIniFileKey* key = malloc(sizeof(wIniFileKey));

	if (key)
	{
		key->name = _strdup(name);
		key->value = _strdup(value);
	}

	return key;
}

void IniFile_Key_Free(wIniFileKey* key)
{
	if (key)
	{
		free(key->name);
		free(key->value);
	}
}

wIniFileSection* IniFile_Section_New(const char* name)
{
	wIniFileSection* section = malloc(sizeof(wIniFileSection));

	section->name = _strdup(name);

	section->nKeys = 0;
	section->cKeys = 64;
	section->keys = (wIniFileKey**) malloc(sizeof(wIniFileKey*) * section->cKeys);

	return section;
}

void IniFile_Section_Free(wIniFileSection* section)
{
	if (section)
	{
		free(section);
	}
}

int IniFile_AddSection(wIniFile* ini, const char* name)
{
	if ((ini->nSections + 1) >= (ini->cSections))
	{
		ini->cSections *= 2;
		ini->sections = (wIniFileSection**) realloc(ini->sections, sizeof(wIniFileSection*) * ini->cSections);
	}

	ini->sections[ini->nSections] = IniFile_Section_New(name);
	ini->nSections++;

	return 1;
}

int IniFile_AddKey(wIniFile* ini, wIniFileSection* section, const char* name, const char* value)
{
	if (!section)
		return -1;
	
	if ((section->nKeys + 1) >= (section->cKeys))
	{
		section->cKeys *= 2;
		section->keys = (wIniFileKey**) realloc(section->keys, sizeof(wIniFileKey*) * section->cKeys);
	}

	section->keys[section->nKeys] = IniFile_Key_New(name, value);
	section->nKeys++;

	return 1;
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

			IniFile_AddKey(ini, section, name, value);
			key = section->keys[section->nKeys - 1];
		}
	}

	IniFile_Load_Finish(ini);

	return 1;
}

int IniFile_ParseString(wIniFile* ini, const char* iniString)
{
	int status;
	
	ini->readOnly = TRUE;
	ini->filename = NULL;
	
	status = IniFile_Load_String(ini, iniString);
	
	if (status < 0)
		return status;
	
	status = IniFile_Load(ini);

	return status;
}

int IniFile_Parse(wIniFile* ini, const char* filename)
{
	int status;
	
	ini->readOnly = TRUE;
	ini->filename = _strdup(filename);

	status = IniFile_Load_File(ini, ini->filename);
	
	if (status < 0)
		return status;
	
	status = IniFile_Load(ini);

	return status;
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
		nameLength = strlen(section->name);
		length += (nameLength + 1);
	}
	
	sectionNames = (char**) malloc(length);
	p = (char*) &((BYTE*) sectionNames)[sizeof(char*) * ini->nSections];
	
	for (index = 0; index < ini->nSections; index++)
	{
		sectionNames[index] = p;
		section = ini->sections[index];
		nameLength = strlen(section->name);
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
		nameLength = strlen(pKey->name);
		length += (nameLength + 1);
	}
	
	keyNames = (char**) malloc(length);
	p = (char*) &((BYTE*) keyNames)[sizeof(char*) * pSection->nKeys];
	
	for (index = 0; index < pSection->nKeys; index++)
	{
		keyNames[index] = p;
		pKey = pSection->keys[index];
		nameLength = strlen(pKey->name);
		CopyMemory(p, pKey->name, nameLength + 1);
		p += (nameLength + 1);
	}

	*p = '\0';

	*count = pSection->nKeys;
	
	return keyNames;
}

char* IniFile_GetKeyValueString(wIniFile* ini, const char* section, const char* key)
{
	char* value = NULL;
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;
	
	pSection = IniFile_GetSection(ini, section);
	
	if (!pSection)
		return NULL;
	
	pKey = IniFile_GetKey(ini, pSection, key);
	
	if (!pKey)
		return NULL;
	
	value = pKey->value;
	
	return value;
}

UINT32 IniFile_GetKeyValueInt(wIniFile* ini, const char* section, const char* key)
{
	UINT32 value = 0;
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

wIniFile* IniFile_New()
{
	wIniFile* ini = (wIniFile*) calloc(1, sizeof(wIniFile));

	if (ini)
	{
		ini->nSections = 0;
		ini->cSections = 64;
		ini->sections = (wIniFileSection**) malloc(sizeof(wIniFileSection) * ini->cSections);
	}

	return ini;
}

void IniFile_Free(wIniFile* ini)
{
	if (ini)
	{
		free(ini->filename);

		free(ini);
	}
}
