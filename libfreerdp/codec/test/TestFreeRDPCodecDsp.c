#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/codec/audio.h>
#include <freerdp/codec/dsp.h>

#define OPUS_TEST_FRAMES 960 /* 20 ms at 48 kHz */

static AUDIO_FORMAT opus_format(UINT32 samplesPerSec)
{
	const AUDIO_FORMAT format = { .wFormatTag = WAVE_FORMAT_OPUS,
		                          .nChannels = 2,
		                          .nSamplesPerSec = samplesPerSec,
		                          .nAvgBytesPerSec = 16000,
		                          .nBlockAlign = 4,
		                          .wBitsPerSample = 16 };
	return format;
}

static BOOL test_opus_samplerates(void)
{
	const AUDIO_FORMAT valid = opus_format(48000);
	const AUDIO_FORMAT invalid = opus_format(44100);
	FREERDP_DSP_CONTEXT* decoder = nullptr;
	BOOL rc = FALSE;

	if (!freerdp_dsp_supports_format(&valid, FALSE))
		goto fail;
	if (freerdp_dsp_supports_format(&invalid, FALSE))
		goto fail;

	decoder = freerdp_dsp_context_new(FALSE);
	if (!decoder)
		goto fail;
	rc = !freerdp_dsp_context_reset(decoder, &invalid, 0);

fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] failed\n", __func__);
	freerdp_dsp_context_free(decoder);
	return rc;
}

static BOOL test_opus_roundtrip(void)
{
	const AUDIO_FORMAT format = opus_format(48000);
	const AUDIO_FORMAT pcm = { .wFormatTag = WAVE_FORMAT_PCM,
		                       .nChannels = 2,
		                       .nSamplesPerSec = 48000,
		                       .nAvgBytesPerSec = 192000,
		                       .nBlockAlign = 4,
		                       .wBitsPerSample = 16 };
	INT16 samples[OPUS_TEST_FRAMES * 2] = WINPR_C_ARRAY_INIT;
	BOOL rc = FALSE;

	/* 1 kHz triangle wave */
	for (size_t x = 0; x < OPUS_TEST_FRAMES; x++)
	{
		const INT32 phase = (INT32)(x % 48);
		const INT16 v = (INT16)(((phase < 24) ? phase : 48 - phase) * 600 - 7200);
		samples[2 * x] = v;
		samples[2 * x + 1] = v;
	}

	FREERDP_DSP_CONTEXT* encoder = freerdp_dsp_context_new(TRUE);
	FREERDP_DSP_CONTEXT* decoder = freerdp_dsp_context_new(FALSE);
	wStream* encoded = Stream_New(nullptr, 4096);
	wStream* decoded = Stream_New(nullptr, 4096);
	if (!encoder || !decoder || !encoded || !decoded)
		goto fail;

	/* A context can be reset more than once, e.g. when the server changes the format */
	for (size_t x = 0; x < 3; x++)
	{
		if (!freerdp_dsp_context_reset(encoder, &format, OPUS_TEST_FRAMES))
			goto fail;
		if (!freerdp_dsp_context_reset(decoder, &format, 0))
			goto fail;
	}

	if (!freerdp_dsp_encode(encoder, &pcm, (const BYTE*)samples, sizeof(samples), encoded))
		goto fail;
	Stream_SealLength(encoded);
	if (Stream_Length(encoded) == 0)
		goto fail;

	if (!freerdp_dsp_decode(decoder, &format, Stream_Buffer(encoded), Stream_Length(encoded),
	                        decoded))
		goto fail;
	Stream_SealLength(decoded);
	rc = (Stream_Length(decoded) == sizeof(samples));

fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] failed\n", __func__);
	Stream_Free(encoded, TRUE);
	Stream_Free(decoded, TRUE);
	freerdp_dsp_context_free(encoder);
	freerdp_dsp_context_free(decoder);
	return rc;
}

/* Like audin: one stream reused for every packet, after Stream_SealLength() made it short, and
 * input in whatever amounts the capture device delivers. Every packet must be a valid one. */
static BOOL test_opus_chunks(void)
{
	const AUDIO_FORMAT format = opus_format(48000);
	const AUDIO_FORMAT pcm = { .wFormatTag = WAVE_FORMAT_PCM,
		                       .nChannels = 2,
		                       .nSamplesPerSec = 48000,
		                       .nAvgBytesPerSec = 192000,
		                       .nBlockAlign = 4,
		                       .wBitsPerSample = 16 };
	const size_t chunks[] = { 333, 1000, 2047, 7, 960, 1919, 441, 2880, 100, 3000 };
	INT16 samples[3000 * 2] = WINPR_C_ARRAY_INIT;
	size_t in_frames = 0;
	size_t out_frames = 0;
	size_t packets = 0;
	BOOL rc = FALSE;

	for (size_t x = 0; x < ARRAYSIZE(samples); x++)
		samples[x] = (INT16)((x % 96) * 300 - 14400);

	FREERDP_DSP_CONTEXT* encoder = freerdp_dsp_context_new(TRUE);
	FREERDP_DSP_CONTEXT* decoder = freerdp_dsp_context_new(FALSE);
	wStream* encoded = Stream_New(nullptr, 16);
	wStream* decoded = Stream_New(nullptr, 4096);
	if (!encoder || !decoder || !encoded || !decoded)
		goto fail;
	if (!freerdp_dsp_context_reset(encoder, &format, OPUS_TEST_FRAMES))
		goto fail;
	if (!freerdp_dsp_context_reset(decoder, &format, 0))
		goto fail;

	for (size_t round = 0; round < 3; round++)
	{
		for (size_t x = 0; x < ARRAYSIZE(chunks); x++)
		{
			Stream_ResetPosition(encoded);
			Stream_Write_UINT8(encoded, 0x06); /* a header byte, as MSG_SNDIN_DATA */
			if (!freerdp_dsp_encode(encoder, &pcm, (const BYTE*)samples, chunks[x] * 4, encoded))
				goto fail;
			in_frames += chunks[x];
			Stream_SealLength(encoded);
			if (Stream_Length(encoded) <= 1)
				continue;

			Stream_ResetPosition(decoded);
			if (!freerdp_dsp_decode(decoder, &format, Stream_Buffer(encoded) + 1,
			                        Stream_Length(encoded) - 1, decoded))
				goto fail;
			/* 10, 20, 40 or 60 ms: at least the packet size asked for */
			const size_t frames = Stream_GetPosition(decoded) / 4;
			if ((frames != 960) && (frames != 1920) && (frames != 2880))
			{
				(void)fprintf(stderr, "[%s] packet of %" PRIuz " frames\n", __func__, frames);
				goto fail;
			}
			out_frames += frames;
			packets++;
		}
	}

	/* everything but less than one packet went out */
	rc = (packets > 0) && (out_frames <= in_frames) && (in_frames - out_frames < 2880);
	if (!rc)
		(void)fprintf(stderr, "[%s] %" PRIuz " frames in, %" PRIuz " out\n", __func__, in_frames,
		              out_frames);

fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] failed\n", __func__);
	Stream_Free(encoded, TRUE);
	Stream_Free(decoded, TRUE);
	freerdp_dsp_context_free(encoder);
	freerdp_dsp_context_free(decoder);
	return rc;
}

static BOOL test_opus_other_format(void)
{
	const AUDIO_FORMAT opus = opus_format(48000);
	const AUDIO_FORMAT pcm = { .wFormatTag = WAVE_FORMAT_PCM,
		                       .nChannels = 2,
		                       .nSamplesPerSec = 48000,
		                       .nAvgBytesPerSec = 192000,
		                       .nBlockAlign = 4,
		                       .wBitsPerSample = 16 };
	const BYTE data[4] = WINPR_C_ARRAY_INIT;
	BOOL rc = FALSE;

	FREERDP_DSP_CONTEXT* decoder = freerdp_dsp_context_new(FALSE);
	wStream* decoded = Stream_New(nullptr, 4096);
	if (!decoder || !decoded)
		goto fail;

	/* switching from Opus to PCM must not keep (or leak) the Opus decoder */
	if (!freerdp_dsp_context_reset(decoder, &opus, 0))
		goto fail;
	if (!freerdp_dsp_context_reset(decoder, &pcm, 0))
		goto fail;
	rc = freerdp_dsp_decode(decoder, &pcm, data, sizeof(data), decoded);

fail:
	if (!rc)
		(void)fprintf(stderr, "[%s] failed\n", __func__);
	Stream_Free(decoded, TRUE);
	freerdp_dsp_context_free(decoder);
	return rc;
}

int TestFreeRDPCodecDsp(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	int rc = 0;

	if (!test_opus_samplerates())
		rc = -1;
	if (!test_opus_roundtrip())
		rc = -1;
	if (!test_opus_chunks())
		rc = -1;
	if (!test_opus_other_format())
		rc = -1;
	return rc;
}
