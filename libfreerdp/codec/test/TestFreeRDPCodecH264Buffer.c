#include <string.h>

#include <winpr/crt.h>

#include "../h264.h"

static void freeYUV420Buffers(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);
	for (size_t x = 0; x < 3; x++)
	{
		winpr_aligned_free(h264->pYUVData[x]);
		winpr_aligned_free(h264->pOldYUVData[x]);
	}
}

static BOOL testYUV420BufferLayout(UINT32 sourceStride, UINT32 width, UINT32 height,
                                   UINT32 expectedStride, UINT32 expectedAllocationStride,
                                   UINT32 expectedAllocationHeight)
{
	H264_CONTEXT h264 = WINPR_C_ARRAY_INIT;
	if (!avc420_ensure_buffer(&h264, sourceStride, width, height))
		return FALSE;

	BOOL rc = h264.iStride[0] == expectedStride;
	rc = rc && h264.iStride[1] == (expectedStride + 1) / 2;
	rc = rc && h264.iStride[2] == (expectedStride + 1) / 2;

	const size_t expectedYSize = (size_t)expectedAllocationStride * expectedAllocationHeight;
	const size_t expectedUVSize =
	    (size_t)((expectedAllocationStride + 1) / 2) * expectedAllocationHeight;
	rc = rc && winpr_aligned_msize(h264.pYUVData[0], 16, 0) >= expectedYSize;
	rc = rc && winpr_aligned_msize(h264.pYUVData[1], 16, 0) >= expectedUVSize;
	rc = rc && winpr_aligned_msize(h264.pYUVData[2], 16, 0) >= expectedUVSize;

	freeYUV420Buffers(&h264);
	return rc;
}

static BOOL testYUV420Copy(void)
{
	const UINT32 width = 5;
	const UINT32 height = 3;
	const UINT32 srcStride = 8;
	const UINT32 srcChromaStride = 4;
	const UINT32 chromaHeight = 2;
	const size_t srcSize = (size_t)srcStride * height + 2ULL * srcChromaStride * chromaHeight;
	BYTE src[40] = WINPR_C_ARRAY_INIT;
	BYTE y[30] = WINPR_C_ARRAY_INIT;
	BYTE u[12] = WINPR_C_ARRAY_INIT;
	BYTE v[14] = WINPR_C_ARRAY_INIT;
	BYTE* dst[3] = { y, u, v };
	const UINT32 dstStride[3] = { 10, 6, 7 };

	WINPR_ASSERT(srcSize == sizeof(src));
	for (size_t x = 0; x < sizeof(src); x++)
		src[x] = (BYTE)(x + 1);

	size_t requiredSize = 0;
	if (!h264_copy_yuv420p(dst, dstStride, src, srcSize, srcStride, width, height, &requiredSize))
		return FALSE;
	if (requiredSize != srcSize)
		return FALSE;

	for (UINT32 row = 0; row < height; row++)
	{
		if (memcmp(&y[(size_t)row * dstStride[0]], &src[(size_t)row * srcStride], srcStride) != 0)
			return FALSE;
	}
	const size_t uOffset = (size_t)srcStride * height;
	const size_t vOffset = uOffset + (size_t)srcChromaStride * chromaHeight;
	for (UINT32 row = 0; row < chromaHeight; row++)
	{
		if (memcmp(&u[(size_t)row * dstStride[1]], &src[uOffset + (size_t)row * srcChromaStride],
		           srcChromaStride) != 0)
			return FALSE;
		if (memcmp(&v[(size_t)row * dstStride[2]], &src[vOffset + (size_t)row * srcChromaStride],
		           srcChromaStride) != 0)
			return FALSE;
	}

	return !h264_copy_yuv420p(dst, dstStride, src, srcSize - 1, srcStride, width, height,
	                          &requiredSize);
}

int TestFreeRDPCodecH264Buffer(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* An already aligned decoder stride remains the logical stride while allocation keeps the
	 * extra block required by decoders which pad aligned dimensions unconditionally. */
	if (!testYUV420BufferLayout(1808, 1804, 1128, 1808, 1824, 1136))
		return -1;
	/* Preserve the pre-existing alignment behavior for an unaligned requested stride. */
	if (!testYUV420BufferLayout(1804, 1804, 1128, 1808, 1808, 1136))
		return -1;
	/* Preserve the extra allocation row block for an already aligned height. */
	if (!testYUV420BufferLayout(1808, 1804, 1120, 1808, 1824, 1136))
		return -1;
	if (!testYUV420Copy())
		return -1;

	return 0;
}
