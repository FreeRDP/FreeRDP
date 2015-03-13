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
#include <winpr/sysinfo.h>

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

	size += size % bb->pageSize;

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

	size += size % bb->pageSize;

	if (size <= bb->size)
		return TRUE;

	buffer = (BYTE*) malloc(size);

	if (!buffer)
		return FALSE;

	block = BipBuffer_ReadTryReserve(bb, 0, &blockSize);

	if (block)
	{
		CopyMemory(&buffer[commitSize], block, blockSize);
		BipBuffer_ReadCommit(bb, blockSize);
		commitSize += blockSize;
	}

	block = BipBuffer_ReadTryReserve(bb, 0, &blockSize);

	if (block)
	{
		CopyMemory(&buffer[commitSize], block, blockSize);
		BipBuffer_ReadCommit(bb, blockSize);
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

size_t BipBuffer_UsedSize(wBipBuffer* bb)
{
	return bb->sizeA + bb->sizeB;
}

size_t BipBuffer_BufferSize(wBipBuffer* bb)
{
	return bb->size;
}

BYTE* BipBuffer_WriteTryReserve(wBipBuffer* bb, size_t size, size_t* reserved)
{
	size_t reservable;

	if (!reserved)
		return NULL;

	if (!bb->sizeB)
	{
		/* block B does not exist */

		reservable = bb->size - bb->headA - bb->sizeA; /* space after block A */

		if (reservable >= bb->headA)
		{
			if (reservable == 0)
				return NULL;

			if (size < reservable)
				reservable = size;

			bb->sizeR = reservable;
			*reserved = reservable;

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

	reservable = bb->headA - bb->headB - bb->sizeB; /* space after block B */

	if (size < reservable)
		reservable = size;

	if (reservable == 0)
		return NULL;

	bb->sizeR = reservable;
	*reserved = reservable;

	bb->headR = bb->headB + bb->sizeB;
	return &bb->buffer[bb->headR];
}

BYTE* BipBuffer_WriteReserve(wBipBuffer* bb, size_t size)
{
	BYTE* block = NULL;
	size_t reserved = 0;

	block = BipBuffer_WriteTryReserve(bb, size, &reserved);

	if (reserved == size)
		return block;

	if (!BipBuffer_Grow(bb, size))
		return NULL;

	block = BipBuffer_WriteTryReserve(bb, size, &reserved);

	return block;
}

void BipBuffer_WriteCommit(wBipBuffer* bb, size_t size)
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

BYTE* BipBuffer_ReadTryReserve(wBipBuffer* bb, size_t size, size_t* reserved)
{
	size_t reservable = 0;

	if (!reserved)
		return NULL;

	if (bb->sizeA == 0)
	{
		*reserved = 0;
		return NULL;
	}

	reservable = bb->sizeA;

	if (size && (reservable > size))
		reservable = size;

	*reserved = reservable;

	return &bb->buffer[bb->headA];
}

BYTE* BipBuffer_ReadReserve(wBipBuffer* bb, size_t size)
{
	BYTE* block = NULL;
	size_t reserved = 0;

	block = BipBuffer_ReadTryReserve(bb, size, &reserved);

	if (reserved == size)
		return block;

	if (!BipBuffer_Grow(bb, size))
		return NULL;

	block = BipBuffer_ReadTryReserve(bb, size, &reserved);

	return block;
}

void BipBuffer_ReadCommit(wBipBuffer* bb, size_t size)
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

	block = BipBuffer_ReadTryReserve(bb, 0, &blockSize);

	if (block)
	{
		readSize = size - status;

		if (readSize > blockSize)
			readSize = blockSize;

		CopyMemory(&data[status], block, readSize);
		BipBuffer_ReadCommit(bb, readSize);
		status += readSize;

		if ((status == size) || (readSize < blockSize))
			return status;
	}

	block = BipBuffer_ReadTryReserve(bb, 0, &blockSize);

	if (block)
	{
		readSize = size - status;

		if (readSize > blockSize)
			readSize = blockSize;

		CopyMemory(&data[status], block, readSize);
		BipBuffer_ReadCommit(bb, readSize);
		status += readSize;

		if ((status == size) || (readSize < blockSize))
			return status;
	}

	return status;
}

int BipBuffer_Write(wBipBuffer* bb, BYTE* data, size_t size)
{
	BYTE* block;

	block = BipBuffer_WriteReserve(bb, size);

	if (!block)
		return -1;

	CopyMemory(block, data, size);
	BipBuffer_WriteCommit(bb, size);

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
		SYSTEM_INFO si;

		GetSystemInfo(&si);

		bb->pageSize = (size_t) si.dwPageSize;

		if (bb->pageSize < 4096)
			bb->pageSize = 4096;

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
