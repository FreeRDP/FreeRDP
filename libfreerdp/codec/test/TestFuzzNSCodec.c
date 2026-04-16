/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for the NSCodec decoder
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/nsc.h>

static int fuzz_nsc_message(UINT16 bpp, UINT32 width, UINT32 height, UINT32 dstFormat,
                            const uint8_t* data, size_t size)
{
	SSIZE_T stride = 0;
	BYTE* dst = NULL;
	NSC_CONTEXT* ctx = NULL;

	if (size > UINT32_MAX)
		return 0;

	ctx = nsc_context_new();
	if (!ctx)
		return 0;

	stride = (SSIZE_T)width * FreeRDPGetBytesPerPixel(dstFormat);
	if (stride <= 0)
		goto fail;

	dst = calloc((size_t)height, (size_t)stride);
	if (!dst)
		goto fail;

	(void)nsc_process_message(ctx, bpp, width, height, data, (UINT32)size, dst, dstFormat,
	                          (UINT32)stride, 0, 0, width, height, FREERDP_FLIP_NONE);

fail:
	free(dst);
	nsc_context_free(ctx);
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	static BOOL loggingInitialized = FALSE;

	if (!loggingInitialized)
	{
		(void)WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
		loggingInitialized = TRUE;
	}

	if (size < 20)
		return 0;

	(void)fuzz_nsc_message(32, 64, 64, PIXEL_FORMAT_BGRA32, data, size);
	(void)fuzz_nsc_message(24, 32, 32, PIXEL_FORMAT_RGBX32, data, size);
	(void)fuzz_nsc_message(16, 17, 13, PIXEL_FORMAT_RGB16, data, size);
	return 0;
}
