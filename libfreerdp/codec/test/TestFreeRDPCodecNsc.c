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
	BOOL rc = FALSE;
	NSC_CONTEXT* ctx = NULL;
	UINT32 dx, dy;
	UINT32 sstride, dstride;
	size_t src;

	/* Create a random offset, limit to 128 */
	winpr_RAND((BYTE*)&dx, sizeof(dx));
	winpr_RAND((BYTE*)&dy, sizeof(dy));

	dx %= 128;
	dy %= 128;

	sstride = (width) * GetBytesPerPixel(format);
	dstride = (dx+width) * GetBytesPerPixel(format);
	src = sstride * (height);
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

	if (!nsc_compose_message(ctx, s, sdata, width, height, sstride))
		goto fail;

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);

	if (!nsc_decompose_message(ctx, s, sddata, 0, 0, width, height, sstride, format, FREERDP_FLIP_NONE))
		goto fail;

	if (memcmp(sddata, sdata, src) != 0)
	{
		winpr_HexDump("src", WLOG_ERROR, sdata, sstride);
		winpr_HexDump("dec", WLOG_ERROR, sddata, sstride);
		goto fail;
	}

	rc = TRUE;

	fail:
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

	w = h = 16;
	for (x=0; x<ARRAYSIZE(formats); x++)
	{
		if (!run_test(w, h, formats[x]))
			return -1;
	}
	return 0;
}

