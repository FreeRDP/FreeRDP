
#include <winpr/crt.h>
#include <winpr/collections.h>

int TestBipBuffer(int argc, char* argv[])
{
	BYTE* data;
	wBipBuffer* bb;
	size_t reserved = 0;

	bb = BipBuffer_New(1024);

	if (!bb)
		return -1;

	data = BipBuffer_Reserve(bb, 1024 * 2);

	fprintf(stderr, "BipBuffer_BufferSize: %d\n", (int) BipBuffer_BufferSize(bb));

	BipBuffer_Free(bb);

	return 0;
}
