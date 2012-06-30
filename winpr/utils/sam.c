/**
 * WinPR: Windows Portable Runtime
 * Security Accounts Manager (SAM)
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

#include <winpr/crt.h>
#include <winpr/sam.h>
#include <winpr/print.h>

#define WINPR_SAM_FILE		"/etc/winpr/SAM"

WINPR_SAM* SamOpen(BOOL read_only)
{
	WINPR_SAM* sam;

	sam = (WINPR_SAM*) malloc(sizeof(WINPR_SAM));

	if (sam != NULL)
	{
		sam->read_only = read_only;

		if (sam->read_only)
		{
			sam->fp = fopen(WINPR_SAM_FILE, "r");
		}
		else
		{
			sam->fp = fopen(WINPR_SAM_FILE, "r+");

			if (!sam->fp)
				sam->fp = fopen(WINPR_SAM_FILE, "w+");
		}
	}

	return sam;
}

void SamLookupStart(WINPR_SAM* sam)
{
	long int file_size;

	fseek(sam->fp, 0, SEEK_END);
	file_size = ftell(sam->fp);
	fseek(sam->fp, 0, SEEK_SET);

	if (file_size < 1)
		return;

	sam->buffer = (char*) malloc(file_size + 2);

	if (fread(sam->buffer, file_size, 1, sam->fp) != 1)
	{
		free(sam->buffer);
		return;
	}

	sam->buffer[file_size] = '\n';
	sam->buffer[file_size + 1] = '\0';

	sam->line = strtok(sam->buffer, "\n");
}

void SamLookupFinish(WINPR_SAM* sam)
{
	free(sam->buffer);

	sam->buffer = NULL;
	sam->line = NULL;
}

void HexStrToBin(char* str, BYTE* bin, int length)
{
	int i;

	CharUpperBuffA(str, length * 2);

	for (i = 0; i < length; i++)
	{
		bin[i] = 0;

		if ((str[i * 2] >= '0') && (str[i * 2] <= '9'))
			bin[i] |= (str[i * 2] - '0') << 4;

		if ((str[i * 2] >= 'A') && (str[i * 2] <= 'F'))
			bin[i] |= (str[i * 2] - 'A' + 10) << 4;

		if ((str[i * 2 + 1] >= '0') && (str[i * 2 + 1] <= '9'))
			bin[i] |= (str[i * 2 + 1] - '0');

		if ((str[i * 2 + 1] >= 'A') && (str[i * 2 + 1] <= 'F'))
			bin[i] |= (str[i * 2 + 1] - 'A' + 10);
	}
}

WINPR_SAM_ENTRY* SamReadEntry(WINPR_SAM* sam, WINPR_SAM_ENTRY* entry)
{
	char* p[5];
	int LmHashLength;
	int NtHashLength;

	p[0] = sam->line;
	p[1] = strchr(p[0], ':') + 1;
	p[2] = strchr(p[1], ':') + 1;
	p[3] = strchr(p[2], ':') + 1;
	p[4] = p[0] + strlen(p[0]);

	entry->UserLength = p[1] - p[0] - 1;
	entry->DomainLength = p[2] - p[1] - 1;

	LmHashLength = p[3] - p[2] - 1;
	NtHashLength = p[4] - p[3];

	entry->User = (LPSTR) malloc(entry->UserLength + 1);
	memcpy(entry->User, p[0], entry->UserLength);
	entry->User[entry->UserLength] = '\0';

	if (entry->DomainLength > 0)
	{
		entry->Domain = (LPSTR) malloc(entry->DomainLength + 1);
		memcpy(entry->Domain, p[1], entry->DomainLength);
		entry->Domain[entry->DomainLength] = '\0';
	}

	if (LmHashLength == 32)
	{
		HexStrToBin(p[2], (BYTE*) entry->LmHash, 16);
	}

	if (NtHashLength == 32)
	{
		HexStrToBin(p[3], (BYTE*) entry->NtHash, 16);
	}

	return entry;
}

void SamFreeEntry(WINPR_SAM* sam, WINPR_SAM_ENTRY* entry)
{
	if (entry)
	{
		if (entry->UserLength > 0)
			free(entry->User);

		if (entry->DomainLength > 0)
			free(entry->Domain);

		free(entry);
	}
}

WINPR_SAM_ENTRY* SamLookupUserA(WINPR_SAM* sam, LPSTR User, UINT32 UserLength, LPSTR Domain, UINT32 DomainLength)
{
	int length;
	BOOL found = 0;
	WINPR_SAM_ENTRY* entry;

	entry = (WINPR_SAM_ENTRY*) malloc(sizeof(WINPR_SAM_ENTRY));

	SamLookupStart(sam);

	while (sam->line != NULL)
	{
		length = strlen(sam->line);

		if (length > 1)
		{
			if (sam->line[0] != '#')
			{
				SamReadEntry(sam, entry);

				if (strcmp(User, entry->User) == 0)
				{
					found = 1;
					break;
				}
			}
		}

		sam->line = strtok(NULL, "\n");
	}

	SamLookupFinish(sam);

	if (!found)
	{
		free(entry);
		return NULL;
	}

	return entry;
}

WINPR_SAM_ENTRY* SamLookupUserW(WINPR_SAM* sam, LPWSTR User, UINT32 UserLength, LPWSTR Domain, UINT32 DomainLength)
{
	int length;
	BOOL found = 0;
	LPWSTR EntryUser;
	UINT32 EntryUserLength;
	WINPR_SAM_ENTRY* entry;

	entry = (WINPR_SAM_ENTRY*) malloc(sizeof(WINPR_SAM_ENTRY));

	SamLookupStart(sam);

	while (sam->line != NULL)
	{
		length = strlen(sam->line);

		if (length > 1)
		{
			if (sam->line[0] != '#')
			{
				entry = SamReadEntry(sam, entry);

				EntryUserLength = strlen(entry->User) * 2;
				EntryUser = (LPWSTR) malloc(EntryUserLength + 2);
				MultiByteToWideChar(CP_ACP, 0, entry->User, EntryUserLength / 2,
						(LPWSTR) EntryUser, EntryUserLength / 2);

				if (UserLength == EntryUserLength)
				{
					if (memcmp(User, EntryUser, UserLength) == 0)
					{
						found = 1;
						break;
					}
				}
			}
		}

		sam->line = strtok(NULL, "\n");
	}

	SamLookupFinish(sam);

	if (!found)
	{
		free(entry);
		return NULL;
	}

	return entry;
}

void SamClose(WINPR_SAM* sam)
{
	if (sam != NULL)
	{
		fclose(sam->fp);
		free(sam);
	}
}
/* Modeline for vim. Don't delete */
/* vim: set cindent:noet:sw=8:ts=8 */
