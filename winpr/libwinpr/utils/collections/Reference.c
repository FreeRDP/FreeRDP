/**
 * WinPR: Windows Portable Runtime
 * Reference Count Table
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

#include <winpr/config.h>

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>

#include <winpr/collections.h>

/**
 * C reference counting
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms693431/
 */

static wReference* ReferenceTable_FindEntry(wReferenceTable* referenceTable, void* ptr)
{
	UINT32 index = 0;
	BOOL found = FALSE;
	wReference* reference = NULL;

	for (index = 0; index < referenceTable->size; index++)
	{
		reference = &referenceTable->array[index];

		if (reference->Pointer == ptr)
			found = TRUE;
	}

	return (found) ? reference : NULL;
}

static wReference* ReferenceTable_GetFreeEntry(wReferenceTable* referenceTable)
{
	UINT32 index = 0;
	BOOL found = FALSE;
	wReference* reference = NULL;

	for (index = 0; index < referenceTable->size; index++)
	{
		reference = &referenceTable->array[index];

		if (reference->Pointer == NULL)
		{
			reference->Count = 0;
			found = TRUE;
		}
	}

	if (!found)
	{
		UINT32 new_size;
		wReference* new_ref;

		if (!referenceTable->size)
		{
			free(referenceTable->array);
			referenceTable->array = NULL;
			return NULL;
		}

		new_size = referenceTable->size * 2;
		new_ref = (wReference*)realloc(referenceTable->array, sizeof(wReference) * new_size);
		if (!new_ref)
			return NULL;

		referenceTable->size = new_size;
		referenceTable->array = new_ref;
		ZeroMemory(&referenceTable->array[(referenceTable->size / 2)],
		           sizeof(wReference) * (referenceTable->size / 2));

		return ReferenceTable_GetFreeEntry(referenceTable);
	}

	return reference;
}

UINT32 ReferenceTable_Add(wReferenceTable* referenceTable, void* ptr)
{
	UINT32 count = 0;
	wReference* reference = NULL;

	if (referenceTable->synchronized)
		EnterCriticalSection(&referenceTable->lock);

	reference = ReferenceTable_FindEntry(referenceTable, ptr);

	if (!reference)
	{
		reference = ReferenceTable_GetFreeEntry(referenceTable);
		reference->Pointer = ptr;
		reference->Count = 0;
	}

	count = ++(reference->Count);

	if (referenceTable->synchronized)
		LeaveCriticalSection(&referenceTable->lock);

	return count;
}

UINT32 ReferenceTable_Release(wReferenceTable* referenceTable, void* ptr)
{
	UINT32 count = 0;
	wReference* reference = NULL;

	if (referenceTable->synchronized)
		EnterCriticalSection(&referenceTable->lock);

	reference = ReferenceTable_FindEntry(referenceTable, ptr);

	if (reference)
	{
		count = --(reference->Count);

		if (count < 1)
		{
			if (referenceTable->ReferenceFree)
			{
				referenceTable->ReferenceFree(referenceTable->context, ptr);
				reference->Pointer = NULL;
				reference->Count = 0;
			}
		}
	}

	if (referenceTable->synchronized)
		LeaveCriticalSection(&referenceTable->lock);

	return count;
}

wReferenceTable* ReferenceTable_New(BOOL synchronized, void* context, REFERENCE_FREE ReferenceFree)
{
	wReferenceTable* referenceTable;

	referenceTable = (wReferenceTable*)calloc(1, sizeof(wReferenceTable));
	if (!referenceTable)
		return NULL;

	referenceTable->context = context;
	referenceTable->ReferenceFree = ReferenceFree;

	referenceTable->size = 32;

	referenceTable->array = (wReference*)calloc(referenceTable->size, sizeof(wReference));
	if (!referenceTable->array)
		goto error_array;

	referenceTable->synchronized = synchronized;
	if (synchronized && !InitializeCriticalSectionAndSpinCount(&referenceTable->lock, 4000))
		goto error_critical_section;

	return referenceTable;

error_critical_section:
	free(referenceTable->array);
error_array:
	free(referenceTable);
	return NULL;
}

void ReferenceTable_Free(wReferenceTable* referenceTable)
{
	if (referenceTable)
	{
		if (referenceTable->synchronized)
			DeleteCriticalSection(&referenceTable->lock);

		DeleteCriticalSection(&referenceTable->lock);
		free(referenceTable->array);
		free(referenceTable);
	}
}
