/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * String utilities.
 *
 * Copyright 2018 Pascal J. Bourguignon <pjb@informatimago.com>
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

#include <string.h>
#include <winpr/string.h>
#include <freerdp/log.h>

#include "str.h"

#define TAG FREERDP_TAG("str")

/* Note WCHAR are 2-byte characters. wchar_t may be bigger! */


WCHAR * towide(BYTE* string)
{
	WCHAR * wstring = 0;
	ConvertToUnicode(CP_UTF8, 0, (LPCSTR)string, -1, (LPWSTR*)&wstring, 0);
	return wstring;
}

char*	tochar(WCHAR* string)
{
	char*	utf8 = 0;
	ConvertFromUnicode(CP_UTF8, 0, string, -1, (CHAR**) &utf8, 0, NULL, NULL);
	return utf8;
}


int compare(BOOL widechar, void* string, void* other_string)
{
	if (widechar)
	{
		WCHAR * wother = towide(other_string);
		if (wother)
		{
			WCHAR * a = (WCHAR *)string;
			WCHAR * b = wother;
			int result;
			while ((*a != 0) && (*a == *b))
			{
				a ++;
				b ++;
			}
			result = (*a == *b)? 0 : ((*a < *b)? -1 : +1);
			free(wother);
			return result;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		int result = strcmp((char *)string, (char *)other_string);
		return (result == 0)? 0 : ((result < 0)? -1 : +1);
	}
}

int ncompare(BOOL widechar, void* string, void* other_string, int max)
{
	if (widechar)
	{
		WCHAR * wother = towide(other_string);
		if (wother)
		{
			WCHAR * a = (WCHAR *)string;
			WCHAR * b = wother;
			int result;
			while ((0 < max) && (*a != 0) && (*a == *b))
			{
				a ++;
				b ++;
				max --;
			}
			result = ((max == 0) || (*a == *b)) ? 0 : ((*a < *b)? -1 : +1);
			free(wother);
			return result;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		int result = strncmp((char *)string, (char *)other_string, max);
		return (result == 0)? 0 : ((result < 0)? -1 : +1);
	}
}

BOOL contains(BOOL widechar, void* string, char* substring)
{
	if (widechar)
	{
		BOOL result = false;
		WCHAR * s = (WCHAR *)string;
		int l = strlen(substring);
		int e = lstrlenW((LPCWSTR)string) - l;
		int i = 0;
		while (i <= e)
		{
			if (ncompare(widechar, s + i, substring, l) == 0)
			{
				result = true;
				break;
			}
			i ++ ;
		}
		return result;
	}
	else
	{
		return 0 != strstr((char *)string, (char *)substring);
	}
}


void ncopy(BOOL widechar, void* destination, void* source, int count)
{
	if (widechar)
	{
		WCHAR * d = (WCHAR *)destination;
		WCHAR * s = (WCHAR *)source;
		while (0 < count)
		{
			* d ++  = * s ++ ;
			count -- ;
		}
	}
	else
	{
		char * d = (char *)destination;
		char * s = (char *)source;
		while (0 < count)
		{
			* d ++  = * s ++ ;
			count -- ;
		}
	}
}

BOOL LinkedList_StringHasSubstring(BOOL widechar, void* string, wLinkedList* list)
{
	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		if (contains(widechar, string, LinkedList_Enumerator_Current(list)))
		{
			return TRUE;
		}
	}

	return FALSE;
}

void mszFilterStrings(BOOL widechar, void*  mszStrings, DWORD* cchReaders, wLinkedList* substrings)
{
	if (widechar)
	{
		WCHAR* current = mszStrings;
		WCHAR* destination = current;

		while (* current != 0)
		{
			int size = lstrlenW(current) + 1;

			if (LinkedList_StringHasSubstring(widechar, (BYTE *)current, substrings))
			{
				/* Keep it */
				ncopy(widechar, destination, current, size);
				destination += size;
			}

			current += size;
		}

		ncopy(widechar, destination, current, 1);
		* cchReaders = 1 + (destination - (WCHAR*)mszStrings);
	}
	else
	{
		char* current = (char*)mszStrings;
		char* destination = current;

		while (* current != 0)
		{
			int size = strlen(current) + 1;

			if (LinkedList_StringHasSubstring(widechar, (BYTE *)current, substrings))
			{
				/* Keep it */
				ncopy(widechar, destination, current, size);
				destination += size;
			}

			current += size;
		}

		ncopy(widechar, destination, current, 1);
		* cchReaders = 1 + (destination - (char*)mszStrings);
	}
}

void mszStrings_Enumerator_Reset(mszStrings_Enumerator* enumerator, BOOL widechar, void* mszStrings)
{
	enumerator->widechar = widechar;
	enumerator->mszStrings = mszStrings;
	enumerator->state = 0;
}

BOOL mszStrings_Enumerator_MoveNext(mszStrings_Enumerator*  enumerator)
{
	if (enumerator->widechar)
	{
		if (enumerator->state == 0)
		{
			enumerator->state = enumerator->mszStrings;
		}
		else
		{
			enumerator->state = ((WCHAR *)enumerator->state) + wcslen(enumerator->state) + 1;
		}
		return *((WCHAR *)enumerator->state);
	}
	else
	{
		if (enumerator->state == 0)
		{
			enumerator->state = enumerator->mszStrings;
		}
		else
		{
			enumerator->state = ((char *)enumerator->state) + strlen(enumerator->state) + 1;
		}
		return *((char *)enumerator->state);
	}
}

void* mszStrings_Enumerator_Current(mszStrings_Enumerator*  enumerator)
{
	return enumerator->state;
}

void mszStringsPrint(FILE* output, BOOL widechar, void* mszStrings)
{
	mszStrings_Enumerator enumerator;
	mszStrings_Enumerator_Reset(&enumerator, widechar, mszStrings);

	while (mszStrings_Enumerator_MoveNext(& enumerator))
	{
		void* 	current = mszStrings_Enumerator_Current(& enumerator);

		if (widechar)
		{
			char* printable = tochar(current);
			fprintf(output, "%s\n", printable);
			free(printable);
		}
		else
		{
			fprintf(output, "%s\n", current);
		}
	}
}

void mszStringsLog(const char* prefix, BOOL widechar, void* mszStrings)
{
	if (prefix == 0)
	{
		prefix = "";
	}

	mszStrings_Enumerator enumerator;
	mszStrings_Enumerator_Reset(&enumerator, widechar, mszStrings);

	while (mszStrings_Enumerator_MoveNext(& enumerator))
	{
		void* current = mszStrings_Enumerator_Current(& enumerator);
		if (widechar)
		{
			char* printable = tochar(current);
			WLog_DBG(TAG, "%s%s", prefix, printable);
			free(printable);
		}
		else
		{
			WLog_DBG(TAG, "%s%s", prefix, current);
		}
	}
}

int mszSize(BOOL widechar, void* mszStrings)
{
	int size = 0;
	mszStrings_Enumerator enumerator;
	mszStrings_Enumerator_Reset(&enumerator, widechar, mszStrings);

	if (widechar)
	{
		while (mszStrings_Enumerator_MoveNext(& enumerator))
		{
			size += wcslen(mszStrings_Enumerator_Current(& enumerator)) + 1;
		}

		return size * sizeof(WCHAR);
	}
	else
	{
		while (mszStrings_Enumerator_MoveNext(& enumerator))
		{
			size += strlen(mszStrings_Enumerator_Current(& enumerator)) + 1;
		}

		return size;
	}
}

