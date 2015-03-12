/**
 * WinPR: Windows Portable Runtime
 * BipBuffer
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>

#include <winpr/collections.h>

/**
 * The Bip Buffer - The Circular Buffer with a Twist:
 * http://www.codeproject.com/Articles/3479/The-Bip-Buffer-The-Circular-Buffer-with-a-Twist
 */

void BipBuffer_Clear(wBipBuffer* bb)
{
	bb->headA = 0;
	bb->sizeA = 0;
	bb->headB = 0;
	bb->sizeB = 0;
	bb->headR = 0;
	bb->sizeR = 0;
}

BOOL BipBuffer_AllocBuffer(wBipBuffer* bb, size_t size)
{
	if (size < 1)
		return FALSE;

	bb->buffer = (BYTE*) malloc(size);

	if (!bb->buffer)
		return FALSE;

	bb->size = size;

	return TRUE;
}

BOOL BipBuffer_Grow(wBipBuffer* bb, size_t size)
{
	BYTE* block;
	BYTE* buffer;
	size_t blockSize = 0;
	size_t commitSize = 0;

	size += size % 4096;

	if (size <= bb->size)
		return TRUE;

	buffer = (BYTE*) malloc(size);

	if (!buffer)
		return FALSE;

	block = BipBuffer_PeekBlock(bb, &blockSize);

	if (block)
	{
		CopyMemory(&buffer[commitSize], block, blockSize);
		BipBuffer_Decommit(bb, blockSize);
		commitSize += blockSize;
	}

	block = BipBuffer_PeekBlock(bb, &blockSize);

	if (block)
	{
		CopyMemory(&buffer[commitSize], block, blockSize);
		BipBuffer_Decommit(bb, blockSize);
		commitSize += blockSize;
	}

	BipBuffer_Clear(bb);

	free(bb->buffer);
	bb->buffer = buffer;
	bb->size = size;

	bb->headA = 0;
	bb->sizeA = commitSize;

	return TRUE;
}

void BipBuffer_FreeBuffer(wBipBuffer* bb)
{
	if (bb->buffer)
	{
		free(bb->buffer);
		bb->buffer = NULL;
	}

	BipBuffer_Clear(bb);
}

size_t BipBuffer_BufferSize(wBipBuffer* bb)
{
	return bb->size;
}

size_t BipBuffer_CommittedSize(wBipBuffer* bb)
{
	return bb->sizeA + bb->sizeB;
}

size_t BipBuffer_ReservedSize(wBipBuffer* bb)
{
	return bb->sizeR;
}

BYTE* BipBuffer_TryReserve(wBipBuffer* bb, size_t size, size_t* reserved)
{
	size_t freeSpace;

	if (!bb->sizeB)
	{
		/* block B does not exist */

		freeSpace = bb->size - bb->headA - bb->sizeA; /* space after block A */

		if (freeSpace >= bb->headA)
		{
			if (freeSpace == 0)
				return NULL;

			if (size < freeSpace)
				freeSpace = size;

			bb->sizeR = freeSpace;
			*reserved = freeSpace;

			bb->headR = bb->headA + bb->sizeA;
			return &bb->buffer[bb->headR];
		}

		if (bb->headA == 0)
			return NULL;

		if (bb->headA < size)
			size = bb->headA;

		bb->sizeR = size;
		*reserved = size;

		bb->headR = 0;
		return bb->buffer;
	}

	/* block B exists */

	freeSpace = bb->headA - bb->headB - bb->sizeB; /* space after block B */

	if (size < freeSpace)
		freeSpace = size;

	if (freeSpace == 0)
		return NULL;

	bb->sizeR = freeSpace;
	*reserved = freeSpace;

	bb->headR = bb->headB + bb->sizeB;
	return &bb->buffer[bb->headR];
}

BYTE* BipBuffer_Reserve(wBipBuffer* bb, size_t size)
{
	BYTE* block = NULL;
	size_t reserved = 0;

	block = BipBuffer_TryReserve(bb, size, &reserved);

	if (reserved == size)
		return block;

	if (!BipBuffer_Grow(bb, size))
		return NULL;

	block = BipBuffer_TryReserve(bb, size, &reserved);

	return block;
}

void BipBuffer_Commit(wBipBuffer* bb, size_t size)
{
	if (size == 0)
	{
		bb->headR = 0;
		bb->sizeR = 0;
		return;
	}

	if (size > bb->sizeR)
		size = bb->sizeR;

	if ((bb->sizeA == 0) && (bb->sizeB == 0))
	{
		bb->headA = bb->headR;
		bb->sizeA = size;
		bb->headR = 0;
		bb->sizeR = 0;
		return;
	}

	if (bb->headR == (bb->sizeA + bb->headA))
		bb->sizeA += size;
	else
		bb->sizeB += size;

	bb->headR = 0;
	bb->sizeR = 0;
}

BYTE* BipBuffer_PeekBlock(wBipBuffer* bb, size_t* size)
{
	if (bb->sizeA == 0)
	{
		*size = 0;
		return NULL;
	}

	*size = bb->sizeA;

	return &bb->buffer[bb->headA];
}

void BipBuffer_Decommit(wBipBuffer* bb, size_t size)
{
	if (!bb)
		return;

	if (size >= bb->sizeA)
	{
		bb->headA = bb->headB;
		bb->sizeA = bb->sizeB;
		bb->headB = 0;
		bb->sizeB = 0;
	}
	else
	{
		bb->sizeA -= size;
		bb->headA += size;
	}
}

int BipBuffer_Read(wBipBuffer* bb, BYTE* data, size_t size)
{
	int status = 0;
	BYTE* block = NULL;
	size_t readSize = 0;
	size_t blockSize = 0;

	if (!bb)
		return -1;

	block = BipBuffer_PeekBlock(bb, &blockSize);

	if (block)
	{
		readSize = size - status;

		if (readSize > blockSize)
			readSize = blockSize;

		CopyMemory(&data[status], block, readSize);
		BipBuffer_Decommit(bb, readSize);
		status += readSize;

		if ((status == size) || (readSize < blockSize))
			return status;
	}

	block = BipBuffer_PeekBlock(bb, &blockSize);

	if (block)
	{
		readSize = size - status;

		if (readSize > blockSize)
			readSize = blockSize;

		CopyMemory(&data[status], block, readSize);
		BipBuffer_Decommit(bb, readSize);
		status += readSize;

		if ((status == size) || (readSize < blockSize))
			return status;
	}

	return status;
}

int BipBuffer_Write(wBipBuffer* bb, BYTE* data, size_t size)
{
	BYTE* block;

	block = BipBuffer_Reserve(bb, size);

	if (!block)
		return -1;

	CopyMemory(block, data, size);
	BipBuffer_Commit(bb, size);

	return (int) size;
}

/**
 * Construction, Destruction
 */

wBipBuffer* BipBuffer_New(size_t size)
{
	wBipBuffer* bb;

	bb = (wBipBuffer*) calloc(1, sizeof(wBipBuffer));

	if (bb)
	{
		if (!BipBuffer_AllocBuffer(bb, size))
		{
			free(bb);
			return NULL;
		}
	}

	return bb;
}

void BipBuffer_Free(wBipBuffer* bb)
{
	if (!bb)
		return;

	BipBuffer_FreeBuffer(bb);

	free(bb);
}
