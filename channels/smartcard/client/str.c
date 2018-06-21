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

struct string_funs string_funs[2] =
{
	{asize, aref, aset, alen, ainc, aconvert},
	{wsize, wref, wset, wlen, winc, wconvert}
};

int asize()
{
	return sizeof(BYTE);
}

int aref(BYTE* string, int index)
{
	return string[index];
}

void aset(BYTE* string, int index, int character)
{
	string[index] = character;
}

int alen(BYTE* string)
{
	return lstrlenA((LPCSTR)string);
}

BYTE* ainc(BYTE* string, int increment)
{
	return string + sizeof(BYTE) * increment;
}

char*	aconvert(BYTE* string)
{
	return strdup((char*)string);
}

int wsize()
{
	return sizeof(WCHAR);
}

int wref(BYTE* string, int index)
{
	return ((WCHAR*)string)[index];
}

void wset(BYTE* string, int index, int character)
{
	((WCHAR*)string)[index] = character;
}

int wlen(BYTE* string)
{
	return lstrlenW((LPCWSTR)string);
}

BYTE* winc(BYTE* string, int increment)
{
	return string + sizeof(WCHAR) * increment;
}

char*	wconvert(BYTE* string)
{
	char*	utf8 = 0;
	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)string, -1, (CHAR**) &utf8, 0, NULL, NULL);
	return utf8;
}


int compare(struct string_funs* funs, BYTE* string, BYTE* other_string)
{
	int i = 0;

	while (1)
	{
		if (funs->ref(string, i) == 0)
		{
			return (other_string[i] == 0) ? 0 : -1;
		}

		if (other_string[i] == 0)
		{
			return 1;
		}

		if (funs->ref(string, i) != other_string[i])
		{
			return (funs->ref(string, i) <	other_string[i]) ? -1 : 1;
		}

		i ++ ;
	}
}

int ncompare(struct string_funs* funs, BYTE* string, BYTE* other_string, int max)
{
	int i = 0;

	for (i = 0; i < max; i ++)
	{
		if (funs->ref(string, i) == 0)
		{
			return (other_string[i] == 0) ? 0 : -1;
		}

		if (other_string[i] == 0)
		{
			return 1;
		}

		if (funs->ref(string, i) != other_string[i])
		{
			return (funs->ref(string, i) <	other_string[i]) ? -1 : 1;
		}
	}

	return 0;
}

BOOL contains(struct string_funs* funs, BYTE* string, BYTE* substring)
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


void ncopy(struct string_funs* funs, BYTE* destination, BYTE* source, int count)
{
	int i;

	for (i = 0; i < count; i ++)
	{
		funs->set(destination, i, funs->ref(source, i));
	}
}

BOOL LinkedList_StringHasSubstring(struct string_funs* funs, BYTE* string, wLinkedList* list)
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
	struct string_funs* funs = & string_funs[widechar ? 1 : 0];
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
	struct string_funs* funs = & string_funs[widechar ? 1 : 0];
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
	struct string_funs* funs = & string_funs[widechar ? 1 : 0];
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
	struct string_funs* funs = & string_funs[widechar ? 1 : 0];
	mszStrings_Enumerator enumerator;
	mszStrings_Enumerator_Reset(&enumerator, widechar, mszStrings);

	while (mszStrings_Enumerator_MoveNext(& enumerator))
	{
		size += funs->len(mszStrings_Enumerator_Current(& enumerator)) + 1;
	}

	return size * funs->size();
}

