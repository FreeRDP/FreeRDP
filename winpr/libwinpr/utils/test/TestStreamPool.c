
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#define BUFFER_SIZE 16384

int TestStreamPool(int argc, char* argv[])
{
	wStream* s[5];
	wStreamPool* pool;

	pool = StreamPool_New(TRUE, BUFFER_SIZE);

	s[0] = StreamPool_Take(pool, 0);
	s[1] = StreamPool_Take(pool, 0);
	s[2] = StreamPool_Take(pool, 0);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	Stream_Release(s[0]);
	Stream_Release(s[1]);
	Stream_Release(s[2]);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	s[3] = StreamPool_Take(pool, 0);
	s[4] = StreamPool_Take(pool, 0);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	Stream_Release(s[3]);
	Stream_Release(s[4]);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	s[2] = StreamPool_Take(pool, 0);
	s[3] = StreamPool_Take(pool, 0);
	s[4] = StreamPool_Take(pool, 0);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	Stream_AddRef(s[2]);

	Stream_AddRef(s[3]);
	Stream_AddRef(s[3]);

	Stream_AddRef(s[4]);
	Stream_AddRef(s[4]);
	Stream_AddRef(s[4]);

	Stream_Release(s[2]);
	Stream_Release(s[2]);

	Stream_Release(s[3]);
	Stream_Release(s[3]);
	Stream_Release(s[3]);

	Stream_Release(s[4]);
	Stream_Release(s[4]);
	Stream_Release(s[4]);
	Stream_Release(s[4]);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	s[2] = StreamPool_Take(pool, 0);
	s[3] = StreamPool_Take(pool, 0);
	s[4] = StreamPool_Take(pool, 0);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	StreamPool_AddRef(pool, s[2]->buffer + 1024);

	StreamPool_AddRef(pool, s[3]->buffer + 1024);
	StreamPool_AddRef(pool, s[3]->buffer + 1024 * 2);

	StreamPool_AddRef(pool, s[4]->buffer + 1024);
	StreamPool_AddRef(pool, s[4]->buffer + 1024 * 2);
	StreamPool_AddRef(pool, s[4]->buffer + 1024 * 3);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	StreamPool_Release(pool, s[2]->buffer + 2048);
	StreamPool_Release(pool, s[2]->buffer + 2048 * 2);

	StreamPool_Release(pool, s[3]->buffer + 2048);
	StreamPool_Release(pool, s[3]->buffer + 2048 * 2);
	StreamPool_Release(pool, s[3]->buffer + 2048 * 3);

	StreamPool_Release(pool, s[4]->buffer + 2048);
	StreamPool_Release(pool, s[4]->buffer + 2048 * 2);
	StreamPool_Release(pool, s[4]->buffer + 2048 * 3);
	StreamPool_Release(pool, s[4]->buffer + 2048 * 4);

	printf("StreamPool: aSize: %d uSize: %d\n", pool->aSize, pool->uSize);

	StreamPool_Free(pool);

	return 0;
}

