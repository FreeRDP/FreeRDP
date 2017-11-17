
#include <winpr/crt.h>
#include <winpr/collections.h>

int TestBipBuffer(int argc, char* argv[])
{
	BYTE* data;
	int rc = -1;
	wBipBuffer* bb;
	bb = BipBuffer_New(1024);

	if (!bb)
		return -1;

	data = BipBuffer_WriteReserve(bb, 1024 * 2);

	if (data)
		rc = 0;

	fprintf(stderr, "BipBuffer_BufferSize: %"PRIuz"\n", BipBuffer_BufferSize(bb));
	BipBuffer_Free(bb);
	return rc;
}
