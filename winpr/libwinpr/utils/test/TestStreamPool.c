
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#define BUFFER_SIZE 16384

int TestStreamPool(int argc, char* argv[])
{
	wStream* s[5];
	wStreamPool* pool;
	char buffer[8192];

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	pool = StreamPool_New(TRUE, BUFFER_SIZE);

	s[0] = StreamPool_Take(pool, 0);
	s[1] = StreamPool_Take(pool, 0);
	s[2] = StreamPool_Take(pool, 0);

	printf("%s\n", StreamPool_GetStatistics(pool, buffer, sizeof(buffer)));

	Stream_Release(s[0]);
	Stream_Release(s[1]);
	Stream_Release(s[2]);

	printf("%s\n", StreamPool_GetStatistics(pool, buffer, sizeof(buffer)));

	s[3] = StreamPool_Take(pool, 0);
	s[4] = StreamPool_Take(pool, 0);

	printf("%s\n", StreamPool_GetStatistics(pool, buffer, sizeof(buffer)));

	Stream_Release(s[3]);
	Stream_Release(s[4]);

	printf("%s\n", StreamPool_GetStatistics(pool, buffer, sizeof(buffer)));

	s[2] = StreamPool_Take(pool, 0);
	s[3] = StreamPool_Take(pool, 0);
	s[4] = StreamPool_Take(pool, 0);

	printf("%s\n", StreamPool_GetStatistics(pool, buffer, sizeof(buffer)));

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

	printf("%s\n", StreamPool_GetStatistics(pool, buffer, sizeof(buffer)));

	s[2] = StreamPool_Take(pool, 0);
	s[3] = StreamPool_Take(pool, 0);
	s[4] = StreamPool_Take(pool, 0);

	printf("%s\n", StreamPool_GetStatistics(pool, buffer, sizeof(buffer)));

	StreamPool_Free(pool);

	return 0;
}
