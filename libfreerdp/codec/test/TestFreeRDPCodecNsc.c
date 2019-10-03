#include <winpr/crt.h>
#include <winpr/print.h>

#include <winpr/crypto.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/nsc.h>

static BOOL run_test(UINT32 width, UINT32 height, UINT32 format)
{
	BYTE* sdata = NULL;
	BYTE* sddata = NULL;
	BYTE* ddata = NULL;
	wStream* s = NULL;
	NSC_MESSAGE* msg = NULL;
	BOOL rc = FALSE;
	NSC_CONTEXT* ctx = NULL;
	UINT32 sx, sy, dx, dy;
	UINT32 sstride, dstride;
	UINT32 maxDataSize, numMsg, i;
	size_t src;
	const UINT16 bpp = (UINT16)GetBitsPerPixel(format);

	/* Create a random offset, limit to 128 */
	winpr_RAND((BYTE*)&sx, sizeof(sx));
	winpr_RAND((BYTE*)&sy, sizeof(sy));
	winpr_RAND((BYTE*)&dx, sizeof(dx));
	winpr_RAND((BYTE*)&dy, sizeof(dy));

	sx %= 128;
	sy %= 128;
	dx %= 128;
	dy %= 128;

	sstride = (sx+width) * GetBytesPerPixel(format);
	dstride = (dx+width) * GetBytesPerPixel(format);
	src = sstride * (sy + height);
	sdata = malloc(src);
	sddata = malloc(src);
	ddata = malloc(dstride * (dy + height));
	s = Stream_New(NULL, 128);

	if (!sdata || !ddata || !sddata || !s)
		goto fail;

	winpr_RAND(sdata, src);

	ctx = nsc_context_new();
	if (!ctx)
		goto fail;

	if (!nsc_context_set_pixel_format(ctx, format))
		goto fail;

	if (!nsc_context_reset(ctx, width, height))
		goto fail;

	/* MaxDataSize must be width / 256 * height / 128 rounded to next */
	maxDataSize = (width + 255) / 256;
	maxDataSize *= (height + 127) / 128;
	maxDataSize *= sizeof(NSC_MESSAGE);
	maxDataSize += 1024;
	msg = nsc_encode_messages(ctx, sdata, sx, sy, width, height, sstride, &numMsg, maxDataSize);
	if (!msg)
		goto fail;

	for (i=0; i<numMsg; i++)
	{
		if (!nsc_write_message(ctx, s, &msg[i]))
			goto fail;
	}

	if (!nsc_process_message(ctx, bpp, width, height,
							 Stream_Buffer(s), Stream_GetPosition(s),
							 ddata, format, dstride, dx, dy, width, height, 0))
		goto fail;
	if (!nsc_process_message(ctx, bpp, width, height,
							 Stream_Buffer(s), Stream_GetPosition(s),
							 sddata, format, sstride, sx, sy, width, height, 0))
		goto fail;

	if (memcmp(sddata, sdata, src) != 0)
		goto fail;

	rc = TRUE;

	fail:
		nsc_message_free(ctx, msg);
		nsc_context_free(ctx);
		Stream_Free(s, TRUE);
		free(sdata);
		free(ddata);
		free(sddata);
		return rc;
}

int TestFreeRDPCodecNsc(int argc, char* argv[])
{
	UINT32 w, h, x;
	const DWORD formats[] = {
		PIXEL_FORMAT_BGRA32,
		PIXEL_FORMAT_BGR24,
		PIXEL_FORMAT_BGR16,
		PIXEL_FORMAT_RGB8,
		PIXEL_FORMAT_A4
	};

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	winpr_RAND((BYTE*)&w, sizeof(w));
	winpr_RAND((BYTE*)&h, sizeof(h));
	w %= 8192;
	w += 16;
	h %= 8192;
	h += 16;

	for (x=0; x<ARRAYSIZE(formats); x++)
	{
		if (!run_test(w, h, formats[x]))
			return -1;
	}
	return 0;
}

