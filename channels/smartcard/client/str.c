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

#include <winpr/string.h>

#include <freerdp/log.h>

#include "str.h"

#define TAG FREERDP_TAG("str")

WCHAR * towide(BYTE* string)
{
	WCHAR * wstring = 0;
	ConvertToUnicode(CP_UTF8, 0, string, -1, &wstring, 0, NULL, NULL);
	return wstring;
}



int compareA(BYTE* string, BYTE* other_string)
{
	return strcmp((char *)string, (char *)other_string);
}

int compareW(BYTE* string, BYTE* other_string)
{
	return strcmp((char *)string, (char *)other_string);
}

int compare(BOOL widechar, BYTE* string, BYTE* other_string)
{
	if (widechar)
	{
		WCHAR * wother = towide(other_string);
		if (wother)
		{
			int result =  compareW((WCHAR *)string, wother);
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
		return compareA(string, other_string);
	}
}

int ncompareA(BYTE* string, BYTE* other_string, int max)
{
	return strncmp((char *)string, (char *)other_string, max);
}

int ncompareW(WCHAR * string, WCHAR * other_string, int max)
{
	return wcsncmp(string, other_string, max;)
}

int ncompare(BOOL widechar, BYTE* string, BYTE* other_string, int max)
{
	if (widechar)
	{
		WCHAR * wother = towide(other_string);
		if (wother)
		{
			int result = ncompareW((WCHAR *)string, wother, max);
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
		return ncompareA(string, other_string, max);
	}
}

BOOL containsA(BYTE* string, BYTE* substring)
{
	int wlen = funs->len(string);
	int slen = strlen((char*)substring);
	int end = wlen - slen;
	int i = 0;

	for (i = 0; i <= end; i ++)
	{
		if (ncompare(funs, funs->inc(string, i), substring, slen) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL contains(BOOL widechar, BYTE* string, BYTE* substring)
{
	int wlen = funs->len(string);
	int slen = strlen((char*)substring);
	int end = wlen - slen;
	int i = 0;

	for (i = 0; i <= end; i ++)
	{
		if (ncompare(funs, funs->inc(string, i), substring, slen) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}


void ncopy(BOOL widechar, BYTE* destination, BYTE* source, int count)
{
	int i;

	for (i = 0; i < count; i ++)
	{
		funs->set(destination, i, funs->ref(source, i));
	}
}

BOOL LinkedList_StringHasSubstring(BOOL widechar, BYTE* string, wLinkedList* list)
{
	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		if (contains(funs, string, LinkedList_Enumerator_Current(list)))
		{
			return TRUE;
		}
	}

	return FALSE;
}

void mszFilterStrings(BOOL widechar, void*   mszStrings, DWORD* cchReaders, wLinkedList* substrings)
{
	BOOL widechar = & string_funs[widechar ? 1 : 0];
	BYTE* current = (BYTE*)mszStrings;
	BYTE* destination = current;

	while (funs->ref(current, 0))
	{
		int size = funs->len(current) + 1;

		if (LinkedList_StringHasSubstring(funs, current, substrings))
		{
			/* Keep it */
			ncopy(funs, destination, current, size);
			destination = funs->inc(destination, size);
		}

		current = funs->inc(current, size);
	}

	ncopy(funs, destination, current, 1);
	* cchReaders = 1 + ((BYTE*)destination - (BYTE*)mszStrings) / funs->size();
}

void mszStrings_Enumerator_Reset(mszStrings_Enumerator* enumerator, BOOL widechar, void* mszStrings)
{
	enumerator->widechar = widechar;
	enumerator->mszStrings = mszStrings;
	enumerator->state = 0;
}

BOOL mszStrings_Enumerator_MoveNext(mszStrings_Enumerator*  enumerator)
{
	struct string_funs*   funs = &string_funs[enumerator->widechar ? 1 : 0];

	if (enumerator->state == 0)
	{
		enumerator->state = enumerator->mszStrings;
	}
	else
	{
		enumerator->state = funs->inc(enumerator->state, funs->len(enumerator->state) + 1);
	}

	return funs->ref(enumerator->state, 0);
}

void* mszStrings_Enumerator_Current(mszStrings_Enumerator*  enumerator)
{
	return enumerator->state;
}

void mszStringsPrint(FILE* output, BOOL widechar, void* mszStrings)
{
	BOOL widechar = & string_funs[widechar ? 1 : 0];
	mszStrings_Enumerator enumerator;
	mszStrings_Enumerator_Reset(&enumerator, widechar, mszStrings);

	while (mszStrings_Enumerator_MoveNext(& enumerator))
	{
		void* 	current = mszStrings_Enumerator_Current(& enumerator);
		char* printable = funs->convert(current);
		fprintf(output, "%s\n", printable);
		free(printable);
	}
}

void mszStringsLog(const char* prefix, BOOL widechar, void* mszStrings)
{
	BOOL widechar = & string_funs[widechar ? 1 : 0];
	mszStrings_Enumerator enumerator;

	if (prefix == 0)
	{
		prefix = "";
	}

	mszStrings_Enumerator_Reset(&enumerator, widechar, mszStrings);

	while (mszStrings_Enumerator_MoveNext(& enumerator))
	{
		void* current = mszStrings_Enumerator_Current(& enumerator);
		char* printable = funs->convert(current);
		WLog_DBG(TAG, "%s%s", prefix, printable);
		free(printable);
	}
}

int mszSize(BOOL widechar, void* mszStrings)
{
	int size = 0;
	mszStrings_Enumerator enumerator;
	mszStrings_Enumerator_Reset(&enumerator, widechar, mszStrings);

	while (mszStrings_Enumerator_MoveNext(& enumerator))
	{
		size += funs->len(mszStrings_Enumerator_Current(& enumerator)) + 1;
	}

	return size * funs->size();
}

