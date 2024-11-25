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

#include <winpr/config.h>
#include <winpr/assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/file.h>

#include <winpr/ini.h>

typedef struct
{
	char* name;
	char* value;
} wIniFileKey;

typedef struct
{
	char* name;
	size_t nKeys;
	size_t cKeys;
	wIniFileKey** keys;
} wIniFileSection;

struct s_wIniFile
{
	char* line;
	char* nextLine;
	size_t lineLength;
	char* tokctx;
	char* buffer;
	size_t buffersize;
	char* filename;
	BOOL readOnly;
	size_t nSections;
	size_t cSections;
	wIniFileSection** sections;
};

static BOOL IniFile_Load_NextLine(wIniFile* ini, char* str)
{
	size_t length = 0;

	WINPR_ASSERT(ini);

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

static BOOL IniFile_BufferResize(wIniFile* ini, size_t size)
{
	WINPR_ASSERT(ini);
	if (size > ini->buffersize)
	{
		const size_t diff = size - ini->buffersize;
		char* tmp = realloc(ini->buffer, size);
		if (!tmp)
			return FALSE;

		memset(&tmp[ini->buffersize], 0, diff * sizeof(char));
		ini->buffer = tmp;
		ini->buffersize = size;
	}
	return TRUE;
}

static BOOL IniFile_Load_String(wIniFile* ini, const char* iniString)
{
	size_t fileSize = 0;

	WINPR_ASSERT(ini);

	if (!iniString)
		return FALSE;

	ini->line = NULL;
	ini->nextLine = NULL;
	fileSize = strlen(iniString);

	if (fileSize < 1)
		return FALSE;

	if (!IniFile_BufferResize(ini, fileSize + 2))
		return FALSE;

	CopyMemory(ini->buffer, iniString, fileSize);
	ini->buffer[fileSize] = '\n';
	IniFile_Load_NextLine(ini, ini->buffer);
	return TRUE;
}

static void IniFile_Close_File(FILE* fp)
{
	if (fp)
		(void)fclose(fp);
}

static FILE* IniFile_Open_File(wIniFile* ini, const char* filename)
{
	WINPR_ASSERT(ini);

	if (!filename)
		return FALSE;

	if (ini->readOnly)
		return winpr_fopen(filename, "rb");
	else
		return winpr_fopen(filename, "w+b");
}

static BOOL IniFile_Load_File(wIniFile* ini, const char* filename)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(ini);

	FILE* fp = IniFile_Open_File(ini, filename);
	if (!fp)
		return FALSE;

	if (_fseeki64(fp, 0, SEEK_END) < 0)
		goto out_file;

	const INT64 fileSize = _ftelli64(fp);

	if (fileSize < 0)
		goto out_file;

	if (_fseeki64(fp, 0, SEEK_SET) < 0)
		goto out_file;

	ini->line = NULL;
	ini->nextLine = NULL;

	if (fileSize < 1)
		goto out_file;

	if (!IniFile_BufferResize(ini, (size_t)fileSize + 2))
		goto out_file;

	if (fread(ini->buffer, (size_t)fileSize, 1ul, fp) != 1)
		goto out_file;

	ini->buffer[fileSize] = '\n';
	IniFile_Load_NextLine(ini, ini->buffer);
	rc = TRUE;

out_file:
	IniFile_Close_File(fp);
	return rc;
}

static BOOL IniFile_Load_HasNextLine(wIniFile* ini)
{
	WINPR_ASSERT(ini);

	return (ini->nextLine) ? TRUE : FALSE;
}

static char* IniFile_Load_GetNextLine(wIniFile* ini)
{
	WINPR_ASSERT(ini);

	ini->line = ini->nextLine;
	ini->lineLength = strlen(ini->line);
	IniFile_Load_NextLine(ini, NULL);
	return ini->line;
}

static void IniFile_Key_Free(wIniFileKey* key)
{
	if (!key)
		return;

	free(key->name);
	free(key->value);
	free(key);
}

static wIniFileKey* IniFile_Key_New(const char* name, const char* value)
{
	if (!name || !value)
		return NULL;

	wIniFileKey* key = calloc(1, sizeof(wIniFileKey));

	if (key)
	{
		key->name = _strdup(name);
		key->value = _strdup(value);

		if (!key->name || !key->value)
		{
			IniFile_Key_Free(key);
			return NULL;
		}
	}

	return key;
}

static void IniFile_Section_Free(wIniFileSection* section)
{
	if (!section)
		return;

	free(section->name);

	for (size_t index = 0; index < section->nKeys; index++)
	{
		IniFile_Key_Free(section->keys[index]);
	}

	free((void*)section->keys);
	free(section);
}

static BOOL IniFile_SectionKeysResize(wIniFileSection* section, size_t count)
{
	WINPR_ASSERT(section);

	if (section->nKeys + count >= section->cKeys)
	{
		const size_t new_size = section->cKeys + count + 1024;
		const size_t diff = new_size - section->cKeys;
		wIniFileKey** new_keys =
		    (wIniFileKey**)realloc((void*)section->keys, sizeof(wIniFileKey*) * new_size);

		if (!new_keys)
			return FALSE;

		memset((void*)&new_keys[section->cKeys], 0, diff * sizeof(wIniFileKey*));
		section->cKeys = new_size;
		section->keys = new_keys;
	}
	return TRUE;
}

static wIniFileSection* IniFile_Section_New(const char* name)
{
	if (!name)
		return NULL;

	wIniFileSection* section = calloc(1, sizeof(wIniFileSection));

	if (!section)
		goto fail;

	section->name = _strdup(name);

	if (!section->name)
		goto fail;

	if (!IniFile_SectionKeysResize(section, 64))
		goto fail;

	return section;

fail:
	IniFile_Section_Free(section);
	return NULL;
}

static wIniFileSection* IniFile_GetSection(wIniFile* ini, const char* name)
{
	wIniFileSection* section = NULL;

	WINPR_ASSERT(ini);

	if (!name)
		return NULL;

	for (size_t index = 0; index < ini->nSections; index++)
	{
		if (_stricmp(name, ini->sections[index]->name) == 0)
		{
			section = ini->sections[index];
			break;
		}
	}

	return section;
}

static BOOL IniFile_SectionResize(wIniFile* ini, size_t count)
{
	WINPR_ASSERT(ini);

	if (ini->nSections + count >= ini->cSections)
	{
		const size_t new_size = ini->cSections + count + 1024;
		const size_t diff = new_size - ini->cSections;
		wIniFileSection** new_sect =
		    (wIniFileSection**)realloc((void*)ini->sections, sizeof(wIniFileSection*) * new_size);

		if (!new_sect)
			return FALSE;

		memset((void*)&new_sect[ini->cSections], 0, diff * sizeof(wIniFileSection*));
		ini->cSections = new_size;
		ini->sections = new_sect;
	}
	return TRUE;
}

static wIniFileSection* IniFile_AddToSection(wIniFile* ini, const char* name)
{
	WINPR_ASSERT(ini);

	if (!name)
		return NULL;

	wIniFileSection* section = IniFile_GetSection(ini, name);

	if (!section)
	{
		if (!IniFile_SectionResize(ini, 1))
			return NULL;

		section = IniFile_Section_New(name);
		if (!section)
			return NULL;
		ini->sections[ini->nSections++] = section;
	}

	return section;
}

static wIniFileKey* IniFile_GetKey(wIniFileSection* section, const char* name)
{
	wIniFileKey* key = NULL;

	WINPR_ASSERT(section);

	if (!name)
		return NULL;

	for (size_t index = 0; index < section->nKeys; index++)
	{
		if (_stricmp(name, section->keys[index]->name) == 0)
		{
			key = section->keys[index];
			break;
		}
	}

	return key;
}

static wIniFileKey* IniFile_AddKey(wIniFileSection* section, const char* name, const char* value)
{
	WINPR_ASSERT(section);

	if (!name || !value)
		return NULL;

	wIniFileKey* key = IniFile_GetKey(section, name);

	if (!key)
	{
		if (!IniFile_SectionKeysResize(section, 1))
			return NULL;

		key = IniFile_Key_New(name, value);

		if (!key)
			return NULL;

		section->keys[section->nKeys++] = key;
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
	char* name = NULL;
	char* value = NULL;
	char* separator = NULL;
	char* beg = NULL;
	char* end = NULL;
	wIniFileSection* section = NULL;

	WINPR_ASSERT(ini);

	while (IniFile_Load_HasNextLine(ini))
	{
		char* line = IniFile_Load_GetNextLine(ini);

		if (line[0] == ';')
			continue;

		if (line[0] == '[')
		{
			beg = &line[1];
			end = strchr(line, ']');

			if (!end)
				return -1;

			*end = '\0';
			IniFile_AddToSection(ini, beg);
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

			if (!IniFile_AddKey(section, name, value))
				return -1;
		}
	}

	return 1;
}

static BOOL IniFile_SetFilename(wIniFile* ini, const char* name)
{
	WINPR_ASSERT(ini);
	free(ini->filename);
	ini->filename = NULL;

	if (!name)
		return TRUE;
	ini->filename = _strdup(name);
	return ini->filename != NULL;
}

int IniFile_ReadBuffer(wIniFile* ini, const char* buffer)
{
	BOOL status = 0;

	WINPR_ASSERT(ini);

	if (!buffer)
		return -1;

	ini->readOnly = TRUE;
	status = IniFile_Load_String(ini, buffer);

	if (!status)
		return -1;

	return IniFile_Load(ini);
}

int IniFile_ReadFile(wIniFile* ini, const char* filename)
{
	WINPR_ASSERT(ini);

	ini->readOnly = TRUE;
	if (!IniFile_SetFilename(ini, filename))
		return -1;
	if (!ini->filename)
		return -1;

	if (!IniFile_Load_File(ini, filename))
		return -1;

	return IniFile_Load(ini);
}

char** IniFile_GetSectionNames(wIniFile* ini, size_t* count)
{
	WINPR_ASSERT(ini);

	if (!count)
		return NULL;

	if (ini->nSections > INT_MAX)
		return NULL;

	size_t length = (sizeof(char*) * ini->nSections) + sizeof(char);

	for (size_t index = 0; index < ini->nSections; index++)
	{
		wIniFileSection* section = ini->sections[index];
		const size_t nameLength = strlen(section->name);
		length += (nameLength + 1);
	}

	char** sectionNames = (char**)calloc(length, sizeof(char*));

	if (!sectionNames)
		return NULL;

	char* p = (char*)&((BYTE*)sectionNames)[sizeof(char*) * ini->nSections];

	for (size_t index = 0; index < ini->nSections; index++)
	{
		sectionNames[index] = p;
		wIniFileSection* section = ini->sections[index];
		const size_t nameLength = strlen(section->name);
		CopyMemory(p, section->name, nameLength + 1);
		p += (nameLength + 1);
	}

	*p = '\0';
	*count = ini->nSections;
	return sectionNames;
}

char** IniFile_GetSectionKeyNames(wIniFile* ini, const char* section, size_t* count)
{
	WINPR_ASSERT(ini);

	if (!section || !count)
		return NULL;

	wIniFileSection* pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		return NULL;

	if (pSection->nKeys > INT_MAX)
		return NULL;

	size_t length = (sizeof(char*) * pSection->nKeys) + sizeof(char);

	for (size_t index = 0; index < pSection->nKeys; index++)
	{
		wIniFileKey* pKey = pSection->keys[index];
		const size_t nameLength = strlen(pKey->name);
		length += (nameLength + 1);
	}

	char** keyNames = (char**)calloc(length, sizeof(char*));

	if (!keyNames)
		return NULL;

	char* p = (char*)&((BYTE*)keyNames)[sizeof(char*) * pSection->nKeys];

	for (size_t index = 0; index < pSection->nKeys; index++)
	{
		keyNames[index] = p;
		wIniFileKey* pKey = pSection->keys[index];
		const size_t nameLength = strlen(pKey->name);
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

	WINPR_ASSERT(ini);

	pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		return NULL;

	pKey = IniFile_GetKey(pSection, key);

	if (!pKey)
		return NULL;

	value = (const char*)pKey->value;
	return value;
}

int IniFile_GetKeyValueInt(wIniFile* ini, const char* section, const char* key)
{
	int err = 0;
	long value = 0;
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;

	WINPR_ASSERT(ini);

	pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		return 0;

	pKey = IniFile_GetKey(pSection, key);

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

	WINPR_ASSERT(ini);
	wIniFileSection* pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		pSection = IniFile_AddToSection(ini, section);

	if (!pSection)
		return -1;

	pKey = IniFile_AddKey(pSection, key, value);

	if (!pKey)
		return -1;

	return 1;
}

int IniFile_SetKeyValueInt(wIniFile* ini, const char* section, const char* key, int value)
{
	char strVal[128] = { 0 };
	wIniFileKey* pKey = NULL;
	wIniFileSection* pSection = NULL;

	WINPR_ASSERT(ini);

	(void)sprintf_s(strVal, sizeof(strVal), "%d", value);
	pSection = IniFile_GetSection(ini, section);

	if (!pSection)
		pSection = IniFile_AddToSection(ini, section);

	if (!pSection)
		return -1;

	pKey = IniFile_AddKey(pSection, key, strVal);

	if (!pKey)
		return -1;

	return 1;
}

char* IniFile_WriteBuffer(wIniFile* ini)
{
	size_t offset = 0;
	size_t size = 0;
	char* buffer = NULL;

	WINPR_ASSERT(ini);

	for (size_t i = 0; i < ini->nSections; i++)
	{
		wIniFileSection* section = ini->sections[i];
		size += (strlen(section->name) + 3);

		for (size_t j = 0; j < section->nKeys; j++)
		{
			wIniFileKey* key = section->keys[j];
			size += (strlen(key->name) + strlen(key->value) + 2);
		}

		size += 1;
	}

	size += 1;
	buffer = calloc(size + 1, sizeof(char));

	if (!buffer)
		return NULL;

	offset = 0;

	for (size_t i = 0; i < ini->nSections; i++)
	{
		wIniFileSection* section = ini->sections[i];
		(void)sprintf_s(&buffer[offset], size - offset, "[%s]\n", section->name);
		offset += (strlen(section->name) + 3);

		for (size_t j = 0; j < section->nKeys; j++)
		{
			wIniFileKey* key = section->keys[j];
			(void)sprintf_s(&buffer[offset], size - offset, "%s=%s\n", key->name, key->value);
			offset += (strlen(key->name) + strlen(key->value) + 2);
		}

		(void)sprintf_s(&buffer[offset], size - offset, "\n");
		offset += 1;
	}

	return buffer;
}

int IniFile_WriteFile(wIniFile* ini, const char* filename)
{
	int ret = -1;

	WINPR_ASSERT(ini);

	char* buffer = IniFile_WriteBuffer(ini);

	if (!buffer)
		return -1;

	const size_t length = strlen(buffer);
	ini->readOnly = FALSE;

	if (!filename)
		filename = ini->filename;

	FILE* fp = IniFile_Open_File(ini, filename);
	if (!fp)
		goto fail;

	if (fwrite((void*)buffer, length, 1, fp) != 1)
		goto fail;

	ret = 1;

fail:
	IniFile_Close_File(fp);
	free(buffer);
	return ret;
}

void IniFile_Free(wIniFile* ini)
{
	if (!ini)
		return;

	IniFile_SetFilename(ini, NULL);

	for (size_t index = 0; index < ini->nSections; index++)
		IniFile_Section_Free(ini->sections[index]);

	free((void*)ini->sections);
	free(ini->buffer);
	free(ini);
}

wIniFile* IniFile_New(void)
{
	wIniFile* ini = (wIniFile*)calloc(1, sizeof(wIniFile));

	if (!ini)
		goto fail;

	if (!IniFile_SectionResize(ini, 64))
		goto fail;

	return ini;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	IniFile_Free(ini);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

wIniFile* IniFile_Clone(const wIniFile* ini)
{
	if (!ini)
		return NULL;

	wIniFile* copy = IniFile_New();
	if (!copy)
		goto fail;

	copy->lineLength = ini->lineLength;
	if (!IniFile_SetFilename(copy, ini->filename))
		goto fail;

	if (ini->buffersize > 0)
	{
		if (!IniFile_BufferResize(copy, ini->buffersize))
			goto fail;
		memcpy(copy->buffer, ini->buffer, copy->buffersize);
	}

	copy->readOnly = ini->readOnly;

	for (size_t x = 0; x < ini->nSections; x++)
	{
		const wIniFileSection* cur = ini->sections[x];
		if (!cur)
			goto fail;

		wIniFileSection* scopy = IniFile_AddToSection(copy, cur->name);
		if (!scopy)
			goto fail;

		for (size_t y = 0; y < cur->nKeys; y++)
		{
			const wIniFileKey* key = cur->keys[y];
			if (!key)
				goto fail;

			IniFile_AddKey(scopy, key->name, key->value);
		}
	}
	return copy;

fail:
	IniFile_Free(copy);
	return NULL;
}
