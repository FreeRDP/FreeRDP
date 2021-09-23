#include <stdio.h>

#include <winpr/stream.h>
#include <winpr/path.h>
#include <winpr/crypto.h>

#include <freerdp/freerdp.h>
#include <freerdp/streamdump.h>

static BOOL test_entry_read_write(void)
{
	BOOL rc = FALSE;
	FILE* fp = NULL;
	wStream *sw = NULL, *sr = NULL;
	size_t offset = 0, x;
	UINT64 ts = 0;
	BYTE tmp[16] = { 0 };
	char tmp2[64] = { 0 };
	char* name = NULL;
	size_t entrysize = sizeof(UINT64) + sizeof(UINT64);

	winpr_RAND(tmp, sizeof(tmp));

	for (x = 0; x < sizeof(tmp); x++)
		_snprintf(&tmp2[x * 2], sizeof(tmp2) - 2 * x, "%02" PRIx8, tmp[x]);
	name = GetKnownSubPath(KNOWN_PATH_TEMP, tmp2);
	if (!name)
	{
		fprintf(stderr, "[%s] Could not create temporary path\n", __FUNCTION__);
		goto fail;
	}

	sw = Stream_New(NULL, 8123);
	sr = Stream_New(NULL, 1024);
	if (!sr || !sw)
	{
		fprintf(stderr, "[%s] Could not create iostreams sw=%p, sr=%p\n", __FUNCTION__, sw, sr);
		goto fail;
	}

	winpr_RAND(Stream_Buffer(sw), Stream_Capacity(sw));
	entrysize += Stream_Capacity(sw);
	Stream_SetLength(sw, Stream_Capacity(sw));

	fp = fopen(name, "wb");
	if (!fp)
		goto fail;
	if (!stream_dump_write_line(fp, sw))
		goto fail;
	fclose(fp);

	fp = fopen(name, "rb");
	if (!fp)
		goto fail;
	if (!stream_dump_read_line(fp, sr, &ts, &offset))
		goto fail;

	if (entrysize != offset)
	{
		fprintf(stderr, "[%s] offset %" PRIuz " bytes, entrysize %" PRIuz " bytes\n", __FUNCTION__,
		        offset, entrysize);
		goto fail;
	}

	if (Stream_Length(sr) != Stream_Capacity(sw))
	{
		fprintf(stderr, "[%s] Written %" PRIuz " bytes, read %" PRIuz " bytes\n", __FUNCTION__,
		        Stream_Length(sr), Stream_Capacity(sw));
		goto fail;
	}

	if (memcmp(Stream_Buffer(sw), Stream_Buffer(sr), Stream_Capacity(sw)) != 0)
	{
		fprintf(stderr, "[%s] Written data does not match data read back\n", __FUNCTION__);
		goto fail;
	}
	rc = TRUE;
fail:
	Stream_Free(sr, TRUE);
	Stream_Free(sw, TRUE);
	fclose(fp);
	if (name)
		DeleteFileA(name);
	free(name);
	fprintf(stderr, "xxxxxxxxxxxxx %d\n", rc);
	return rc;
}

int TestStreamDump(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_entry_read_write())
		return -1;
	return 0;
}
