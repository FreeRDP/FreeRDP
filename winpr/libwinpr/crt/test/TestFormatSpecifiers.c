#include <stdio.h>
#include <winpr/wtypes.h>
#include <winpr/string.h>

int TestFormatSpecifiers(int argc, char* argv[])
{
	unsigned errors = 0;

	char fmt[4096];

	/* size_t */
	{
		size_t arg = 0xabcd;
		const char* chk = "uz:43981 oz:125715 xz:abcd Xz:ABCD";

		sprintf_s(fmt, sizeof(fmt), "uz:%" PRIuz " oz:%" PRIoz " xz:%" PRIxz " Xz:%" PRIXz "", arg,
		          arg, arg, arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed size_t test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	/* INT8 */
	{
		INT8 arg = -16;
		const char* chk = "d8:-16 x8:f0 X8:F0";

		sprintf_s(fmt, sizeof(fmt), "d8:%" PRId8 " x8:%" PRIx8 " X8:%" PRIX8 "", arg, (UINT8)arg,
		          (UINT8)arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed INT8 test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	/* UINT8 */
	{
		UINT8 arg = 0xFE;
		const char* chk = "u8:254 o8:376 x8:fe X8:FE";

		sprintf_s(fmt, sizeof(fmt), "u8:%" PRIu8 " o8:%" PRIo8 " x8:%" PRIx8 " X8:%" PRIX8 "", arg,
		          arg, arg, arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed UINT8 test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	/* INT16 */
	{
		INT16 arg = -16;
		const char* chk = "d16:-16 x16:fff0 X16:FFF0";

		sprintf_s(fmt, sizeof(fmt), "d16:%" PRId16 " x16:%" PRIx16 " X16:%" PRIX16 "", arg,
		          (UINT16)arg, (UINT16)arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed INT16 test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	/* UINT16 */
	{
		UINT16 arg = 0xFFFE;
		const char* chk = "u16:65534 o16:177776 x16:fffe X16:FFFE";

		sprintf_s(fmt, sizeof(fmt),
		          "u16:%" PRIu16 " o16:%" PRIo16 " x16:%" PRIx16 " X16:%" PRIX16 "", arg, arg, arg,
		          arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed UINT16 test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	/* INT32 */
	{
		INT32 arg = -16;
		const char* chk = "d32:-16 x32:fffffff0 X32:FFFFFFF0";

		sprintf_s(fmt, sizeof(fmt), "d32:%" PRId32 " x32:%" PRIx32 " X32:%" PRIX32 "", arg,
		          (UINT32)arg, (UINT32)arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed INT32 test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	/* UINT32 */
	{
		UINT32 arg = 0xFFFFFFFE;
		const char* chk = "u32:4294967294 o32:37777777776 x32:fffffffe X32:FFFFFFFE";

		sprintf_s(fmt, sizeof(fmt),
		          "u32:%" PRIu32 " o32:%" PRIo32 " x32:%" PRIx32 " X32:%" PRIX32 "", arg, arg, arg,
		          arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed UINT16 test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	/* INT64 */
	{
		INT64 arg = -16;
		const char* chk = "d64:-16 x64:fffffffffffffff0 X64:FFFFFFFFFFFFFFF0";

		sprintf_s(fmt, sizeof(fmt), "d64:%" PRId64 " x64:%" PRIx64 " X64:%" PRIX64 "", arg,
		          (UINT64)arg, (UINT64)arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed INT64 test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	/* UINT64 */
	{
		UINT64 arg = 0xFFFFFFFFFFFFFFFE;
		const char* chk = "u64:18446744073709551614 o64:1777777777777777777776 "
		                  "x64:fffffffffffffffe X64:FFFFFFFFFFFFFFFE";

		sprintf_s(fmt, sizeof(fmt),
		          "u64:%" PRIu64 " o64:%" PRIo64 " x64:%016" PRIx64 " X64:%016" PRIX64 "", arg, arg,
		          arg, arg);

		if (strcmp(fmt, chk))
		{
			fprintf(stderr, "%s failed UINT64 test: got [%s] instead of [%s]\n", __FUNCTION__, fmt,
			        chk);
			errors++;
		}
	}

	if (errors)
	{
		fprintf(stderr, "%s produced %u errors\n", __FUNCTION__, errors);
		return -1;
	}

	return 0;
}
