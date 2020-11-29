#include <assert.h>

#include <freerdp/utils/chunkstream.h>

/** @brief kind of slot **/
typedef enum
{
	CHUNK_T_STATIC, /*!< some static content */
	CHUNK_T_MALLOC, /*!< some content allocated by the caller */
	CHUNK_T_POOL    /*!< some memory taken from our pool */
} ChunkStreamSlotType;

struct _ChunkStreamSlot
{
	ChunkStreamSlotType slotType;
	BYTE* data;
	size_t allocated;
	size_t used;
};

struct _ChunkStream
{
	BYTE* pool;
	size_t availablePoolSz;
	BYTE* availPtr;
	size_t poolSize;

	ChunkStreamSlot slots[CHUNKSTREAM_MAX_SLOTS];
	size_t slotCounter;
};

size_t chunkstreamslot_allocated(const ChunkStreamSlot* slot)
{
	assert(slot);
	return slot->allocated;
}

size_t chunkstreamslot_size(const ChunkStreamSlot* slot)
{
	assert(slot);
	return slot->used;
}

BYTE* chunkstreamslot_data(const ChunkStreamSlot* slot)
{
	assert(slot);
	return slot->data;
}

BOOL chunkstreamslot_update_used(ChunkStreamSlot* slot, size_t used)
{
	assert(slot);
	if (slot->allocated < used)
		return FALSE;

	slot->used = used;
	return TRUE;
}

BOOL chunkstreamslot_update_fromStream(ChunkStreamSlot* slot, wStream* s)
{
	assert(s);
	return chunkstreamslot_update_used(slot, Stream_GetPosition(s));
}

ChunkStream* chunkstream_new(size_t initialSize)
{
	ChunkStream* ret = (ChunkStream*)calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	ret->poolSize = ret->availablePoolSz = initialSize;
	if (initialSize)
	{
		ret->pool = (BYTE*)calloc(initialSize, 1);
		if (!ret->pool)
		{
			free(ret);
			return NULL;
		}

		ret->availPtr = ret->pool;
	}

	return ret;
}

void chunkstream_destroy(ChunkStream** pchunkstream)
{
	ChunkStream* chunkstream;

	assert(pchunkstream);
	chunkstream = *pchunkstream;
	if (chunkstream)
	{
		size_t i;
		for (i = 0; i < chunkstream->slotCounter; i++)
		{
			ChunkStreamSlot* slot = &chunkstream->slots[i];
			switch (slot->slotType)
			{
				case CHUNK_T_STATIC:
				case CHUNK_T_POOL:
					break;
				case CHUNK_T_MALLOC:
					free(slot->data);
					break;
			}
		}

		free(chunkstream->pool);
		free(chunkstream);
	}

	*pchunkstream = NULL;
}

static ChunkStreamSlot* allocateSlot(ChunkStream* chunkstream, ChunkStreamSlotType t)
{
	size_t slotId;
	ChunkStreamSlot* ret;

	if (chunkstream->slotCounter == CHUNKSTREAM_MAX_SLOTS)
		return NULL;

	slotId = chunkstream->slotCounter++;
	ret = &chunkstream->slots[slotId];
	ret->slotType = t;
	return ret;
}

ChunkStreamSlot* chunkstream_getStaticMemSlot(ChunkStream* chunkstream, const BYTE* data, size_t sz)
{
	ChunkStreamSlot* slot;
	assert(chunkstream);

	slot = allocateSlot(chunkstream, CHUNK_T_STATIC);
	if (!slot)
		return NULL;
	slot->allocated = 0;
	slot->used = sz;
	slot->data = (BYTE*)data;
	return slot;
}

ChunkStreamSlot* chunkstream_getStaticStringSlot(ChunkStream* chunkstream, const char* str,
                                                 BOOL include0)
{
	size_t len;

	assert(str);

	len = strlen(str);
	if (include0)
		len++;
	return chunkstream_getStaticMemSlot(chunkstream, (const BYTE*)str, len);
}

ChunkStreamSlot* chunkstream_getMallocSlot(ChunkStream* chunkstream, BYTE* data, size_t sz)
{
	ChunkStreamSlot* slot;

	assert(chunkstream);
	slot = allocateSlot(chunkstream, CHUNK_T_MALLOC);
	if (!slot)
		return NULL;

	slot->data = data;
	slot->allocated = 0;
	slot->used = sz;
	return slot;
}

static size_t computeFullSize(ChunkStream* chunkstream)
{
	size_t i;
	size_t ret = 0;
	for (i = 0; i < chunkstream->slotCounter; i++)
		ret += chunkstream->slots[i].used;
	return ret;
}

wStream* chunkstream_linearizeToStream(ChunkStream* chunkstream)
{
	wStream* ret;
	size_t i;
	size_t allocSz;

	assert(chunkstream);

	allocSz = computeFullSize(chunkstream);
	if (!allocSz)
		allocSz = 1;

	ret = Stream_New(NULL, allocSz);
	if (!ret)
		return NULL;

	for (i = 0; i < chunkstream->slotCounter; i++)
	{
		ChunkStreamSlot* slot = &chunkstream->slots[i];
		Stream_Write(ret, slot->data, slot->used);
	}

	return ret;
}

FREERDP_API BOOL chunkstream_linearizeInStream(ChunkStream* chunkstream, wStream* s)
{
	size_t i;
	size_t allocSz;

	assert(chunkstream);
	assert(s);

	allocSz = computeFullSize(chunkstream);
	if (allocSz && !Stream_EnsureRemainingCapacity(s, allocSz))
		return FALSE;

	for (i = 0; i < chunkstream->slotCounter; i++)
	{
		ChunkStreamSlot* slot = &chunkstream->slots[i];
		Stream_Write(s, slot->data, slot->used);
	}

	return TRUE;
}

int chunkstream_sizeAfterSlot(const ChunkStream* chunkstream, const ChunkStreamSlot* slot)
{
	int ret, i;

	// first pass to retrieve the slot
	for (i = 0; i < chunkstream->slotCounter; i++)
	{
		if (&chunkstream->slots[i] == slot)
			break;
	}
	if (i == chunkstream->slotCounter)
		return -1;

	ret = 0;
	for (i = i + 1; i < chunkstream->slotCounter; i++)
		ret += chunkstream->slots[i].used;

	return ret;
}

ChunkStreamSlot* chunkstream_getPoolSlot(ChunkStream* chunkstream, size_t sz)
{
	ChunkStreamSlot* slot;

	assert(chunkstream);

	if (chunkstream->availablePoolSz < sz)
		return NULL;

	slot = allocateSlot(chunkstream, CHUNK_T_POOL);
	if (!slot)
		return NULL;

	slot->data = chunkstream->availPtr;
	slot->allocated = sz;
	slot->used = 0;

	chunkstream->availPtr += sz;
	chunkstream->availablePoolSz -= sz;

	return slot;
}

ChunkStreamSlot* chunkstream_getPoolStream(ChunkStream* chunkstream, size_t sz, wStream* s)
{
	ChunkStreamSlot* slot = chunkstream_getPoolSlot(chunkstream, sz);
	if (!slot)
		return NULL;

	Stream_StaticInit(s, slot->data, sz);
	return slot;
}
