
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#define BUFFER_SIZE 16384

int TestStreamPool(int argc, char* argv[])
{
	wStream* s[5];
	wStreamPool* pool;

	pool = StreamPool_New(TRUE);

	s[0] = StreamPool_Take(pool, BUFFER_SIZE);
	s[1] = StreamPool_Take(pool, BUFFER_SIZE);
	s[2] = StreamPool_Take(pool, BUFFER_SIZE);

	printf("StreamPool: size: %d\n", pool->size);

	Stream_Release(s[0]);
	Stream_Release(s[1]);
	Stream_Release(s[2]);

	printf("StreamPool: size: %d\n", pool->size);

	s[3] = StreamPool_Take(pool, BUFFER_SIZE);
	s[4] = StreamPool_Take(pool, BUFFER_SIZE);

	printf("StreamPool: size: %d\n", pool->size);

	Stream_Release(s[3]);
	Stream_Release(s[4]);

	printf("StreamPool: size: %d\n", pool->size);

	s[2] = StreamPool_Take(pool, BUFFER_SIZE);
	s[3] = StreamPool_Take(pool, BUFFER_SIZE);
	s[4] = StreamPool_Take(pool, BUFFER_SIZE);

	printf("StreamPool: size: %d\n", pool->size);

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

	printf("StreamPool: size: %d\n", pool->size);

	StreamPool_Free(pool);

	return 0;
}

