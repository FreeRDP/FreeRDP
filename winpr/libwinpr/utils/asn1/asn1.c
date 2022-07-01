/**
 * WinPR: Windows Portable Runtime
 * ASN1 routines
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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

#include <winpr/asn1.h>
#include <winpr/wlog.h>
#include <winpr/crt.h>

#define TAG "winpr.asn1"

typedef struct
{
	size_t poolOffset;
	size_t capacity;
	size_t used;
} Asn1Chunk;

#define MAX_STATIC_ITEMS 50

/** @brief type of encoder container */
typedef enum
{
	ASN1_CONTAINER_SEQ,
	ASN1_CONTAINER_SET,
	ASN1_CONTAINER_APP,
	ASN1_CONTAINER_CONTEXT_ONLY,
	ASN1_CONTAINER_OCTETSTRING,
} ContainerType;

typedef struct WinPrAsn1EncContainer WinPrAsn1EncContainer;
/** @brief a container in the ASN1 stream (sequence, set, app or contextual) */
struct WinPrAsn1EncContainer
{
	size_t headerChunkId;
	BOOL contextual;
	WinPrAsn1_tag tag;
	ContainerType containerType;
};

/** @brief the encoder internal state */
struct WinPrAsn1Encoder
{
	WinPrAsn1EncodingRule encoding;
	wStream* pool;

	Asn1Chunk* chunks;
	Asn1Chunk staticChunks[MAX_STATIC_ITEMS];
	size_t freeChunkId;
	size_t chunksCapacity;

	WinPrAsn1EncContainer* containers;
	WinPrAsn1EncContainer staticContainers[MAX_STATIC_ITEMS];
	size_t freeContainerIndex;
	size_t containerCapacity;
};

#define WINPR_ASSERT_VALID_TAG(t) WINPR_ASSERT(t < 64)

void WinPrAsn1FreeOID(WinPrAsn1_OID* poid)
{
	WINPR_ASSERT(poid);
	free(poid->data);
	poid->data = NULL;
	poid->len = 0;
}

void WinPrAsn1FreeOctetString(WinPrAsn1_OctetString* octets)
{
	WinPrAsn1FreeOID(octets);
}

/**
 * The encoder is implemented with the goals to:
 *    * have an API which is convenient to use (avoid computing inner elements size)
 *    * hide the BER/DER encoding details
 *    * avoid multiple copies and memory moves when building the content
 *
 * To achieve this, the encoder contains a big memory block (encoder->pool), and various chunks
 * (encoder->chunks) pointing to that memory block. The idea is to reserve some space in the pool
 * for the container headers when we start a new container element. For example when a sequence is
 * started we reserve 6 bytes which is the maximum size: byte0 + length. Then fill the content of
 * the sequence in further chunks. When a container is closed, we compute the inner size (by adding
 * the size of inner chunks), we write the headers bytes, and we adjust the chunk size accordingly.
 *
 *  For example to encode:
 *      SEQ
 *          IASTRING(test1)
 *          INTEGER(200)
 *
 *  with this code:
 *
 *      WinPrAsn1EncSeqContainer(enc);
 *      WinPrAsn1EncIA5String(enc, "test1");
 *      WinPrAsn1EncInteger(enc, 200);
 *
 *  Memory pool and chunks would look like:
 *
 *     [ reserved for seq][string|5|"test1"][integer|0x81|200]
 *       (6 bytes)
 *     |-----------------||----------------------------------|
 *     ^                  ^
 *     |                  |
 *     chunk0           chunk1
 *
 *  As we try to compact chunks as much as we can, we managed to encode the ia5string and the
 * integer using the same chunk.
 *
 *  When the sequence is closed with:
 *
 *      WinPrAsn1EncEndContainer(enc);
 *
 *  The final pool and chunks will look like:
 *
 *     XXXXXX[seq headers][string|5|"test1"][integer|0x81|200]
 *
 *           |-----------||----------------------------------|
 *           ^            ^
 *           |            |
 *         chunk0       chunk1
 *
 *  The generated content can be retrieved using:
 *
 *      WinPrAsn1EncToStream(enc, targetStream);
 *
 *  It will sequentially write all the chunks in the given target stream.
 */

WinPrAsn1Encoder* WinPrAsn1Encoder_New(WinPrAsn1EncodingRule encoding)
{
	WinPrAsn1Encoder* enc = calloc(1, sizeof(*enc));
	if (!enc)
		return NULL;

	enc->encoding = encoding;
	enc->pool = Stream_New(NULL, 1024);
	if (!enc->pool)
	{
		free(enc);
		return NULL;
	}

	enc->containers = &enc->staticContainers[0];
	enc->chunks = &enc->staticChunks[0];
	enc->chunksCapacity = MAX_STATIC_ITEMS;
	enc->freeContainerIndex = 0;
	return enc;
}

void WinPrAsn1Encoder_Reset(WinPrAsn1Encoder* enc)
{
	WINPR_ASSERT(enc);

	enc->freeContainerIndex = 0;
	enc->freeChunkId = 0;

	ZeroMemory(enc->chunks, sizeof(*enc->chunks) * enc->chunksCapacity);
}

void WinPrAsn1Encoder_Free(WinPrAsn1Encoder** penc)
{
	WinPrAsn1Encoder* enc;

	WINPR_ASSERT(penc);
	enc = *penc;
	if (enc)
	{
		if (enc->containers != &enc->staticContainers[0])
			free(enc->containers);

		if (enc->chunks != &enc->staticChunks[0])
			free(enc->chunks);

		Stream_Free(enc->pool, TRUE);
		free(enc);
	}
	*penc = NULL;
}

static Asn1Chunk* asn1enc_get_free_chunk(WinPrAsn1Encoder* enc, size_t chunkSz, BOOL commit,
                                         size_t* id)
{
	Asn1Chunk* ret;
	WINPR_ASSERT(enc);
	WINPR_ASSERT(chunkSz);

	if (commit)
	{
		/* if it's not a reservation let's see if the last chunk is not a reservation and can be
		 * expanded */
		size_t lastChunk = enc->freeChunkId ? enc->freeChunkId - 1 : 0;
		ret = &enc->chunks[lastChunk];
		if (ret->capacity && ret->capacity == ret->used)
		{
			if (!Stream_EnsureRemainingCapacity(enc->pool, chunkSz))
				return NULL;

			Stream_Seek(enc->pool, chunkSz);
			ret->capacity += chunkSz;
			ret->used += chunkSz;
			if (id)
				*id = lastChunk;
			return ret;
		}
	}

	if (enc->freeChunkId == enc->chunksCapacity)
	{
		/* chunks need a resize */
		Asn1Chunk* src = (enc->chunks != &enc->staticChunks[0]) ? enc->chunks : NULL;
		Asn1Chunk* tmp = realloc(src, (enc->chunksCapacity + 10) * sizeof(*src));
		if (!tmp)
			return NULL;

		if (enc->chunks == &enc->staticChunks[0])
			memcpy(tmp, &enc->staticChunks[0], enc->chunksCapacity * sizeof(*src));
		else
			memset(tmp + enc->freeChunkId, 0, sizeof(*tmp) * 10);

		enc->chunks = tmp;
		enc->chunksCapacity += 10;
	}
	if (enc->freeChunkId == enc->chunksCapacity)
		return NULL;

	if (!Stream_EnsureRemainingCapacity(enc->pool, chunkSz))
		return NULL;

	ret = &enc->chunks[enc->freeChunkId];
	ret->poolOffset = Stream_GetPosition(enc->pool);
	ret->capacity = chunkSz;
	ret->used = commit ? chunkSz : 0;
	if (id)
		*id = enc->freeChunkId;

	enc->freeChunkId++;
	Stream_Seek(enc->pool, chunkSz);
	return ret;
}

static WinPrAsn1EncContainer* asn1enc_get_free_container(WinPrAsn1Encoder* enc, size_t* id)
{
	WinPrAsn1EncContainer* ret;
	WINPR_ASSERT(enc);

	if (enc->freeContainerIndex == enc->containerCapacity)
	{
		/* containers need a resize (or switch from static to dynamic) */
		WinPrAsn1EncContainer* src =
		    (enc->containers != &enc->staticContainers[0]) ? enc->containers : NULL;
		WinPrAsn1EncContainer* tmp = realloc(src, (enc->containerCapacity + 10) * sizeof(*src));
		if (!tmp)
			return NULL;

		if (enc->containers == &enc->staticContainers[0])
			memcpy(tmp, &enc->staticContainers[0], enc->containerCapacity * sizeof(*src));

		enc->containers = tmp;
		enc->containerCapacity += 10;
	}
	if (enc->freeContainerIndex == enc->containerCapacity)
		return NULL;

	ret = &enc->containers[enc->freeContainerIndex];
	*id = enc->freeContainerIndex;

	enc->freeContainerIndex++;
	return ret;
}

static size_t lenBytes(size_t len)
{
	if (len < 128)
		return 1;
	if (len < (1 << 8))
		return 2;
	if (len < (1 << 16))
		return 3;
	if (len < (1 << 24))
		return 4;

	return 5;
}

static void asn1WriteLen(wStream* s, size_t len)
{
	if (len < 128)
	{
		Stream_Write_UINT8(s, len);
	}
	else if (len < (1 << 8))
	{
		Stream_Write_UINT8(s, 0x81);
		Stream_Write_UINT8(s, len);
	}
	else if (len < (1 << 16))
	{
		Stream_Write_UINT8(s, 0x82);
		Stream_Write_UINT16_BE(s, len);
	}
	else if (len < (1 << 24))
	{
		Stream_Write_UINT8(s, 0x83);
		Stream_Write_UINT24_BE(s, len);
	}
	else
	{
		Stream_Write_UINT8(s, 0x84);
		Stream_Write_UINT32_BE(s, len);
	}
}

static WinPrAsn1EncContainer* getAsn1Container(WinPrAsn1Encoder* enc, ContainerType ctype,
                                               WinPrAsn1_tag tag, BOOL contextual, size_t maxLen)
{
	size_t ret;
	size_t chunkId;
	WinPrAsn1EncContainer* container;

	Asn1Chunk* chunk = asn1enc_get_free_chunk(enc, maxLen, FALSE, &chunkId);
	if (!chunk)
		return NULL;

	container = asn1enc_get_free_container(enc, &ret);
	container->containerType = ctype;
	container->tag = tag;
	container->contextual = contextual;
	container->headerChunkId = chunkId;
	return container;
}

BOOL WinPrAsn1EncAppContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId)
{
	WINPR_ASSERT_VALID_TAG(tagId);
	return getAsn1Container(enc, ASN1_CONTAINER_APP, tagId, FALSE, 6) != NULL;
}

BOOL WinPrAsn1EncSeqContainer(WinPrAsn1Encoder* enc)
{
	return getAsn1Container(enc, ASN1_CONTAINER_SEQ, 0, FALSE, 6) != NULL;
}

BOOL WinPrAsn1EncSetContainer(WinPrAsn1Encoder* enc)
{
	return getAsn1Container(enc, ASN1_CONTAINER_SET, 0, FALSE, 6) != NULL;
}

BOOL WinPrAsn1EncContextualSeqContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId)
{
	return getAsn1Container(enc, ASN1_CONTAINER_SEQ, tagId, TRUE, 6 + 6) != NULL;
}

BOOL WinPrAsn1EncContextualSetContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId)
{
	return getAsn1Container(enc, ASN1_CONTAINER_SET, tagId, TRUE, 6 + 6) != NULL;
}

BOOL WinPrAsn1EncContextualContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId)
{
	return getAsn1Container(enc, ASN1_CONTAINER_CONTEXT_ONLY, tagId, TRUE, 6) != NULL;
}

BOOL WinPrAsn1EncOctetStringContainer(WinPrAsn1Encoder* enc)
{
	return getAsn1Container(enc, ASN1_CONTAINER_OCTETSTRING, 0, FALSE, 6) != NULL;
}

BOOL WinPrAsn1EncContextualOctetStringContainer(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId)
{
	return getAsn1Container(enc, ASN1_CONTAINER_OCTETSTRING, tagId, TRUE, 6 + 6) != NULL;
}

size_t WinPrAsn1EncEndContainer(WinPrAsn1Encoder* enc)
{
	size_t innerLen, i, unused;
	size_t innerHeaderBytes, outerHeaderBytes;
	BYTE containerByte;
	WinPrAsn1EncContainer* container;
	Asn1Chunk* chunk;
	wStream staticS;
	wStream* s = &staticS;

	WINPR_ASSERT(enc);
	WINPR_ASSERT(enc->freeContainerIndex);

	/* compute inner length */
	container = &enc->containers[enc->freeContainerIndex - 1];
	innerLen = 0;
	for (i = container->headerChunkId + 1; i < enc->freeChunkId; i++)
		innerLen += enc->chunks[i].used;

	/* compute effective headerLength */
	switch (container->containerType)
	{
		case ASN1_CONTAINER_SEQ:
			containerByte = ER_TAG_SEQUENCE;
			innerHeaderBytes = 1 + lenBytes(innerLen);
			break;
		case ASN1_CONTAINER_SET:
			containerByte = ER_TAG_SET;
			innerHeaderBytes = 1 + lenBytes(innerLen);
			break;
		case ASN1_CONTAINER_OCTETSTRING:
			containerByte = ER_TAG_OCTET_STRING;
			innerHeaderBytes = 1 + lenBytes(innerLen);
			break;
		case ASN1_CONTAINER_APP:
			containerByte = ER_TAG_APP | container->tag;
			innerHeaderBytes = 1 + lenBytes(innerLen);
			break;
		case ASN1_CONTAINER_CONTEXT_ONLY:
			innerHeaderBytes = 0;
			break;
		default:
			WLog_ERR(TAG, "invalid containerType");
			return 0;
	}

	outerHeaderBytes = innerHeaderBytes;
	if (container->contextual)
	{
		outerHeaderBytes = 1 + lenBytes(innerHeaderBytes + innerLen) + innerHeaderBytes;
	}

	/* we write the headers at the end of the reserved space and we adjust
	 * the chunk to be a non reserved chunk */
	chunk = &enc->chunks[container->headerChunkId];
	unused = chunk->capacity - outerHeaderBytes;
	chunk->poolOffset += unused;
	chunk->capacity = chunk->used = outerHeaderBytes;

	Stream_StaticInit(s, Stream_Buffer(enc->pool) + chunk->poolOffset, outerHeaderBytes);
	if (container->contextual)
	{
		Stream_Write_UINT8(s, ER_TAG_CONTEXTUAL | container->tag);
		asn1WriteLen(s, innerHeaderBytes + innerLen);
	}

	switch (container->containerType)
	{
		case ASN1_CONTAINER_SEQ:
		case ASN1_CONTAINER_SET:
		case ASN1_CONTAINER_OCTETSTRING:
		case ASN1_CONTAINER_APP:
			Stream_Write_UINT8(s, containerByte);
			asn1WriteLen(s, innerLen);
			break;
		case ASN1_CONTAINER_CONTEXT_ONLY:
			break;
		default:
			WLog_ERR(TAG, "invalid containerType");
			return 0;
	}

	/* TODO: here there is place for packing chunks */
	enc->freeContainerIndex--;
	return outerHeaderBytes + innerLen;
}

static BOOL asn1_getWriteStream(WinPrAsn1Encoder* enc, size_t len, wStream* s)
{
	BYTE* dest;
	Asn1Chunk* chunk = asn1enc_get_free_chunk(enc, len, TRUE, NULL);
	if (!chunk)
		return FALSE;

	dest = Stream_Buffer(enc->pool) + chunk->poolOffset + chunk->capacity - len;
	Stream_StaticInit(s, dest, len);
	return TRUE;
}

size_t WinPrAsn1EncRawContent(WinPrAsn1Encoder* enc, const WinPrAsn1_MemoryChunk* c)
{
	wStream staticS;
	wStream* s = &staticS;

	WINPR_ASSERT(enc);
	WINPR_ASSERT(c);

	if (!asn1_getWriteStream(enc, c->len, s))
		return 0;

	Stream_Write(s, c->data, c->len);
	return c->len;
}

size_t WinPrAsn1EncContextualRawContent(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
                                        const WinPrAsn1_MemoryChunk* c)
{
	wStream staticS;
	wStream* s = &staticS;

	WINPR_ASSERT(enc);
	WINPR_ASSERT(c);
	WINPR_ASSERT_VALID_TAG(tagId);

	size_t len = 1 + lenBytes(c->len) + c->len;
	if (!asn1_getWriteStream(enc, len, s))
		return 0;

	Stream_Write_UINT8(s, ER_TAG_CONTEXTUAL | tagId);
	asn1WriteLen(s, c->len);

	Stream_Write(s, c->data, c->len);
	return len;
}

static size_t asn1IntegerLen(WinPrAsn1_INTEGER value)
{
	if (value <= 127 && value >= -128)
		return 2;
	else if (value <= 32767 && value >= -32768)
		return 3;
	else
		return 5;
}

static size_t WinPrAsn1EncIntegerLike(WinPrAsn1Encoder* enc, WinPrAsn1_tag b,
                                      WinPrAsn1_INTEGER value)
{
	wStream staticS;
	wStream* s = &staticS;
	size_t len;

	len = asn1IntegerLen(value);
	if (!asn1_getWriteStream(enc, 1 + len, s))
		return 0;

	Stream_Write_UINT8(s, b);
	switch (len)
	{
		case 2:
			Stream_Write_UINT8(s, 1);
			Stream_Write_UINT8(s, value);
			break;
		case 3:
			Stream_Write_UINT8(s, 2);
			Stream_Write_UINT16_BE(s, value);
			break;
		case 5:
			Stream_Write_UINT8(s, 4);
			Stream_Write_UINT32_BE(s, value);
			break;
	}
	return 1 + len;
}

size_t WinPrAsn1EncInteger(WinPrAsn1Encoder* enc, WinPrAsn1_INTEGER value)
{
	return WinPrAsn1EncIntegerLike(enc, ER_TAG_INTEGER, value);
}

size_t WinPrAsn1EncEnumerated(WinPrAsn1Encoder* enc, WinPrAsn1_ENUMERATED value)
{
	return WinPrAsn1EncIntegerLike(enc, ER_TAG_ENUMERATED, value);
}

static size_t WinPrAsn1EncContextualIntegerLike(WinPrAsn1Encoder* enc, WinPrAsn1_tag tag,
                                                WinPrAsn1_tagId tagId, WinPrAsn1_INTEGER value)
{
	wStream staticS;
	wStream* s = &staticS;
	size_t len, outLen;

	WINPR_ASSERT(enc);
	WINPR_ASSERT_VALID_TAG(tagId);

	len = asn1IntegerLen(value);

	outLen = 1 + lenBytes(1 + len) + (1 + len);
	if (!asn1_getWriteStream(enc, outLen, s))
		return 0;

	Stream_Write_UINT8(s, ER_TAG_CONTEXTUAL | tagId);
	asn1WriteLen(s, 1 + len);

	Stream_Write_UINT8(s, tag);
	switch (len)
	{
		case 2:
			Stream_Write_UINT8(s, 1);
			Stream_Write_UINT8(s, value);
			break;
		case 3:
			Stream_Write_UINT8(s, 2);
			Stream_Write_UINT16_BE(s, value);
			break;
		case 5:
			Stream_Write_UINT8(s, 4);
			Stream_Write_UINT32_BE(s, value);
			break;
	}
	return outLen;
}

size_t WinPrAsn1EncContextualInteger(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
                                     WinPrAsn1_INTEGER value)
{
	return WinPrAsn1EncContextualIntegerLike(enc, ER_TAG_INTEGER, tagId, value);
}

size_t WinPrAsn1EncContextualEnumerated(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
                                        WinPrAsn1_ENUMERATED value)
{
	return WinPrAsn1EncContextualIntegerLike(enc, ER_TAG_ENUMERATED, tagId, value);
}

size_t WinPrAsn1EncBoolean(WinPrAsn1Encoder* enc, WinPrAsn1_BOOL b)
{
	wStream staticS;
	wStream* s = &staticS;

	if (!asn1_getWriteStream(enc, 3, s))
		return 0;

	Stream_Write_UINT8(s, ER_TAG_BOOLEAN);
	Stream_Write_UINT8(s, 1);
	Stream_Write_UINT8(s, b ? 0xff : 0);

	return 3;
}

size_t WinPrAsn1EncContextualBoolean(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId, WinPrAsn1_BOOL b)
{
	wStream staticS;
	wStream* s = &staticS;

	WINPR_ASSERT(enc);
	WINPR_ASSERT_VALID_TAG(tagId);

	if (!asn1_getWriteStream(enc, 5, s))
		return 0;

	Stream_Write_UINT8(s, ER_TAG_CONTEXTUAL | tagId);
	Stream_Write_UINT8(s, 3);

	Stream_Write_UINT8(s, ER_TAG_BOOLEAN);
	Stream_Write_UINT8(s, 1);
	Stream_Write_UINT8(s, b ? 0xff : 0);

	return 5;
}

static size_t WinPrAsn1EncMemoryChunk(WinPrAsn1Encoder* enc, BYTE wireType,
                                      const WinPrAsn1_MemoryChunk* mchunk)
{
	wStream s;
	size_t len;

	WINPR_ASSERT(enc);
	WINPR_ASSERT(mchunk);
	len = 1 + lenBytes(mchunk->len) + mchunk->len;

	if (!asn1_getWriteStream(enc, len, &s))
		return 0;

	Stream_Write_UINT8(&s, wireType);
	asn1WriteLen(&s, mchunk->len);
	Stream_Write(&s, mchunk->data, mchunk->len);
	return len;
}

size_t WinPrAsn1EncOID(WinPrAsn1Encoder* enc, const WinPrAsn1_OID* oid)
{
	return WinPrAsn1EncMemoryChunk(enc, ER_TAG_OBJECT_IDENTIFIER, oid);
}

size_t WinPrAsn1EncOctetString(WinPrAsn1Encoder* enc, const WinPrAsn1_OctetString* octets)
{
	return WinPrAsn1EncMemoryChunk(enc, ER_TAG_OCTET_STRING, octets);
}

size_t WinPrAsn1EncIA5String(WinPrAsn1Encoder* enc, WinPrAsn1_IA5STRING ia5)
{
	WinPrAsn1_MemoryChunk chunk;
	WINPR_ASSERT(ia5);
	chunk.data = (BYTE*)ia5;
	chunk.len = strlen(ia5);
	return WinPrAsn1EncMemoryChunk(enc, ER_TAG_IA5STRING, &chunk);
}

size_t WinPrAsn1EncContextualMemoryChunk(WinPrAsn1Encoder* enc, BYTE wireType,
                                         WinPrAsn1_tagId tagId, const WinPrAsn1_MemoryChunk* mchunk)
{
	wStream s;
	size_t len, outLen;

	WINPR_ASSERT(enc);
	WINPR_ASSERT_VALID_TAG(tagId);
	WINPR_ASSERT(mchunk);
	len = 1 + lenBytes(mchunk->len) + mchunk->len;

	outLen = 1 + lenBytes(len) + len;
	if (!asn1_getWriteStream(enc, outLen, &s))
		return 0;

	Stream_Write_UINT8(&s, ER_TAG_CONTEXTUAL | tagId);
	asn1WriteLen(&s, len);

	Stream_Write_UINT8(&s, wireType);
	asn1WriteLen(&s, mchunk->len);
	Stream_Write(&s, mchunk->data, mchunk->len);
	return outLen;
}

size_t WinPrAsn1EncContextualOID(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
                                 const WinPrAsn1_OID* oid)
{
	return WinPrAsn1EncContextualMemoryChunk(enc, ER_TAG_OBJECT_IDENTIFIER, tagId, oid);
}

size_t WinPrAsn1EncContextualOctetString(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
                                         const WinPrAsn1_OctetString* octets)
{
	return WinPrAsn1EncContextualMemoryChunk(enc, ER_TAG_OCTET_STRING, tagId, octets);
}

size_t WinPrAsn1EncContextualIA5String(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
                                       WinPrAsn1_IA5STRING ia5)
{
	WinPrAsn1_MemoryChunk chunk;
	WINPR_ASSERT(ia5);
	chunk.data = (BYTE*)ia5;
	chunk.len = strlen(ia5);

	return WinPrAsn1EncContextualMemoryChunk(enc, ER_TAG_IA5STRING, tagId, &chunk);
}

static void write2digit(wStream* s, UINT8 v)
{
	Stream_Write_UINT8(s, '0' + (v / 10));
	Stream_Write_UINT8(s, '0' + (v % 10));
}

size_t WinPrAsn1EncUtcTime(WinPrAsn1Encoder* enc, const WinPrAsn1_UTCTIME* utc)
{
	wStream staticS;
	wStream* s = &staticS;

	WINPR_ASSERT(enc);
	WINPR_ASSERT(utc);
	WINPR_ASSERT(utc->year >= 2000);

	if (!asn1_getWriteStream(enc, 15, s))
		return 0;

	Stream_Write_UINT8(s, ER_TAG_UTCTIME);
	Stream_Write_UINT8(s, 13);

	write2digit(s, utc->year - 2000);
	write2digit(s, utc->month);
	write2digit(s, utc->day);
	write2digit(s, utc->hour);
	write2digit(s, utc->minute);
	write2digit(s, utc->second);
	Stream_Write_UINT8(s, utc->tz);
	return 15;
}

size_t WinPrAsn1EncContextualUtcTime(WinPrAsn1Encoder* enc, WinPrAsn1_tagId tagId,
                                     const WinPrAsn1_UTCTIME* utc)
{
	wStream staticS;
	wStream* s = &staticS;

	WINPR_ASSERT(enc);
	WINPR_ASSERT_VALID_TAG(tagId);
	WINPR_ASSERT(utc);
	WINPR_ASSERT(utc->year >= 2000);

	if (!asn1_getWriteStream(enc, 17, s))
		return 0;

	Stream_Write_UINT8(s, ER_TAG_CONTEXTUAL | tagId);
	Stream_Write_UINT8(s, 15);

	Stream_Write_UINT8(s, ER_TAG_UTCTIME);
	Stream_Write_UINT8(s, 13);

	write2digit(s, utc->year - 2000);
	write2digit(s, utc->month);
	write2digit(s, utc->day);
	write2digit(s, utc->hour);
	write2digit(s, utc->minute);
	write2digit(s, utc->second);
	Stream_Write_UINT8(s, utc->tz);

	return 17;
}

BOOL WinPrAsn1EncStreamSize(WinPrAsn1Encoder* enc, size_t* s)
{
	size_t finalSize = 0;
	size_t i;

	WINPR_ASSERT(enc);
	WINPR_ASSERT(s);

	if (enc->freeContainerIndex != 0)
	{
		WLog_ERR(TAG, "some container have not been closed");
		return FALSE;
	}

	for (i = 0; i < enc->freeChunkId; i++)
		finalSize += enc->chunks[i].used;
	*s = finalSize;
	return TRUE;
}

BOOL WinPrAsn1EncToStream(WinPrAsn1Encoder* enc, wStream* s)
{
	size_t finalSize;
	size_t i;

	WINPR_ASSERT(enc);
	WINPR_ASSERT(s);

	if (!WinPrAsn1EncStreamSize(enc, &finalSize))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, finalSize))
		return FALSE;

	for (i = 0; i < enc->freeChunkId; i++)
	{
		BYTE* src = Stream_Buffer(enc->pool) + enc->chunks[i].poolOffset;
		Stream_Write(s, src, enc->chunks[i].used);
	}

	return TRUE;
}

void WinPrAsn1Decoder_Init(WinPrAsn1Decoder* decoder, WinPrAsn1EncodingRule encoding,
                           wStream* source)
{
	WINPR_ASSERT(decoder);
	WINPR_ASSERT(source);

	decoder->encoding = encoding;
	memcpy(&decoder->source, source, sizeof(*source));
}

void WinPrAsn1Decoder_InitMem(WinPrAsn1Decoder* decoder, WinPrAsn1EncodingRule encoding,
                              const BYTE* source, size_t len)
{
	WINPR_ASSERT(decoder);
	WINPR_ASSERT(source);

	decoder->encoding = encoding;
	Stream_StaticConstInit(&decoder->source, source, len);
}

BOOL WinPrAsn1DecPeekTag(WinPrAsn1Decoder* dec, WinPrAsn1_tag* tag)
{
	WINPR_ASSERT(dec);
	WINPR_ASSERT(tag);

	if (Stream_GetRemainingLength(&dec->source) < 1)
		return FALSE;
	Stream_Peek(&dec->source, tag, 1);
	return TRUE;
}

static size_t readLen(wStream* s, size_t* len, BOOL derCheck)
{
	size_t retLen;
	size_t ret = 0;

	if (Stream_GetRemainingLength(s) < 1)
		return 0;

	Stream_Read_UINT8(s, retLen);
	ret++;
	if (retLen & 0x80)
	{
		BYTE tmp;
		size_t nBytes = (retLen & 0x7f);

		if (Stream_GetRemainingLength(s) < nBytes)
			return 0;

		ret += nBytes;
		for (retLen = 0; nBytes; nBytes--)
		{
			Stream_Read_UINT8(s, tmp);
			retLen = (retLen << 8) + tmp;
		}

		if (derCheck)
		{
			/* check that the DER rule is respected, and that length encoding is optimal */
			if (ret > 1 && retLen < 128)
				return 0;
		}
	}

	*len = retLen;
	return ret;
}

static size_t readTagAndLen(WinPrAsn1Decoder* dec, wStream* s, WinPrAsn1_tag* tag, size_t* len)
{
	size_t lenBytes;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read(s, tag, 1);
	lenBytes = readLen(s, len, (dec->encoding == WINPR_ASN1_DER));
	if (!lenBytes)
		return 0;

	return 1 + lenBytes;
}

size_t WinPrAsn1DecReadTagAndLen(WinPrAsn1Decoder* dec, WinPrAsn1_tag* tag, size_t* len)
{
	WINPR_ASSERT(dec);
	WINPR_ASSERT(tag);
	WINPR_ASSERT(len);

	return readTagAndLen(dec, &dec->source, tag, len);
}

size_t WinPrAsn1DecPeekTagAndLen(WinPrAsn1Decoder* dec, WinPrAsn1_tag* tag, size_t* len)
{
	wStream staticS;
	wStream* s = &staticS;

	WINPR_ASSERT(dec);

	Stream_StaticConstInit(s, Stream_ConstPointer(&dec->source),
	                       Stream_GetRemainingLength(&dec->source));
	return readTagAndLen(dec, s, tag, len);
}

size_t WinPrAsn1DecReadTagLenValue(WinPrAsn1Decoder* dec, WinPrAsn1_tag* tag, size_t* len,
                                   WinPrAsn1Decoder* value)
{
	size_t ret;
	WINPR_ASSERT(dec);
	WINPR_ASSERT(tag);
	WINPR_ASSERT(len);
	WINPR_ASSERT(value);

	ret = readTagAndLen(dec, &dec->source, tag, len);
	if (!ret)
		return 0;

	if (Stream_GetRemainingLength(&dec->source) < *len)
		return 0;

	value->encoding = dec->encoding;
	Stream_StaticInit(&value->source, Stream_Pointer(&dec->source), *len);
	Stream_Seek(&dec->source, *len);
	return ret + *len;
}

size_t WinPrAsn1DecReadBoolean(WinPrAsn1Decoder* dec, WinPrAsn1_BOOL* target)
{
	BYTE v;
	WinPrAsn1_tag tag;
	size_t len;
	size_t ret;

	WINPR_ASSERT(dec);
	WINPR_ASSERT(target);

	ret = readTagAndLen(dec, &dec->source, &tag, &len);
	if (!ret || tag != ER_TAG_BOOLEAN)
		return 0;
	if (Stream_GetRemainingLength(&dec->source) < len || len != 1)
		return 0;

	Stream_Read_UINT8(&dec->source, v);
	*target = !!v;
	return ret;
}

static size_t WinPrAsn1DecReadIntegerLike(WinPrAsn1Decoder* dec, WinPrAsn1_tag expectedTag,
                                          WinPrAsn1_INTEGER* target)
{
	signed char v;
	WinPrAsn1_tag tag;
	size_t len;
	size_t ret;

	WINPR_ASSERT(dec);
	WINPR_ASSERT(target);

	ret = readTagAndLen(dec, &dec->source, &tag, &len);
	if (!ret || tag != expectedTag)
		return 0;
	if (Stream_GetRemainingLength(&dec->source) < len || len > 4)
		return 0;

	ret += len;
	for (*target = 0; len; len--)
	{
		Stream_Read_INT8(&dec->source, v);
		*target = (*target << 8) + v;
	}

	/* TODO: check ber/der rules */
	return ret;
}

size_t WinPrAsn1DecReadInteger(WinPrAsn1Decoder* dec, WinPrAsn1_INTEGER* target)
{
	return WinPrAsn1DecReadIntegerLike(dec, ER_TAG_INTEGER, target);
}

size_t WinPrAsn1DecReadEnumerated(WinPrAsn1Decoder* dec, WinPrAsn1_ENUMERATED* target)
{
	return WinPrAsn1DecReadIntegerLike(dec, ER_TAG_ENUMERATED, target);
}

static size_t WinPrAsn1DecReadMemoryChunkLike(WinPrAsn1Decoder* dec, WinPrAsn1_tag expectedTag,
                                              WinPrAsn1_MemoryChunk* target, BOOL allocate)
{
	WinPrAsn1_tag tag;
	size_t len;
	size_t ret;

	WINPR_ASSERT(dec);
	WINPR_ASSERT(target);

	ret = readTagAndLen(dec, &dec->source, &tag, &len);
	if (!ret || tag != expectedTag)
		return 0;
	if (Stream_GetRemainingLength(&dec->source) < len)
		return 0;

	ret += len;

	target->len = len;
	if (allocate)
	{
		target->data = malloc(len);
		if (!target->data)
			return 0;
		Stream_Read(&dec->source, target->data, len);
	}
	else
	{
		target->data = Stream_Pointer(&dec->source);
		Stream_Seek(&dec->source, len);
	}

	return ret;
}

size_t WinPrAsn1DecReadOID(WinPrAsn1Decoder* dec, WinPrAsn1_OID* target, BOOL allocate)
{
	return WinPrAsn1DecReadMemoryChunkLike(dec, ER_TAG_OBJECT_IDENTIFIER,
	                                       (WinPrAsn1_MemoryChunk*)target, allocate);
}

size_t WinPrAsn1DecReadOctetString(WinPrAsn1Decoder* dec, WinPrAsn1_OctetString* target,
                                   BOOL allocate)
{
	return WinPrAsn1DecReadMemoryChunkLike(dec, ER_TAG_OCTET_STRING, (WinPrAsn1_OctetString*)target,
	                                       allocate);
}

size_t WinPrAsn1DecReadIA5String(WinPrAsn1Decoder* dec, WinPrAsn1_IA5STRING* target)
{
	WinPrAsn1_tag tag;
	size_t len;
	size_t ret;
	WinPrAsn1_IA5STRING s;

	WINPR_ASSERT(dec);
	WINPR_ASSERT(target);

	ret = readTagAndLen(dec, &dec->source, &tag, &len);
	if (!ret || tag != ER_TAG_IA5STRING)
		return 0;
	if (Stream_GetRemainingLength(&dec->source) < len)
		return 0;

	ret += len;

	s = malloc(len + 1);
	if (!s)
		return 0;
	Stream_Read(&dec->source, s, len);
	s[len] = 0;
	*target = s;
	return ret;
}

static int read2digits(wStream* s)
{
	int ret = 0;
	char c;

	Stream_Read_UINT8(s, c);
	if (c < '0' || c > '9')
		return -1;

	ret = (c - '0') * 10;

	Stream_Read_UINT8(s, c);
	if (c < '0' || c > '9')
		return -1;

	ret += (c - '0');
	return ret;
}

size_t WinPrAsn1DecReadUtcTime(WinPrAsn1Decoder* dec, WinPrAsn1_UTCTIME* target)
{
	WinPrAsn1_tag tag;
	size_t len;
	size_t ret;
	int v;
	wStream sub;
	wStream* s = &sub;

	WINPR_ASSERT(dec);
	WINPR_ASSERT(target);

	ret = readTagAndLen(dec, &dec->source, &tag, &len);
	if (!ret || tag != ER_TAG_UTCTIME)
		return 0;
	if (Stream_GetRemainingLength(&dec->source) < len || len < 12)
		return 0;

	Stream_StaticConstInit(s, Stream_Pointer(&dec->source), len);

	v = read2digits(s);
	if (v <= 0)
		return 0;
	target->year = 2000 + v;

	v = read2digits(s);
	if (v <= 0)
		return 0;
	target->month = v;

	v = read2digits(s);
	if (v <= 0)
		return 0;
	target->day = v;

	v = read2digits(s);
	if (v <= 0)
		return 0;
	target->hour = v;

	v = read2digits(s);
	if (v <= 0)
		return 0;
	target->minute = v;

	v = read2digits(s);
	if (v <= 0)
		return 0;
	target->second = v;

	if (Stream_GetRemainingLength(s) >= 1)
	{
		Stream_Read_UINT8(s, target->tz);
	}

	Stream_Seek(&dec->source, len);
	ret += len;

	return ret;
}

size_t WinPrAsn1DecReadNull(WinPrAsn1Decoder* dec)
{
	WinPrAsn1_tag tag;
	size_t len;
	size_t ret;

	WINPR_ASSERT(dec);

	ret = readTagAndLen(dec, &dec->source, &tag, &len);
	if (!ret || tag != ER_TAG_NULL || len)
		return 0;

	return ret;
}

static size_t readConstructed(WinPrAsn1Decoder* dec, wStream* s, WinPrAsn1_tag* tag,
                              WinPrAsn1Decoder* target)
{
	size_t len;
	size_t ret;

	ret = readTagAndLen(dec, s, tag, &len);
	if (!ret || Stream_GetRemainingLength(s) < len)
		return 0;

	target->encoding = dec->encoding;
	Stream_StaticConstInit(&target->source, Stream_Pointer(s), len);
	Stream_Seek(s, len);
	return ret + len;
}

size_t WinPrAsn1DecReadApp(WinPrAsn1Decoder* dec, WinPrAsn1_tagId* tagId, WinPrAsn1Decoder* target)
{
	WinPrAsn1_tag tag;
	size_t ret;

	WINPR_ASSERT(dec);
	WINPR_ASSERT(target);

	ret = readConstructed(dec, &dec->source, &tag, target);
	if ((tag & ER_TAG_APP) != ER_TAG_APP)
		return 0;

	*tagId = (tag & ER_TAG_MASK);
	return ret;
}

size_t WinPrAsn1DecReadSequence(WinPrAsn1Decoder* dec, WinPrAsn1Decoder* target)
{
	WinPrAsn1_tag tag;
	size_t ret;

	WINPR_ASSERT(dec);
	WINPR_ASSERT(target);

	ret = readConstructed(dec, &dec->source, &tag, target);
	if (tag != ER_TAG_SEQUENCE)
		return 0;

	return ret;
}

size_t WinPrAsn1DecReadSet(WinPrAsn1Decoder* dec, WinPrAsn1Decoder* target)
{
	WinPrAsn1_tag tag;
	size_t ret;

	WINPR_ASSERT(dec);
	WINPR_ASSERT(target);

	ret = readConstructed(dec, &dec->source, &tag, target);
	if (tag != ER_TAG_SET)
		return 0;

	return ret;
}

size_t readContextualTag(WinPrAsn1Decoder* dec, wStream* s, WinPrAsn1_tagId* tagId,
                         WinPrAsn1Decoder* ctxtDec)
{
	size_t ret;
	WinPrAsn1_tag ftag;

	ret = readConstructed(dec, s, &ftag, ctxtDec);
	if (!ret)
		return 0;

	if ((ftag & ER_TAG_CONTEXTUAL) != ER_TAG_CONTEXTUAL)
		return 0;

	*tagId = (ftag & ER_TAG_MASK);
	return ret;
}

size_t WinPrAsn1DecReadContextualTag(WinPrAsn1Decoder* dec, WinPrAsn1_tagId* tagId,
                                     WinPrAsn1Decoder* ctxtDec)
{
	WINPR_ASSERT(dec);
	WINPR_ASSERT(tagId);
	WINPR_ASSERT(ctxtDec);

	return readContextualTag(dec, &dec->source, tagId, ctxtDec);
}

size_t WinPrAsn1DecPeekContextualTag(WinPrAsn1Decoder* dec, WinPrAsn1_tagId* tagId,
                                     WinPrAsn1Decoder* ctxtDec)
{
	wStream staticS;
	WINPR_ASSERT(dec);

	Stream_StaticConstInit(&staticS, Stream_Pointer(&dec->source),
	                       Stream_GetRemainingLength(&dec->source));
	return readContextualTag(dec, &staticS, tagId, ctxtDec);
}

size_t WinPrAsn1DecReadContextualBool(WinPrAsn1Decoder* dec, WinPrAsn1_tagId tagId, BOOL* error,
                                      WinPrAsn1_BOOL* target)
{
	size_t ret, ret2;
	WinPrAsn1_tag ftag;
	WinPrAsn1Decoder content;

	WINPR_ASSERT(error);

	ret = WinPrAsn1DecPeekContextualTag(dec, &ftag, &content);
	if (!ret || ftag != tagId)
		return 0;

	ret2 = WinPrAsn1DecReadBoolean(&content, target);
	if (!ret2)
	{
		*error = TRUE;
		return 0;
	}

	*error = FALSE;
	Stream_Seek(&dec->source, ret);
	return ret;
}

size_t WinPrAsn1DecReadContextualInteger(WinPrAsn1Decoder* dec, WinPrAsn1_tagId tagId, BOOL* error,
                                         WinPrAsn1_INTEGER* target)
{
	size_t ret, ret2;
	WinPrAsn1_tag ftag;
	WinPrAsn1Decoder content;

	WINPR_ASSERT(error);

	ret = WinPrAsn1DecPeekContextualTag(dec, &ftag, &content);
	if (!ret || ftag != tagId)
		return 0;

	ret2 = WinPrAsn1DecReadInteger(&content, target);
	if (!ret2)
	{
		*error = TRUE;
		return 0;
	}

	*error = FALSE;
	Stream_Seek(&dec->source, ret);
	return ret;
}

size_t WinPrAsn1DecReadContextualOID(WinPrAsn1Decoder* dec, WinPrAsn1_tagId tagId, BOOL* error,
                                     WinPrAsn1_OID* target, BOOL allocate)
{
	size_t ret, ret2;
	WinPrAsn1_tag ftag;
	WinPrAsn1Decoder content;

	WINPR_ASSERT(error);

	ret = WinPrAsn1DecPeekContextualTag(dec, &ftag, &content);
	if (!ret || ftag != tagId)
		return 0;

	ret2 = WinPrAsn1DecReadOID(&content, target, allocate);
	if (!ret2)
	{
		*error = TRUE;
		return 0;
	}

	*error = FALSE;
	Stream_Seek(&dec->source, ret);
	return ret;
}

size_t WinPrAsn1DecReadContextualSequence(WinPrAsn1Decoder* dec, WinPrAsn1_tagId tagId, BOOL* error,
                                          WinPrAsn1Decoder* target)
{
	size_t ret, ret2;
	WinPrAsn1_tag ftag;
	WinPrAsn1Decoder content;

	WINPR_ASSERT(error);

	ret = WinPrAsn1DecPeekContextualTag(dec, &ftag, &content);
	if (!ret || ftag != tagId)
		return 0;

	ret2 = WinPrAsn1DecReadSequence(&content, target);
	if (!ret2)
	{
		*error = TRUE;
		return 0;
	}

	*error = FALSE;
	Stream_Seek(&dec->source, ret);
	return ret;
}
