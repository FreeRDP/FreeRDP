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

static BOOL pointer_cache_put(rdpPointerCache* pointer_cache, UINT32 index,
                              rdpPointer* pointer);
static const rdpPointer* pointer_cache_get(rdpPointerCache* pointer_cache,
        UINT32 index);

static void pointer_free(rdpContext* context, rdpPointer* pointer)
{
	if (pointer)
	{
		pointer->Free(context, pointer);

		if (pointer->xorMaskData)
		{
			free(pointer->xorMaskData);
			pointer->xorMaskData = NULL;
		}

		if (pointer->andMaskData)
		{
			free(pointer->andMaskData);
			pointer->andMaskData = NULL;
		}

		free(pointer);
	}
}

static BOOL update_pointer_position(rdpContext* context,
                                    const POINTER_POSITION_UPDATE* pointer_position)
{
	rdpPointer* pointer;

	if (!context || !context->graphics || !context->graphics->Pointer_Prototype
	    || !pointer_position)
		return FALSE;

	pointer = context->graphics->Pointer_Prototype;
	return pointer->SetPosition(context, pointer_position->xPos,
	                            pointer_position->yPos);
}

static BOOL update_pointer_system(rdpContext* context,
                                  const POINTER_SYSTEM_UPDATE* pointer_system)
{
	rdpPointer* pointer;

	if (!context || !context->graphics || !context->graphics->Pointer_Prototype
	    || !pointer_system)
		return FALSE;

	pointer = context->graphics->Pointer_Prototype;

	switch (pointer_system->type)
	{
		case SYSPTR_NULL:
			pointer->SetNull(context);
			break;

		case SYSPTR_DEFAULT:
			pointer->SetDefault(context);
			break;

		default:
			WLog_ERR(TAG,  "Unknown system pointer type (0x%08"PRIX32")", pointer_system->type);
	}

	return TRUE;
}

static BOOL update_pointer_color(rdpContext* context,
                                 const POINTER_COLOR_UPDATE* pointer_color)
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

			if (!pointer->andMaskData)
				goto out_fail;

			CopyMemory(pointer->andMaskData, pointer_color->andMaskData,
			           pointer->lengthAndMask);
		}

		if (pointer->lengthXorMask && pointer_color->xorMaskData)
		{
			pointer->xorMaskData = (BYTE*) malloc(pointer->lengthXorMask);

			if (!pointer->xorMaskData)
				goto out_fail;

			CopyMemory(pointer->xorMaskData, pointer_color->xorMaskData,
			           pointer->lengthXorMask);
		}

		if (!pointer->New(context, pointer))
			goto out_fail;

		if (!pointer_cache_put(cache->pointer, pointer_color->cacheIndex, pointer))
			goto out_fail;

		return pointer->Set(context, pointer);
	}

	return FALSE;
out_fail:
	pointer_free(context, pointer);
	return FALSE;
}

static BOOL update_pointer_new(rdpContext* context,
                               const POINTER_NEW_UPDATE* pointer_new)
{
	rdpPointer* pointer;
	rdpCache* cache;

	if (!context || !pointer_new)
		return FALSE;

	cache = context->cache;
	pointer = Pointer_Alloc(context);

	if (!pointer)
		return FALSE;

	pointer->xorBpp = pointer_new->xorBpp;
	pointer->xPos = pointer_new->colorPtrAttr.xPos;
	pointer->yPos = pointer_new->colorPtrAttr.yPos;
	pointer->width = pointer_new->colorPtrAttr.width;
	pointer->height = pointer_new->colorPtrAttr.height;
	pointer->lengthAndMask = pointer_new->colorPtrAttr.lengthAndMask;
	pointer->lengthXorMask = pointer_new->colorPtrAttr.lengthXorMask;

	if (pointer->lengthAndMask)
	{
		pointer->andMaskData = (BYTE*) malloc(pointer->lengthAndMask);

		if (!pointer->andMaskData)
			goto out_fail;

		CopyMemory(pointer->andMaskData, pointer_new->colorPtrAttr.andMaskData,
		           pointer->lengthAndMask);
	}

	if (pointer->lengthXorMask)
	{
		pointer->xorMaskData = (BYTE*) malloc(pointer->lengthXorMask);

		if (!pointer->xorMaskData)
			goto out_fail;

		CopyMemory(pointer->xorMaskData, pointer_new->colorPtrAttr.xorMaskData,
		           pointer->lengthXorMask);
	}

	if (!pointer->New(context, pointer))
		goto out_fail;

	if (!pointer_cache_put(cache->pointer, pointer_new->colorPtrAttr.cacheIndex,
	                       pointer))
		goto out_fail;

	return pointer->Set(context, pointer);
out_fail:
	pointer_free(context, pointer);
	return FALSE;
}

static BOOL update_pointer_cached(rdpContext* context,
                                  const POINTER_CACHED_UPDATE* pointer_cached)
{
	const rdpPointer* pointer;
	rdpCache* cache = context->cache;
	pointer = pointer_cache_get(cache->pointer, pointer_cached->cacheIndex);

	if (pointer != NULL)
	{
		pointer->Set(context, pointer);
		return TRUE;
	}

	return FALSE;
}

const rdpPointer* pointer_cache_get(rdpPointerCache* pointer_cache,
                                    UINT32 index)
{
	const rdpPointer* pointer;

	if (index >= pointer_cache->cacheSize)
	{
		WLog_ERR(TAG,  "invalid pointer index:%"PRIu32"", index);
		return NULL;
	}

	pointer = pointer_cache->entries[index];
	return pointer;
}

BOOL pointer_cache_put(rdpPointerCache* pointer_cache, UINT32 index,
                       rdpPointer* pointer)
{
	rdpPointer* prevPointer;

	if (index >= pointer_cache->cacheSize)
	{
		WLog_ERR(TAG,  "invalid pointer index:%"PRIu32"", index);
		return FALSE;
	}

	prevPointer = pointer_cache->entries[index];
	pointer_free(pointer_cache->update->context, prevPointer);
	pointer_cache->entries[index] = pointer;
	return TRUE;
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
	pointer_cache = (rdpPointerCache*) calloc(1, sizeof(rdpPointerCache));

	if (!pointer_cache)
		return NULL;

	pointer_cache->settings = settings;
	pointer_cache->cacheSize = settings->PointerCacheSize;
	pointer_cache->update = ((freerdp*) settings->instance)->update;
	pointer_cache->entries = (rdpPointer**) calloc(pointer_cache->cacheSize,
	                         sizeof(rdpPointer*));

	if (!pointer_cache->entries)
	{
		free(pointer_cache);
		return NULL;
	}

	return pointer_cache;
}

void pointer_cache_free(rdpPointerCache* pointer_cache)
{
	if (pointer_cache != NULL)
	{
		UINT32 i;
		rdpPointer* pointer;

		for (i = 0; i < pointer_cache->cacheSize; i++)
		{
			pointer = pointer_cache->entries[i];
			pointer_free(pointer_cache->update->context, pointer);
		}

		free(pointer_cache->entries);
		free(pointer_cache);
	}
}
