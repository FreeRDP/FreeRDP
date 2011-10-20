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

void update_pointer_position(rdpUpdate* update, POINTER_POSITION_UPDATE* pointer_position)
{

}

void update_pointer_system(rdpUpdate* update, POINTER_SYSTEM_UPDATE* pointer_system)
{

}

void update_pointer_color(rdpUpdate* update, POINTER_COLOR_UPDATE* pointer_color)
{

}

void update_pointer_new(rdpUpdate* update, POINTER_NEW_UPDATE* pointer_new)
{
	rdpPointer* pointer;
	rdpCache* cache = (rdpCache*) update->context->cache;

	pointer = Pointer_Alloc(update->context);

	if (pointer != NULL)
	{
		pointer->xorBpp = pointer_new->xorBpp;
		pointer->xPos = pointer_new->colorPtrAttr.xPos;
		pointer->yPos = pointer_new->colorPtrAttr.yPos;
		pointer->width = pointer_new->colorPtrAttr.width;
		pointer->height = pointer_new->colorPtrAttr.height;
		pointer->lengthAndMask = pointer_new->colorPtrAttr.lengthAndMask;
		pointer->lengthXorMask = pointer_new->colorPtrAttr.lengthXorMask;
		pointer->xorMaskData = pointer_new->colorPtrAttr.xorMaskData;
		pointer->andMaskData = pointer_new->colorPtrAttr.andMaskData;

		pointer->New(update->context, pointer);
		pointer_cache_put(cache->pointer, pointer_new->colorPtrAttr.cacheIndex, pointer);
		Pointer_Set(update->context, pointer);
	}
}

void update_pointer_cached(rdpUpdate* update, POINTER_CACHED_UPDATE* pointer_cached)
{
	rdpPointer* pointer;
	rdpCache* cache = (rdpCache*) update->context->cache;

	pointer = pointer_cache_get(cache->pointer, pointer_cached->cacheIndex);
	Pointer_Set(update->context, pointer);
}

rdpPointer* pointer_cache_get(rdpPointerCache* pointer_cache, uint16 index)
{
	rdpPointer* pointer;

	if (index >= pointer_cache->cacheSize)
	{
		printf("invalid pointer index:%d\n", index);
		return NULL;
	}

	pointer = pointer_cache->entries[index];

	return pointer;
}

void pointer_cache_put(rdpPointerCache* pointer_cache, uint16 index, rdpPointer* pointer)
{
	if (index >= pointer_cache->cacheSize)
	{
		printf("invalid pointer index:%d\n", index);
		return;
	}

	pointer_cache->entries[index] = pointer;
}

void pointer_cache_register_callbacks(rdpUpdate* update)
{
	update->PointerPosition = update_pointer_position;
	update->PointerSystem = update_pointer_system;
	update->PointerColor = update_pointer_color;
	update->PointerNew = update_pointer_new;
	update->PointerCached = update_pointer_cached;
}

rdpPointerCache* pointer_cache_new(rdpSettings* settings)
{
	rdpPointerCache* pointer_cache;

	pointer_cache = (rdpPointerCache*) xzalloc(sizeof(rdpPointerCache));

	if (pointer_cache != NULL)
	{
		pointer_cache->settings = settings;
		pointer_cache->cacheSize = settings->pointer_cache_size;
		pointer_cache->update = ((freerdp*) settings->instance)->update;
		pointer_cache->entries = xzalloc(sizeof(rdpPointer**) * pointer_cache->cacheSize);
	}

	return pointer_cache;
}

void pointer_cache_free(rdpPointerCache* pointer_cache)
{
	if (pointer_cache != NULL)
	{
		int i;
		rdpPointer* pointer;

		for (i = 0; i < pointer_cache->cacheSize; i++)
		{
			pointer = pointer_cache->entries[i];

			if (pointer != NULL)
			{
				pointer->Free(pointer_cache->update->context, pointer);

				if (pointer->xorMaskData != NULL)
					xfree(pointer->xorMaskData);

				if (pointer->andMaskData != NULL)
					xfree(pointer->andMaskData);

				xfree(pointer);
			}
		}

		xfree(pointer_cache->entries);
		xfree(pointer_cache);
	}
}
