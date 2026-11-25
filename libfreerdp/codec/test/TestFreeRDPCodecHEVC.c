#include <stdio.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/hevc.h>

/* The encoder's bit rate, in bit/s. */
int TestFreeRDPCodecHEVC(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	FREERDP_HEVC_CONTEXT* hevc = freerdp_hevc_context_new(TRUE);
	if (!hevc)
	{
		(void)fprintf(stderr, "no HEVC encoder in this build\n");
		return 0;
	}

	int rc = -1;
	/* 500 kbit/s, given in bit/s */
	if (!freerdp_hevc_context_set_option(hevc, FREERDP_HEVC_CONTEXT_OPTION_BITRATE, 500000) ||
	    (freerdp_hevc_context_get_option(hevc, FREERDP_HEVC_CONTEXT_OPTION_BITRATE) != 500000))
	{
		(void)fprintf(stderr, "the bit rate is not taken\n");
		goto fail;
	}

	rc = 0;
fail:
	freerdp_hevc_context_free(hevc);
	return rc;
}
