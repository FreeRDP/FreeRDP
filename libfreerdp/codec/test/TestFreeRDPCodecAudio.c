#include <stdio.h>

#include <winpr/crt.h>

#include <freerdp/codec/audio.h>

/* The filters rdpsnd and audin build from /sound:format: and /microphone:format:, all zero
 * without the option, and the one the resampler builds without a tag. */
static BOOL test_format_compatible(void)
{
	const AUDIO_FORMAT pcm = { .wFormatTag = WAVE_FORMAT_PCM,
		                       .nChannels = 2,
		                       .nSamplesPerSec = 44100,
		                       .nAvgBytesPerSec = 176400,
		                       .nBlockAlign = 4,
		                       .wBitsPerSample = 16 };
	AUDIO_FORMAT alaw = pcm;
	alaw.wFormatTag = WAVE_FORMAT_ALAW;
	AUDIO_FORMAT broken = pcm;
	broken.nSamplesPerSec = 0;

	const AUDIO_FORMAT any = WINPR_C_ARRAY_INIT;
	const AUDIO_FORMAT onlyPcm = { .wFormatTag = WAVE_FORMAT_PCM };
	const AUDIO_FORMAT only48k = { .nSamplesPerSec = 48000 };
	AUDIO_FORMAT noTag = pcm;
	noTag.wFormatTag = WAVE_FORMAT_UNKNOWN;
	noTag.wBitsPerSample = 0;

	BOOL rc = TRUE;
	const struct
	{
		const char* what;
		const AUDIO_FORMAT* with;
		const AUDIO_FORMAT* format;
		BOOL expected;
	} cases[] = {
		{ "no filter, PCM", &any, &pcm, TRUE },
		{ "no filter, A-law", &any, &alaw, TRUE },
		{ "PCM filter, PCM", &onlyPcm, &pcm, TRUE },
		{ "PCM filter, A-law", &onlyPcm, &alaw, FALSE },
		{ "48 kHz filter, 44.1 kHz", &only48k, &pcm, FALSE },
		{ "no tag, same rate", &noTag, &alaw, TRUE },
		{ "same format", &pcm, &pcm, TRUE },
		{ "no filter, no sample rate", &any, &broken, FALSE },
	};
	for (size_t i = 0; i < ARRAYSIZE(cases); i++)
	{
		const BOOL got = audio_format_compatible(cases[i].with, cases[i].format);
		if (got != cases[i].expected)
		{
			(void)fprintf(stderr, "[%s] %s: got %d, expected %d\n", __func__, cases[i].what, got,
			              cases[i].expected);
			rc = FALSE;
		}
	}
	return rc;
}

int TestFreeRDPCodecAudio(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!test_format_compatible())
		return -1;
	return 0;
}
