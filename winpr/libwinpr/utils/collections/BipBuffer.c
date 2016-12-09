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

#define BipBlock_Clear(_bbl) \
	_bbl.index = _bbl.size = 0

#define BipBlock_Copy(_dst, _src) \
	_dst.index = _src.index; \
	_dst.size = _src.size

void BipBuffer_Clear(wBipBuffer* bb)
{
	BipBlock_Clear(bb->blockA);
	BipBlock_Clear(bb->blockB);
	BipBlock_Clear(bb->readR);
	BipBlock_Clear(bb->writeR);
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

	bb->blockA.index = 0;
	bb->blockA.size = commitSize;

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
	return bb->blockA.size + bb->blockB.size;
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

	if (!bb->blockB.size)
	{
		/* block B does not exist */

		reservable = bb->size - bb->blockA.index - bb->blockA.size; /* space after block A */

		if (reservable >= bb->blockA.index)
		{
			if (reservable == 0)
				return NULL;

			if (size < reservable)
				reservable = size;

			bb->writeR.size = reservable;
			*reserved = reservable;

			bb->writeR.index = bb->blockA.index + bb->blockA.size;
			return &bb->buffer[bb->writeR.index];
		}

		if (bb->blockA.index == 0)
			return NULL;

		if (bb->blockA.index < size)
			size = bb->blockA.index;

		bb->writeR.size = size;
		*reserved = size;

		bb->writeR.index = 0;
		return bb->buffer;
	}

	/* block B exists */

	reservable = bb->blockA.index - bb->blockB.index - bb->blockB.size; /* space after block B */

	if (size < reservable)
		reservable = size;

	if (reservable == 0)
		return NULL;

	bb->writeR.size = reservable;
	*reserved = reservable;

	bb->writeR.index = bb->blockB.index + bb->blockB.size;
	return &bb->buffer[bb->writeR.index];
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
		BipBlock_Clear(bb->writeR);
		return;
	}

	if (size > bb->writeR.size)
		size = bb->writeR.size;

	if ((bb->blockA.size == 0) && (bb->blockB.size == 0))
	{
		bb->blockA.index = bb->writeR.index;
		bb->blockA.size = size;
		BipBlock_Clear(bb->writeR);
		return;
	}

	if (bb->writeR.index == (bb->blockA.size + bb->blockA.index))
		bb->blockA.size += size;
	else
		bb->blockB.size += size;

	BipBlock_Clear(bb->writeR);
}

int BipBuffer_Write(wBipBuffer* bb, BYTE* data, size_t size)
{
	int status = 0;
	BYTE* block = NULL;
	size_t writeSize = 0;
	size_t blockSize = 0;

	if (!bb)
		return -1;

	block = BipBuffer_WriteReserve(bb, size);

	if (!block)
		return -1;

	block = BipBuffer_WriteTryReserve(bb, size - status, &blockSize);

	if (block)
	{
		writeSize = size - status;

		if (writeSize > blockSize)
			writeSize = blockSize;

		CopyMemory(block, &data[status], writeSize);
		BipBuffer_WriteCommit(bb, writeSize);
		status += (int) writeSize;

		if ((status == size) || (writeSize < blockSize))
			return status;
	}

	block = BipBuffer_WriteTryReserve(bb, size - status, &blockSize);

	if (block)
	{
		writeSize = size - status;

		if (writeSize > blockSize)
			writeSize = blockSize;

		CopyMemory(block, &data[status], writeSize);
		BipBuffer_WriteCommit(bb, writeSize);
		status += (int) writeSize;

		if ((status == size) || (writeSize < blockSize))
			return status;
	}

	return status;
}

BYTE* BipBuffer_ReadTryReserve(wBipBuffer* bb, size_t size, size_t* reserved)
{
	size_t reservable = 0;

	if (!reserved)
		return NULL;

	if (bb->blockA.size == 0)
	{
		*reserved = 0;
		return NULL;
	}

	reservable = bb->blockA.size;

	if (size && (reservable > size))
		reservable = size;

	*reserved = reservable;
	return &bb->buffer[bb->blockA.index];
}

BYTE* BipBuffer_ReadReserve(wBipBuffer* bb, size_t size)
{
	BYTE* block = NULL;
	size_t reserved = 0;

	if (BipBuffer_UsedSize(bb) < size)
		return NULL;

	block = BipBuffer_ReadTryReserve(bb, size, &reserved);

	if (reserved == size)
		return block;

	if (!BipBuffer_Grow(bb, bb->size + 1))
		return NULL;

	block = BipBuffer_ReadTryReserve(bb, size, &reserved);

	if (reserved != size)
		return NULL;

	return block;
}

void BipBuffer_ReadCommit(wBipBuffer* bb, size_t size)
{
	if (!bb)
		return;

	if (size >= bb->blockA.size)
	{
		BipBlock_Copy(bb->blockA, bb->blockB);
		BipBlock_Clear(bb->blockB);
	}
	else
	{
		bb->blockA.size -= size;
		bb->blockA.index += size;
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
		status += (int) readSize;

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
		status += (int) readSize;

		if ((status == size) || (readSize < blockSize))
			return status;
	}

	return status;
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
