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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/sam.h>
#include <winpr/print.h>

#include "../log.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#define WINPR_SAM_FILE "C:\\SAM"
#else
#define WINPR_SAM_FILE "/etc/winpr/SAM"
#endif
#define TAG WINPR_TAG("utils")

struct winpr_sam
{
	FILE* fp;
	char* line;
	char* buffer;
	char* context;
	BOOL readOnly;
};

WINPR_SAM* SamOpen(const char* filename, BOOL readOnly)
{
	FILE* fp = NULL;
	WINPR_SAM* sam = NULL;

	if (!filename)
		filename = WINPR_SAM_FILE;

	if (readOnly)
		fp = fopen(filename, "r");
	else
	{
		fp = fopen(filename, "r+");

		if (!fp)
			fp = fopen(filename, "w+");
	}

	if (fp)
	{
		sam = (WINPR_SAM*)malloc(sizeof(WINPR_SAM));

		if (!sam)
		{
			fclose(fp);
			return NULL;
		}

		sam->readOnly = readOnly;
		sam->fp = fp;
	}
	else
	{
		WLog_DBG(TAG, "Could not open SAM file!");
		return NULL;
	}

	return sam;
}

static BOOL SamLookupStart(WINPR_SAM* sam)
{
	size_t readSize;
	INT64 fileSize;

	if (!sam || !sam->fp)
		return FALSE;

	_fseeki64(sam->fp, 0, SEEK_END);
	fileSize = _ftelli64(sam->fp);
	_fseeki64(sam->fp, 0, SEEK_SET);

	if (fileSize < 1)
		return FALSE;

	sam->context = NULL;
	sam->buffer = (char*)malloc(fileSize + 2);

	if (!sam->buffer)
		return FALSE;

	readSize = fread(sam->buffer, fileSize, 1, sam->fp);

	if (!readSize)
	{
		if (!ferror(sam->fp))
			readSize = fileSize;
	}

	if (readSize < 1)
	{
		free(sam->buffer);
		sam->buffer = NULL;
		return FALSE;
	}

	sam->buffer[fileSize] = '\n';
	sam->buffer[fileSize + 1] = '\0';
	sam->line = strtok_s(sam->buffer, "\n", &sam->context);
	return TRUE;
}

static void SamLookupFinish(WINPR_SAM* sam)
{
	free(sam->buffer);
	sam->buffer = NULL;
	sam->line = NULL;
}

static void HexStrToBin(char* str, BYTE* bin, int length)
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

static BOOL SamReadEntry(WINPR_SAM* sam, WINPR_SAM_ENTRY* entry)
{
	char* p[5];
	size_t LmHashLength;
	size_t NtHashLength;
	size_t count = 0;
	char* cur;

	if (!sam || !entry || !sam->line)
		return FALSE;

	cur = sam->line;

	while ((cur = strchr(cur, ':')) != NULL)
	{
		count++;
		cur++;
	}

	if (count < 4)
		return FALSE;

	p[0] = sam->line;
	p[1] = strchr(p[0], ':') + 1;
	p[2] = strchr(p[1], ':') + 1;
	p[3] = strchr(p[2], ':') + 1;
	p[4] = strchr(p[3], ':') + 1;
	LmHashLength = (p[3] - p[2] - 1);
	NtHashLength = (p[4] - p[3] - 1);

	if ((LmHashLength != 0) && (LmHashLength != 32))
		return FALSE;

	if ((NtHashLength != 0) && (NtHashLength != 32))
		return FALSE;

	entry->UserLength = (UINT32)(p[1] - p[0] - 1);
	entry->User = (LPSTR)malloc(entry->UserLength + 1);

	if (!entry->User)
		return FALSE;

	entry->User[entry->UserLength] = '\0';
	entry->DomainLength = (UINT32)(p[2] - p[1] - 1);
	memcpy(entry->User, p[0], entry->UserLength);

	if (entry->DomainLength > 0)
	{
		entry->Domain = (LPSTR)malloc(entry->DomainLength + 1);

		if (!entry->Domain)
		{
			free(entry->User);
			entry->User = NULL;
			return FALSE;
		}

		memcpy(entry->Domain, p[1], entry->DomainLength);
		entry->Domain[entry->DomainLength] = '\0';
	}
	else
		entry->Domain = NULL;

	if (LmHashLength == 32)
		HexStrToBin(p[2], (BYTE*)entry->LmHash, 16);

	if (NtHashLength == 32)
		HexStrToBin(p[3], (BYTE*)entry->NtHash, 16);

	return TRUE;
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

void SamResetEntry(WINPR_SAM_ENTRY* entry)
{
	if (!entry)
		return;

	if (entry->UserLength)
	{
		free(entry->User);
		entry->User = NULL;
	}

	if (entry->DomainLength)
	{
		free(entry->Domain);
		entry->Domain = NULL;
	}

	ZeroMemory(entry->LmHash, sizeof(entry->LmHash));
	ZeroMemory(entry->NtHash, sizeof(entry->NtHash));
}

WINPR_SAM_ENTRY* SamLookupUserA(WINPR_SAM* sam, LPSTR User, UINT32 UserLength, LPSTR Domain,
                                UINT32 DomainLength)
{
	size_t length;
	BOOL found = FALSE;
	WINPR_SAM_ENTRY* entry;
	entry = (WINPR_SAM_ENTRY*)calloc(1, sizeof(WINPR_SAM_ENTRY));

	if (!entry)
		return NULL;

	if (!SamLookupStart(sam))
	{
		free(entry);
		return NULL;
	}

	while (sam->line != NULL)
	{
		length = strlen(sam->line);

		if (length > 1)
		{
			if (sam->line[0] != '#')
			{
				if (!SamReadEntry(sam, entry))
				{
					goto out_fail;
				}

				if (strcmp(User, entry->User) == 0)
				{
					found = 1;
					break;
				}
			}
		}

		SamResetEntry(entry);
		sam->line = strtok_s(NULL, "\n", &sam->context);
	}

out_fail:
	SamLookupFinish(sam);

	if (!found)
	{
		free(entry);
		return NULL;
	}

	return entry;
}

WINPR_SAM_ENTRY* SamLookupUserW(WINPR_SAM* sam, LPWSTR User, UINT32 UserLength, LPWSTR Domain,
                                UINT32 DomainLength)
{
	size_t length;
	BOOL Found = FALSE;
	BOOL UserMatch;
	BOOL DomainMatch;
	LPWSTR EntryUser = NULL;
	UINT32 EntryUserLength;
	LPWSTR EntryDomain = NULL;
	UINT32 EntryDomainLength;
	WINPR_SAM_ENTRY* entry;

	if (!(entry = (WINPR_SAM_ENTRY*)calloc(1, sizeof(WINPR_SAM_ENTRY))))
		return NULL;

	if (!SamLookupStart(sam))
	{
		free(entry);
		return NULL;
	}

	while (sam->line != NULL)
	{
		length = strlen(sam->line);

		if (length > 1)
		{
			if (sam->line[0] != '#')
			{
				DomainMatch = 0;
				UserMatch = 0;

				if (!SamReadEntry(sam, entry))
					goto out_fail;

				if (DomainLength > 0)
				{
					if (entry->DomainLength > 0)
					{
						EntryDomainLength = (UINT32)strlen(entry->Domain) * 2;
						EntryDomain = (LPWSTR)malloc(EntryDomainLength + 2);

						if (!EntryDomain)
							goto out_fail;

						MultiByteToWideChar(CP_ACP, 0, entry->Domain, EntryDomainLength / 2,
						                    (LPWSTR)EntryDomain, EntryDomainLength / 2);

						if (DomainLength == EntryDomainLength)
						{
							if (memcmp(Domain, EntryDomain, DomainLength) == 0)
							{
								DomainMatch = 1;
							}
						}

						free(EntryDomain);
					}
					else
					{
						DomainMatch = 0;
					}
				}
				else
				{
					DomainMatch = 1;
				}

				if (DomainMatch)
				{
					EntryUserLength = (UINT32)strlen(entry->User) * 2;
					EntryUser = (LPWSTR)malloc(EntryUserLength + 2);

					if (!EntryUser)
						goto out_fail;

					MultiByteToWideChar(CP_ACP, 0, entry->User, EntryUserLength / 2,
					                    (LPWSTR)EntryUser, EntryUserLength / 2);

					if (UserLength == EntryUserLength)
					{
						if (memcmp(User, EntryUser, UserLength) == 0)
						{
							UserMatch = 1;
						}
					}

					free(EntryUser);

					if (UserMatch)
					{
						Found = TRUE;
						break;
					}
				}
			}
		}

		SamResetEntry(entry);
		sam->line = strtok_s(NULL, "\n", sam->context);
	}

out_fail:
	SamLookupFinish(sam);

	if (!Found)
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
