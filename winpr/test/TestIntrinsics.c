#include <winpr/crt.h>
#include <winpr/sysinfo.h>
#include <winpr/windows.h>

#include <winpr/intrin.h>

static BOOL g_LZCNT = FALSE;

static INLINE UINT32 lzcnt_s(UINT32 x)
{
	if (!x)
		return 32;

	if (!g_LZCNT)
	{
		UINT32 y;
		int n = 32;
		y = x >> 16;
		if (y != 0)
		{
			n = n - 16;
			x = y;
		}
		y = x >> 8;
		if (y != 0)
		{
			n = n - 8;
			x = y;
		}
		y = x >> 4;
		if (y != 0)
		{
			n = n - 4;
			x = y;
		}
		y = x >> 2;
		if (y != 0)
		{
			n = n - 2;
			x = y;
		}
		y = x >> 1;
		if (y != 0)
			return n - 2;
		return n - x;
	}

	return __lzcnt(x);
}

int test_lzcnt()
{
	if (lzcnt_s(0x1) != 31)
	{
		fprintf(stderr, "__lzcnt(0x1) != 31: %" PRIu32 "\n", __lzcnt(0x1));
		return -1;
	}

	if (lzcnt_s(0xFF) != 24)
	{
		fprintf(stderr, "__lzcnt(0xFF) != 24\n");
		return -1;
	}

	if (lzcnt_s(0xFFFF) != 16)
	{
		fprintf(stderr, "__lzcnt(0xFFFF) != 16\n");
		return -1;
	}

	if (lzcnt_s(0xFFFFFF) != 8)
	{
		fprintf(stderr, "__lzcnt(0xFFFFFF) != 8\n");
		return -1;
	}

	if (lzcnt_s(0xFFFFFFFF) != 0)
	{
		fprintf(stderr, "__lzcnt(0xFFFFFFFF) != 0\n");
		return -1;
	}

	return 0;
}

int test_lzcnt16()
{
	if (__lzcnt16(0x1) != 15)
	{
		fprintf(stderr, "__lzcnt16(0x1) != 15\n");
		return -1;
	}

	if (__lzcnt16(0xFF) != 8)
	{
		fprintf(stderr, "__lzcnt16(0xFF) != 8\n");
		return -1;
	}

	if (__lzcnt16(0xFFFF) != 0)
	{
		fprintf(stderr, "__lzcnt16(0xFFFF) != 0\n");
		return -1;
	}

	return 0;
}

int TestIntrinsics(int argc, char* argv[])
{
	g_LZCNT = IsProcessorFeaturePresentEx(PF_EX_LZCNT);

	printf("LZCNT available: %" PRId32 "\n", g_LZCNT);

	// test_lzcnt16();
	return test_lzcnt();
}
