/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <winpr/crt.h>

#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/cache/pointer.h>


#define TAG FREERDP_TAG("cache.pointer")

void update_pointer_position(rdpContext* context, POINTER_POSITION_UPDATE* pointer_position)
{
	Pointer_SetPosition(context, pointer_position->xPos, pointer_position->yPos);
}

void update_pointer_system(rdpContext* context, POINTER_SYSTEM_UPDATE* pointer_system)
{
	switch (pointer_system->type)
	{
		case SYSPTR_NULL:
			Pointer_SetNull(context);
			break;

		case SYSPTR_DEFAULT:
			Pointer_SetDefault(context);
			break;

		default:
			WLog_ERR(TAG,  "Unknown system pointer type (0x%08X)", pointer_system->type);
			break;
	}
}

void update_pointer_color(rdpContext* context, POINTER_COLOR_UPDATE* pointer_color)
{
	rdpPointer* pointer;
	rdpCache* cache = context->cache;

	pointer = Pointer_Alloc(context);

	if (pointer != NULL)
	{
		pointer->xorBpp = 24;
		pointer->xPos = pointer_color->xPos;
		pointer->yPos = pointer_color->yPos;
		pointer->width = pointer_color->width;
		pointer->height = pointer_color->height;
		pointer->lengthAndMask = pointer_color->lengthAndMask;
		pointer->lengthXorMask = pointer_color->lengthXorMask;

		if (pointer->lengthAndMask && pointer_color->xorMaskData)
		{
			pointer->andMaskData = (BYTE*) malloc(pointer->lengthAndMask);
			CopyMemory(pointer->andMaskData, pointer_color->andMaskData, pointer->lengthAndMask);
		}

		if (pointer->lengthXorMask && pointer_color->xorMaskData)
		{
			pointer->xorMaskData = (BYTE*) malloc(pointer->lengthXorMask);
			CopyMemory(pointer->xorMaskData, pointer_color->xorMaskData, pointer->lengthXorMask);
		}
		pointer->New(context, pointer);
		pointer_cache_put(cache->pointer, pointer_color->cacheIndex, pointer);
		Pointer_Set(context, pointer);
	}
}

void update_pointer_new(rdpContext* context, POINTER_NEW_UPDATE* pointer_new)
{
	rdpPointer* pointer;
	rdpCache* cache = context->cache;

	pointer = Pointer_Alloc(context);

	if (pointer != NULL)
	{
		pointer->xorBpp = pointer_new->xorBpp;
		pointer->xPos = pointer_new->colorPtrAttr.xPos;
		pointer->yPos = pointer_new->colorPtrAttr.yPos;
		pointer->width = pointer_new->colorPtrAttr.width;
		pointer->height = pointer_new->colorPtrAttr.height;
		pointer->lengthAndMask = pointer_new->colorPtrAttr.lengthAndMask;
		pointer->lengthXorMask = pointer_new->colorPtrAttr.lengthXorMask;

		pointer->andMaskData = pointer->xorMaskData = NULL;

		if (pointer->lengthAndMask)
		{
			pointer->andMaskData = (BYTE*) malloc(pointer->lengthAndMask);
			CopyMemory(pointer->andMaskData, pointer_new->colorPtrAttr.andMaskData, pointer->lengthAndMask);
		}

		if (pointer->lengthXorMask)
		{
			pointer->xorMaskData = (BYTE*) malloc(pointer->lengthXorMask);
			CopyMemory(pointer->xorMaskData, pointer_new->colorPtrAttr.xorMaskData, pointer->lengthXorMask);
		}

		pointer->New(context, pointer);
		pointer_cache_put(cache->pointer, pointer_new->colorPtrAttr.cacheIndex, pointer);
		Pointer_Set(context, pointer);
	}
}

void update_pointer_cached(rdpContext* context, POINTER_CACHED_UPDATE* pointer_cached)
{
	rdpPointer* pointer;
	rdpCache* cache = context->cache;

	pointer = pointer_cache_get(cache->pointer, pointer_cached->cacheIndex);

	if (pointer != NULL)
		Pointer_Set(context, pointer);
}

rdpPointer* pointer_cache_get(rdpPointerCache* pointer_cache, UINT32 index)
{
	rdpPointer* pointer;

	if (index >= pointer_cache->cacheSize)
	{
		WLog_ERR(TAG,  "invalid pointer index:%d", index);
		return NULL;
	}

	pointer = pointer_cache->entries[index];

	return pointer;
}

void pointer_cache_put(rdpPointerCache* pointer_cache, UINT32 index, rdpPointer* pointer)
{
	rdpPointer* prevPointer;

	if (index >= pointer_cache->cacheSize)
	{
		WLog_ERR(TAG,  "invalid pointer index:%d", index);
		return;
	}

	prevPointer = pointer_cache->entries[index];

	if (prevPointer != NULL)
		Pointer_Free(pointer_cache->update->context, prevPointer);

	pointer_cache->entries[index] = pointer;
}

void pointer_cache_register_callbacks(rdpUpdate* update)
{
	rdpPointerUpdate* pointer = update->pointer;

	pointer->PointerPosition = update_pointer_position;
	pointer->PointerSystem = update_pointer_system;
	pointer->PointerColor = update_pointer_color;
	pointer->PointerNew = update_pointer_new;
	pointer->PointerCached = update_pointer_cached;
}

rdpPointerCache* pointer_cache_new(rdpSettings* settings)
{
	rdpPointerCache* pointer_cache;

	pointer_cache = (rdpPointerCache*) malloc(sizeof(rdpPointerCache));

	if (pointer_cache != NULL)
	{
		ZeroMemory(pointer_cache, sizeof(rdpPointerCache));

		pointer_cache->settings = settings;
		pointer_cache->cacheSize = settings->PointerCacheSize;
		pointer_cache->update = ((freerdp*) settings->instance)->update;

		pointer_cache->entries = (rdpPointer**) malloc(sizeof(rdpPointer*) * pointer_cache->cacheSize);
		ZeroMemory(pointer_cache->entries, sizeof(rdpPointer*) * pointer_cache->cacheSize);
	}

	return pointer_cache;
}

void pointer_cache_free(rdpPointerCache* pointer_cache)
{
	if (pointer_cache != NULL)
	{
		int i;
		rdpPointer* pointer;

		for (i = 0; i < (int) pointer_cache->cacheSize; i++)
		{
			pointer = pointer_cache->entries[i];

			if (pointer != NULL)
			{
				Pointer_Free(pointer_cache->update->context, pointer);
				pointer_cache->entries[i] = NULL;
			}
		}

		free(pointer_cache->entries);
		free(pointer_cache);
	}
}
