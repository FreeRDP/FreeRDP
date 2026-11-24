
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

int TestBufferPool(int argc, char* argv[])
{
	DWORD PoolSize = 0;
	SSIZE_T BufferSize = 0;
	wBufferPool* pool = nullptr;
	BYTE* Buffers[10] = WINPR_C_ARRAY_INIT;
	int DefaultSize = 1234;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	pool = BufferPool_New(TRUE, -1, 16);
	if (!pool)
		return -1;

	Buffers[0] = BufferPool_Take(pool, DefaultSize);
	Buffers[1] = BufferPool_Take(pool, DefaultSize);
	Buffers[2] = BufferPool_Take(pool, 2048);
	if (!Buffers[0] || !Buffers[1] || !Buffers[2])
		return -1;

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[0]);

	if (BufferSize != DefaultSize)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %" PRIdz " Expected: %" PRIu32 "\n",
		       BufferSize, DefaultSize);
		return -1;
	}

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[1]);

	if (BufferSize != DefaultSize)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %" PRIdz " Expected: %" PRIu32 "\n",
		       BufferSize, DefaultSize);
		return -1;
	}

	BufferSize = BufferPool_GetBufferSize(pool, Buffers[2]);

	if (BufferSize != 2048)
	{
		printf("BufferPool_GetBufferSize failure: Actual: %" PRIdz " Expected: 2048\n", BufferSize);
		return -1;
	}

	BufferPool_Return(pool, Buffers[1]);

	PoolSize = BufferPool_GetPoolSize(pool);

	if (PoolSize != 2)
	{
		printf("BufferPool_GetPoolSize failure: Actual: %" PRIu32 " Expected: 2\n", PoolSize);
		return -1;
	}

	BufferPool_Clear(pool);

	BufferPool_Free(pool);

	/* variable size pool: fill the array of buffers in use (initial capacity 32), then return the
	 * buffers first to last, so all entries after the removed one are shifted down each time */
	{
		BYTE* Many[32] = WINPR_C_ARRAY_INIT;
		const size_t count = ARRAYSIZE(Many);

		pool = BufferPool_New(TRUE, 0, 16);
		if (!pool)
			return -1;

		for (size_t x = 0; x < count; x++)
		{
			Many[x] = BufferPool_Take(pool, 64);
			if (!Many[x])
				return -1;
		}

		for (size_t x = 0; x < count; x++)
		{
			BufferPool_Return(pool, Many[x]);
			Many[x] = nullptr;
			PoolSize = BufferPool_GetPoolSize(pool);
			if (PoolSize != count - x - 1)
			{
				printf("BufferPool_GetPoolSize failure: Actual: %" PRIu32 " Expected: %" PRIuz "\n",
				       PoolSize, count - x - 1);
				return -1;
			}
		}

		BufferPool_Free(pool);
	}

	return 0;
}
