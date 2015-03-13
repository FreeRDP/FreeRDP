
#include <winpr/crt.h>
#include <winpr/collections.h>

int TestBipBuffer(int argc, char* argv[])
{
	BYTE* data;
	wBipBuffer* bb;

	bb = BipBuffer_New(1024);

	if (!bb)
		return -1;

	data = BipBuffer_WriteReserve(bb, 1024 * 2);

	fprintf(stderr, "BipBuffer_BufferSize: %d\n", (int) BipBuffer_BufferSize(bb));

	BipBuffer_Free(bb);

	return 0;
}
