
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

int TestBufferPool(int argc, char* argv[])
{
	int status = -1;
	BOOL rc;
	size_t PoolSize;
	SSIZE_T BufferSize;
	size_t uBufferSize;
	wBufferPool* pool;
	BYTE* Buffers[10];
	size_t DefaultSize = 1234;
	pool = BufferPool_New(TRUE, -1, 16);

	if (!pool)
		return -1;

	Buffers[0] = BufferPool_Take(pool, DefaultSize);
	Buffers[1] = BufferPool_Take(pool, DefaultSize);
	Buffers[2] = BufferPool_Take(pool, 2048);

	if (!Buffers[0] || !Buffers[1] || !Buffers[2])
		goto fail;

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[0]);

	if (BufferSize < 0)
		goto fail;

	uBufferSize = (size_t)BufferSize;

	if (uBufferSize != DefaultSize)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %"PRIdz" Expected: %"PRIuz"\n", BufferSize,
		       DefaultSize);
		goto fail;
	}

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[1]);

	if (uBufferSize != DefaultSize)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %"PRIdz" Expected: %"PRIuz"\n", BufferSize,
		       DefaultSize);
		goto fail;
	}

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[2]);

	if (BufferSize != 2048)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %"PRIdz" Expected: 2048\n", BufferSize);
		goto fail;
	}

	rc = BufferPool_Return(pool, Buffers[1]);

	if (!rc)
	{
		goto fail;
	}

	PoolSize = BufferPool_GetPoolSize(pool);

	if (PoolSize != 2)
	{
		printf("BufferPool_GetPoolSize failure: Actual: %"PRIuz" Expected: 2\n", PoolSize);
		goto fail;
	}

	status = 0;
fail:
	BufferPool_Clear(pool);
	BufferPool_Free(pool);
	return status;
}

