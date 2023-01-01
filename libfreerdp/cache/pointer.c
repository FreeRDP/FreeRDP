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

#include <freerdp/config.h>

#include <stdio.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/cache/pointer.h>

#include "pointer.h"

#define TAG FREERDP_TAG("cache.pointer")

static BOOL pointer_cache_put(rdpPointerCache* pointer_cache, UINT32 index, rdpPointer* pointer,
                              BOOL colorCache);
static rdpPointer* pointer_cache_get(rdpPointerCache* pointer_cache, UINT32 index);

static void pointer_clear(rdpPointer* pointer)
{
	if (pointer)
	{
		pointer->lengthAndMask = 0;
		free(pointer->andMaskData);
		pointer->andMaskData = NULL;

		pointer->lengthXorMask = 0;
		free(pointer->xorMaskData);
		pointer->xorMaskData = NULL;
	}
}

static void pointer_free(rdpContext* context, rdpPointer* pointer)
{
	if (pointer)
	{
		IFCALL(pointer->Free, context, pointer);
		pointer_clear(pointer);
	}
	free(pointer);
}

static BOOL update_pointer_position(rdpContext* context,
                                    const POINTER_POSITION_UPDATE* pointer_position)
{
	rdpPointer* pointer;
	BOOL GrabMouse;

	if (!context || !context->graphics || !context->graphics->Pointer_Prototype ||
	    !pointer_position)
		return FALSE;

	GrabMouse = freerdp_settings_get_bool(context->settings, FreeRDP_GrabMouse);
	if (!GrabMouse)
		return TRUE;

	pointer = context->graphics->Pointer_Prototype;
	return IFCALLRESULT(TRUE, pointer->SetPosition, context, pointer_position->xPos,
	                    pointer_position->yPos);
}

static BOOL update_pointer_system(rdpContext* context, const POINTER_SYSTEM_UPDATE* pointer_system)
{
	rdpPointer* pointer;

	if (!context || !context->graphics || !context->graphics->Pointer_Prototype || !pointer_system)
		return FALSE;

	pointer = context->graphics->Pointer_Prototype;

	switch (pointer_system->type)
	{
		case SYSPTR_NULL:
			return IFCALLRESULT(TRUE, pointer->SetNull, context);

		case SYSPTR_DEFAULT:
			return IFCALLRESULT(TRUE, pointer->SetDefault, context);

		default:
			WLog_ERR(TAG, "Unknown system pointer type (0x%08" PRIX32 ")", pointer_system->type);
	}
	return TRUE;
}

static BOOL upate_pointer_copy_andxor(rdpPointer* pointer, const BYTE* andMaskData,
                                      size_t lengthAndMask, const BYTE* xorMaskData,
                                      size_t lengthXorMask)
{
	WINPR_ASSERT(pointer);

	pointer_clear(pointer);
	if (lengthAndMask && andMaskData)
	{
		pointer->lengthAndMask = lengthAndMask;
		pointer->andMaskData = (BYTE*)malloc(lengthAndMask);
		if (!pointer->andMaskData)
			return FALSE;

		CopyMemory(pointer->andMaskData, andMaskData, lengthAndMask);
	}

	if (lengthXorMask && xorMaskData)
	{
		pointer->lengthXorMask = lengthXorMask;
		pointer->xorMaskData = (BYTE*)malloc(lengthXorMask);
		if (!pointer->xorMaskData)
			return FALSE;

		CopyMemory(pointer->xorMaskData, xorMaskData, lengthXorMask);
	}

	return TRUE;
}

static BOOL update_pointer_color(rdpContext* context, const POINTER_COLOR_UPDATE* pointer_color)
{
	rdpPointer* pointer;
	rdpCache* cache;

	WINPR_ASSERT(context);
	WINPR_ASSERT(pointer_color);

	cache = context->cache;
	WINPR_ASSERT(cache);

	pointer = Pointer_Alloc(context);

	if (pointer == NULL)
		return FALSE;
	pointer->xorBpp = 24;
	pointer->xPos = pointer_color->xPos;
	pointer->yPos = pointer_color->yPos;
	pointer->width = pointer_color->width;
	pointer->height = pointer_color->height;

	if (!upate_pointer_copy_andxor(pointer, pointer_color->andMaskData,
	                               pointer_color->lengthAndMask, pointer_color->xorMaskData,
	                               pointer_color->lengthXorMask))
		goto out_fail;

	if (!IFCALLRESULT(TRUE, pointer->New, context, pointer))
		goto out_fail;

	if (!pointer_cache_put(cache->pointer, pointer_color->cacheIndex, pointer, TRUE))
		goto out_fail;

	return IFCALLRESULT(TRUE, pointer->Set, context, pointer);

out_fail:
	pointer_free(context, pointer);
	return FALSE;
}

static BOOL update_pointer_large(rdpContext* context, const POINTER_LARGE_UPDATE* pointer_large)
{
	rdpPointer* pointer;
	rdpCache* cache;

	WINPR_ASSERT(context);
	WINPR_ASSERT(pointer_large);

	cache = context->cache;
	WINPR_ASSERT(cache);

	pointer = Pointer_Alloc(context);
	if (pointer == NULL)
		return FALSE;
	pointer->xorBpp = pointer_large->xorBpp;
	pointer->xPos = pointer_large->hotSpotX;
	pointer->yPos = pointer_large->hotSpotY;
	pointer->width = pointer_large->width;
	pointer->height = pointer_large->height;

	if (!upate_pointer_copy_andxor(pointer, pointer_large->andMaskData,
	                               pointer_large->lengthAndMask, pointer_large->xorMaskData,
	                               pointer_large->lengthXorMask))
		goto out_fail;

	if (!IFCALLRESULT(TRUE, pointer->New, context, pointer))
		goto out_fail;

	if (!pointer_cache_put(cache->pointer, pointer_large->cacheIndex, pointer, FALSE))
		goto out_fail;

	return IFCALLRESULT(TRUE, pointer->Set, context, pointer);

out_fail:
	pointer_free(context, pointer);
	return FALSE;
}

static BOOL update_pointer_new(rdpContext* context, const POINTER_NEW_UPDATE* pointer_new)
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
	if (!upate_pointer_copy_andxor(
	        pointer, pointer_new->colorPtrAttr.andMaskData, pointer_new->colorPtrAttr.lengthAndMask,
	        pointer_new->colorPtrAttr.xorMaskData, pointer_new->colorPtrAttr.lengthXorMask))
		goto out_fail;

	if (!IFCALLRESULT(TRUE, pointer->New, context, pointer))
		goto out_fail;

	if (!pointer_cache_put(cache->pointer, pointer_new->colorPtrAttr.cacheIndex, pointer, FALSE))
		goto out_fail;

	return IFCALLRESULT(TRUE, pointer->Set, context, pointer);

out_fail:
	pointer_free(context, pointer);
	return FALSE;
}

static BOOL update_pointer_cached(rdpContext* context, const POINTER_CACHED_UPDATE* pointer_cached)
{
	rdpPointer* pointer;
	rdpCache* cache;

	WINPR_ASSERT(context);
	WINPR_ASSERT(pointer_cached);

	cache = context->cache;
	WINPR_ASSERT(cache);

	pointer = pointer_cache_get(cache->pointer, pointer_cached->cacheIndex);

	if (pointer != NULL)
		return IFCALLRESULT(TRUE, pointer->Set, context, pointer);

	return FALSE;
}

rdpPointer* pointer_cache_get(rdpPointerCache* pointer_cache, UINT32 index)
{
	rdpPointer* pointer;

	WINPR_ASSERT(pointer_cache);

	if (index >= pointer_cache->cacheSize)
	{
		WLog_ERR(TAG, "invalid pointer index:%" PRIu32 " [%" PRIu32 "]", index,
		         pointer_cache->cacheSize);
		return NULL;
	}

	WINPR_ASSERT(pointer_cache->entries);
	pointer = pointer_cache->entries[index];
	return pointer;
}

BOOL pointer_cache_put(rdpPointerCache* pointer_cache, UINT32 index, rdpPointer* pointer,
                       BOOL colorCache)
{
	rdpPointer* prevPointer;
	const size_t id = colorCache ? FreeRDP_ColorPointerCacheSize : FreeRDP_PointerCacheSize;

	WINPR_ASSERT(pointer_cache);
	WINPR_ASSERT(pointer_cache->context);

	const UINT32 size = freerdp_settings_get_uint32(pointer_cache->context->settings, id);
	if (index >= pointer_cache->cacheSize)
	{
		WLog_ERR(TAG,
		         "invalid pointer index:%" PRIu32 " [allocated %" PRIu32 ", %s size %" PRIu32 "]",
		         index, pointer_cache->cacheSize,
		         colorCache ? "color-pointer-cache" : "pointer-cache", size);
		return FALSE;
	}
	if (index >= size)
	{
		WLog_WARN(TAG,
		          "suspicious pointer index:%" PRIu32 " [allocated %" PRIu32 ", %s size %" PRIu32
		          "]",
		          index, pointer_cache->cacheSize,
		          colorCache ? "color-pointer-cache" : "pointer-cache", size);
	}

	WINPR_ASSERT(pointer_cache->entries);
	prevPointer = pointer_cache->entries[index];
	pointer_free(pointer_cache->context, prevPointer);
	pointer_cache->entries[index] = pointer;
	return TRUE;
}

void pointer_cache_register_callbacks(rdpUpdate* update)
{
	rdpPointerUpdate* pointer;

	WINPR_ASSERT(update);
	WINPR_ASSERT(update->context);

	pointer = update->pointer;
	WINPR_ASSERT(pointer);

	if (!freerdp_settings_get_bool(update->context->settings, FreeRDP_DeactivateClientDecoding))
	{
		pointer->PointerPosition = update_pointer_position;
		pointer->PointerSystem = update_pointer_system;
		pointer->PointerColor = update_pointer_color;
		pointer->PointerLarge = update_pointer_large;
		pointer->PointerNew = update_pointer_new;
		pointer->PointerCached = update_pointer_cached;
	}
}

rdpPointerCache* pointer_cache_new(rdpContext* context)
{
	rdpPointerCache* pointer_cache;
	rdpSettings* settings;

	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	pointer_cache = (rdpPointerCache*)calloc(1, sizeof(rdpPointerCache));

	if (!pointer_cache)
		return NULL;

	pointer_cache->context = context;

	/* seen invalid pointer cache requests by mstsc (off by 1) so we ensure the cache entry size
	 * matches */
	const UINT32 size = freerdp_settings_get_uint32(settings, FreeRDP_PointerCacheSize);
	const UINT32 colorSize = freerdp_settings_get_uint32(settings, FreeRDP_ColorPointerCacheSize);
	pointer_cache->cacheSize = MAX(size, colorSize) + 1;

	pointer_cache->entries = (rdpPointer**)calloc(pointer_cache->cacheSize, sizeof(rdpPointer*));

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

		if (pointer_cache->entries)
		{
			for (i = 0; i < pointer_cache->cacheSize; i++)
			{
				pointer = pointer_cache->entries[i];
				pointer_free(pointer_cache->context, pointer);
			}
		}

		free(pointer_cache->entries);
		free(pointer_cache);
	}
}

POINTER_COLOR_UPDATE* copy_pointer_color_update(rdpContext* context,
                                                const POINTER_COLOR_UPDATE* src)
{
	POINTER_COLOR_UPDATE* dst = calloc(1, sizeof(POINTER_COLOR_UPDATE));

	if (!dst || !src)
		goto fail;

	*dst = *src;

	if (src->lengthAndMask > 0)
	{
		dst->andMaskData = calloc(src->lengthAndMask, sizeof(BYTE));

		if (!dst->andMaskData)
			goto fail;

		memcpy(dst->andMaskData, src->andMaskData, src->lengthAndMask);
	}

	if (src->lengthXorMask > 0)
	{
		dst->xorMaskData = calloc(src->lengthXorMask, sizeof(BYTE));

		if (!dst->xorMaskData)
			goto fail;

		memcpy(dst->xorMaskData, src->xorMaskData, src->lengthXorMask);
	}

	return dst;
fail:
	free_pointer_color_update(context, dst);
	return NULL;
}

void free_pointer_color_update(rdpContext* context, POINTER_COLOR_UPDATE* pointer)
{
	WINPR_UNUSED(context);

	if (!pointer)
		return;

	free(pointer->xorMaskData);
	free(pointer->andMaskData);
	free(pointer);
}

POINTER_LARGE_UPDATE* copy_pointer_large_update(rdpContext* context,
                                                const POINTER_LARGE_UPDATE* src)
{
	POINTER_LARGE_UPDATE* dst = calloc(1, sizeof(POINTER_LARGE_UPDATE));

	if (!dst || !src)
		goto fail;

	*dst = *src;

	if (src->lengthAndMask > 0)
	{
		dst->andMaskData = calloc(src->lengthAndMask, sizeof(BYTE));

		if (!dst->andMaskData)
			goto fail;

		memcpy(dst->andMaskData, src->andMaskData, src->lengthAndMask);
	}

	if (src->lengthXorMask > 0)
	{
		dst->xorMaskData = calloc(src->lengthXorMask, sizeof(BYTE));

		if (!dst->xorMaskData)
			goto fail;

		memcpy(dst->xorMaskData, src->xorMaskData, src->lengthXorMask);
	}

	return dst;
fail:
	free_pointer_large_update(context, dst);
	return NULL;
}

void free_pointer_large_update(rdpContext* context, POINTER_LARGE_UPDATE* pointer)
{
	WINPR_UNUSED(context);
	if (!pointer)
		return;

	free(pointer->xorMaskData);
	free(pointer->andMaskData);
	free(pointer);
}

POINTER_NEW_UPDATE* copy_pointer_new_update(rdpContext* context, const POINTER_NEW_UPDATE* src)
{
	POINTER_NEW_UPDATE* dst = calloc(1, sizeof(POINTER_NEW_UPDATE));

	if (!dst || !src)
		goto fail;

	*dst = *src;

	if (src->colorPtrAttr.lengthAndMask > 0)
	{
		dst->colorPtrAttr.andMaskData = calloc(src->colorPtrAttr.lengthAndMask, sizeof(BYTE));

		if (!dst->colorPtrAttr.andMaskData)
			goto fail;

		memcpy(dst->colorPtrAttr.andMaskData, src->colorPtrAttr.andMaskData,
		       src->colorPtrAttr.lengthAndMask);
	}

	if (src->colorPtrAttr.lengthXorMask > 0)
	{
		dst->colorPtrAttr.xorMaskData = calloc(src->colorPtrAttr.lengthXorMask, sizeof(BYTE));

		if (!dst->colorPtrAttr.xorMaskData)
			goto fail;

		memcpy(dst->colorPtrAttr.xorMaskData, src->colorPtrAttr.xorMaskData,
		       src->colorPtrAttr.lengthXorMask);
	}

	return dst;
fail:
	free_pointer_new_update(context, dst);
	return NULL;
}

void free_pointer_new_update(rdpContext* context, POINTER_NEW_UPDATE* pointer)
{
	if (!pointer)
		return;

	free(pointer->colorPtrAttr.xorMaskData);
	free(pointer->colorPtrAttr.andMaskData);
	free(pointer);
}

POINTER_CACHED_UPDATE* copy_pointer_cached_update(rdpContext* context,
                                                  const POINTER_CACHED_UPDATE* pointer)
{
	POINTER_CACHED_UPDATE* dst = calloc(1, sizeof(POINTER_CACHED_UPDATE));

	if (!dst)
		goto fail;

	*dst = *pointer;
	return dst;
fail:
	free_pointer_cached_update(context, dst);
	return NULL;
}

void free_pointer_cached_update(rdpContext* context, POINTER_CACHED_UPDATE* pointer)
{
	WINPR_UNUSED(context);
	free(pointer);
}

void free_pointer_position_update(rdpContext* context, POINTER_POSITION_UPDATE* pointer)
{
	WINPR_UNUSED(context);
	free(pointer);
}

POINTER_POSITION_UPDATE* copy_pointer_position_update(rdpContext* context,
                                                      const POINTER_POSITION_UPDATE* pointer)
{
	POINTER_POSITION_UPDATE* dst = calloc(1, sizeof(POINTER_POSITION_UPDATE));

	if (!dst || !pointer)
		goto fail;

	*dst = *pointer;
	return dst;
fail:
	free_pointer_position_update(context, dst);
	return NULL;
}

void free_pointer_system_update(rdpContext* context, POINTER_SYSTEM_UPDATE* pointer)
{
	WINPR_UNUSED(context);
	free(pointer);
}

POINTER_SYSTEM_UPDATE* copy_pointer_system_update(rdpContext* context,
                                                  const POINTER_SYSTEM_UPDATE* pointer)
{
	POINTER_SYSTEM_UPDATE* dst = calloc(1, sizeof(POINTER_SYSTEM_UPDATE));

	if (!dst || !pointer)
		goto fail;

	*dst = *pointer;
	return dst;
fail:
	free_pointer_system_update(context, dst);
	return NULL;
}
