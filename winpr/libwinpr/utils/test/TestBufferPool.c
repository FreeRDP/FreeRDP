
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

int TestBufferPool(int argc, char* argv[])
{
	DWORD PoolSize;
	int BufferSize;
	wBufferPool* pool;
	BYTE* Buffers[10];
	DWORD DefaultSize = 1234;

	pool = BufferPool_New(TRUE, -1, 16);

	Buffers[0] = BufferPool_Take(pool, DefaultSize);
	Buffers[1] = BufferPool_Take(pool, DefaultSize);
	Buffers[2] = BufferPool_Take(pool, 2048);

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[0]);

	if (BufferSize != DefaultSize)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %d Expected: %d\n", BufferSize, DefaultSize);
		return -1;
	}

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[1]);

	if (BufferSize != DefaultSize)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %d Expected: %d\n", BufferSize, DefaultSize);
		return -1;
	}

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[2]);

	if (BufferSize != 2048)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %d Expected: %d\n", BufferSize, 2048);
		return -1;
	}

	BufferPool_Return(pool, Buffers[1]);

	PoolSize = BufferPool_GetPoolSize(pool);

	if (PoolSize != 2)
	{
		printf("BufferPool_GetPoolSize failure: Actual: %d Expected: %d\n", PoolSize, 2);
		return -1;
	}

	BufferPool_Clear(pool);

	BufferPool_Free(pool);

	return 0;
}

