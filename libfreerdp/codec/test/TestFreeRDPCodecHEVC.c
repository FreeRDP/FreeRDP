#include <stdio.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/hevc.h>

/* The encoder's options: the rate control it can do, and the bit rate in bit/s. */
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
	if (!freerdp_hevc_context_set_option(hevc, FREERDP_HEVC_CONTEXT_OPTION_RATECONTROL,
	                                     FREERDP_HEVC_VBR) ||
	    (freerdp_hevc_context_get_option(hevc, FREERDP_HEVC_CONTEXT_OPTION_RATECONTROL) !=
	     FREERDP_HEVC_VBR))
	{
		(void)fprintf(stderr, "VBR is not taken\n");
		goto fail;
	}

	/* the other modes have no counterpart in the encoder: refused, VBR stays */
	const FREERDP_HEVC_RATECONTROL others[] = { FREERDP_HEVC_CBR, FREERDP_HEVC_CQ, FREERDP_HEVC_Q };
	for (size_t x = 0; x < ARRAYSIZE(others); x++)
	{
		if (freerdp_hevc_context_set_option(hevc, FREERDP_HEVC_CONTEXT_OPTION_RATECONTROL,
		                                    others[x]))
		{
			(void)fprintf(stderr, "rate control %d is taken, it should be refused\n", others[x]);
			goto fail;
		}
		const UINT32 mode =
		    freerdp_hevc_context_get_option(hevc, FREERDP_HEVC_CONTEXT_OPTION_RATECONTROL);
		if (mode != FREERDP_HEVC_VBR)
		{
			(void)fprintf(stderr, "rate control %d changed the mode to %" PRIu32 "\n", others[x],
			              mode);
			goto fail;
		}
	}

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
