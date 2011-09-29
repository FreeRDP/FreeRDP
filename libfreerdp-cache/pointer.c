/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Glyph Cache
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/cache/pointer.h>

void* pointer_get(rdpPointer* pointer, uint16 index, void** extra)
{
	void* entry;

	if (index >= pointer->cacheSize)
	{
		printf("invalid pointer index:%d\n", index);
		return NULL;
	}

	entry = pointer->entries[index].entry;

	if (extra != NULL)
		*extra = pointer->entries[index].extra;

	return entry;
}

void pointer_put(rdpPointer* pointer, uint16 index, void* entry, void* extra)
{
	if (index >= pointer->cacheSize)
	{
		printf("invalid pointer index:%d\n", index);
		return;
	}

	pointer->entries[index].entry = entry;
	pointer->entries[index].extra = extra;
}

rdpPointer* pointer_new(rdpSettings* settings)
{
	rdpPointer* pointer;

	pointer = (rdpPointer*) xzalloc(sizeof(rdpPointer));

	if (pointer != NULL)
	{
		pointer->settings = settings;
		pointer->cacheSize = settings->pointer_cache_size;
		pointer->entries = xzalloc(sizeof(POINTER_CACHE_ENTRY) * pointer->cacheSize);
	}

	return pointer;
}

void pointer_free(rdpPointer* pointer)
{
	if (pointer != NULL)
	{
		int i;

		for (i = 0; i < pointer->cacheSize; i++)
		{
			if (pointer->entries[i].entry != NULL)
				xfree(pointer->entries[i].entry);

			if (pointer->entries[i].extra != NULL)
				xfree(pointer->entries[i].extra);
		}

		xfree(pointer->entries);
		xfree(pointer);
	}
}
