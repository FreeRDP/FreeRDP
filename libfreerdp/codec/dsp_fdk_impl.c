/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>

#include <fdk-aac/aacdecoder_lib.h>
#include <fdk-aac/aacenc_lib.h>

#include "dsp_fdk_impl.h"

#define WLOG_TRACE 0
#define WLOG_DEBUG 1
#define WLOG_INFO 2
#define WLOG_WARN 3
#define WLOG_ERROR 4
#define WLOG_FATAL 5

static const char* enc_err_str(AACENC_ERROR err)
{
	switch (err)
	{
		case AACENC_OK:
			return "AACENC_OK";
		case AACENC_INVALID_HANDLE:
			return "AACENC_INVALID_HANDLE";
		case AACENC_MEMORY_ERROR:
			return "AACENC_MEMORY_ERROR";
		case AACENC_UNSUPPORTED_PARAMETER:
			return "AACENC_UNSUPPORTED_PARAMETER";
		case AACENC_INVALID_CONFIG:
			return "AACENC_INVALID_CONFIG";
		case AACENC_INIT_ERROR:
			return "AACENC_INIT_ERROR";
		case AACENC_INIT_AAC_ERROR:
			return "AACENC_INIT_AAC_ERROR";
		case AACENC_INIT_SBR_ERROR:
			return "AACENC_INIT_SBR_ERROR";
		case AACENC_INIT_TP_ERROR:
			return "AACENC_INIT_TP_ERROR";
		case AACENC_INIT_META_ERROR:
			return "AACENC_INIT_META_ERROR";
#ifdef AACENC_INIT_MPS_ERROR
		case AACENC_INIT_MPS_ERROR:
			return "AACENC_INIT_MPS_ERROR";
#endif
		case AACENC_ENCODE_ERROR:
			return "AACENC_ENCODE_ERROR";
		case AACENC_ENCODE_EOF:
			return "AACENC_ENCODE_EOF";
		default:
			return "AACENC_UNKNOWN";
	}
}

static const char* dec_err_str(AAC_DECODER_ERROR err)
{
	switch (err)
	{
		case AAC_DEC_OK:
			return "AAC_DEC_OK";
		case AAC_DEC_OUT_OF_MEMORY:
			return "AAC_DEC_OUT_OF_MEMORY";
		case AAC_DEC_UNKNOWN:
			return "AAC_DEC_UNKNOWN";
		case aac_dec_sync_error_start:
			return "aac_dec_sync_error_start";
		case AAC_DEC_TRANSPORT_SYNC_ERROR:
			return "AAC_DEC_TRANSPORT_SYNC_ERROR";
		case AAC_DEC_NOT_ENOUGH_BITS:
			return "AAC_DEC_NOT_ENOUGH_BITS";
		case aac_dec_sync_error_end:
			return "aac_dec_sync_error_end";
		case aac_dec_init_error_start:
			return "aac_dec_init_error_start";
		case AAC_DEC_INVALID_HANDLE:
			return "AAC_DEC_INVALID_HANDLE";
		case AAC_DEC_UNSUPPORTED_FORMAT:
			return "AAC_DEC_UNSUPPORTED_FORMAT";
		case AAC_DEC_UNSUPPORTED_ER_FORMAT:
			return "AAC_DEC_UNSUPPORTED_ER_FORMAT";
		case AAC_DEC_UNSUPPORTED_EPCONFIG:
			return "AAC_DEC_UNSUPPORTED_EPCONFIG";
		case AAC_DEC_UNSUPPORTED_MULTILAYER:
			return "AAC_DEC_UNSUPPORTED_MULTILAYER";
		case AAC_DEC_UNSUPPORTED_CHANNELCONFIG:
			return "AAC_DEC_UNSUPPORTED_CHANNELCONFIG";
		case AAC_DEC_UNSUPPORTED_SAMPLINGRATE:
			return "AAC_DEC_UNSUPPORTED_SAMPLINGRATE";
		case AAC_DEC_INVALID_SBR_CONFIG:
			return "AAC_DEC_INVALID_SBR_CONFIG";
		case AAC_DEC_SET_PARAM_FAIL:
			return "AAC_DEC_SET_PARAM_FAIL";
		case AAC_DEC_NEED_TO_RESTART:
			return "AAC_DEC_NEED_TO_RESTART";
		case AAC_DEC_OUTPUT_BUFFER_TOO_SMALL:
			return "AAC_DEC_OUTPUT_BUFFER_TOO_SMALL";
		case aac_dec_init_error_end:
			return "aac_dec_init_error_end";
		case aac_dec_decode_error_start:
			return "aac_dec_decode_error_start";
		case AAC_DEC_TRANSPORT_ERROR:
			return "AAC_DEC_TRANSPORT_ERROR";
		case AAC_DEC_PARSE_ERROR:
			return "AAC_DEC_PARSE_ERROR";
		case AAC_DEC_UNSUPPORTED_EXTENSION_PAYLOAD:
			return "AAC_DEC_UNSUPPORTED_EXTENSION_PAYLOAD";
		case AAC_DEC_DECODE_FRAME_ERROR:
			return "AAC_DEC_DECODE_FRAME_ERROR";
		case AAC_DEC_CRC_ERROR:
			return "AAC_DEC_CRC_ERROR";
		case AAC_DEC_INVALID_CODE_BOOK:
			return "AAC_DEC_INVALID_CODE_BOOK";
		case AAC_DEC_UNSUPPORTED_PREDICTION:
			return "AAC_DEC_UNSUPPORTED_PREDICTION";
		case AAC_DEC_UNSUPPORTED_CCE:
			return "AAC_DEC_UNSUPPORTED_CCE";
		case AAC_DEC_UNSUPPORTED_LFE:
			return "AAC_DEC_UNSUPPORTED_LFE";
		case AAC_DEC_UNSUPPORTED_GAIN_CONTROL_DATA:
			return "AAC_DEC_UNSUPPORTED_GAIN_CONTROL_DATA";
		case AAC_DEC_UNSUPPORTED_SBA:
			return "AAC_DEC_UNSUPPORTED_SBA";
		case AAC_DEC_TNS_READ_ERROR:
			return "AAC_DEC_TNS_READ_ERROR";
		case AAC_DEC_RVLC_ERROR:
			return "AAC_DEC_RVLC_ERROR";
		case aac_dec_decode_error_end:
			return "aac_dec_decode_error_end";
		case aac_dec_anc_data_error_start:
			return "aac_dec_anc_data_error_start";
		case AAC_DEC_ANC_DATA_ERROR:
			return "AAC_DEC_ANC_DATA_ERROR";
		case AAC_DEC_TOO_SMALL_ANC_BUFFER:
			return "AAC_DEC_TOO_SMALL_ANC_BUFFER";
		case AAC_DEC_TOO_MANY_ANC_ELEMENTS:
			return "AAC_DEC_TOO_MANY_ANC_ELEMENTS";
		case aac_dec_anc_data_error_end:
			return "aac_dec_anc_data_error_end";
		default:
			return "AAC_DEC unknown value";
	}
}

static void log_dec_info(const CStreamInfo* info, void (*log)(const char* fmt, ...))
{
	assert(info);
	assert(log);

	log("info:"
	    "aacSampleRate: %d, "
	    "frameSize: %d, "
	    "numChannels: %d, "
	    "pChannelType: %p, "
	    "pChannelIndices: %p, "
	    "aacSampleRate: %d, "
	    "profile: %d, "
	    "aot: %d, " /* TODO: Enum 2 string */
	    "channelConfig: %d, "
	    "bitRate: %d, "
	    "aacSamplesPerFrame: %d, "
	    "aacNumChannels: %d, "
	    "extAot: %d" /* TODO: Enum 2 string */
	    "extSamplingRate: %d, "
	    "outputDelay: %u, "
	    "flags: %u, "
	    "epConfig: %d, "
	    "numLostAccessUnits: %d, "
	    "numTotalBytes: %" PRIu64 ", "
	    "numBadBytes: %" PRIu64 ", "
	    "numTotalAccessUnits: %" PRIu64 ", "
	    "numBadAccessUnits: %" PRIu64 ", "
	    "drcProgRefLev: %d, "
	    "drcPresMode: %d, ",
	    info->aacSampleRate, info->frameSize, info->numChannels, info->pChannelType,
	    info->pChannelIndices, info->aacSampleRate, info->profile, info->aot, info->channelConfig,
	    info->bitRate, info->aacSamplesPerFrame, info->aacNumChannels, info->extAot,
	    info->extSamplingRate, info->outputDelay, info->flags, (int)info->epConfig,
	    info->numLostAccessUnits,

	    info->numTotalBytes, info->numBadBytes, info->numTotalAccessUnits, info->numBadAccessUnits,

	    (int)info->drcProgRefLev, (int)info->drcPresMode);
}

static void log_enc_info(const AACENC_InfoStruct* info, fdk_log_fkt_t log)
{
	char confBuf[1024] = { 0 };

	assert(info);
	assert(log);

	size_t offset = 0;
	size_t remain = sizeof(confBuf) - 1;
	int rc = snprintf(confBuf, remain, "{");
	if (rc <= 0)
		return;
	offset += (size_t)rc;

	for (size_t x = 0; x < 64; x++)
	{
		rc = snprintf(&confBuf[offset], remain - offset, "0x%02x%s", (int)info->confBuf[x],
		              (x > 0) ? ", " : "");
		if (rc <= 0)
			return;
	}

	rc = snprintf(confBuf, remain - offset, "}");
	if (rc <= 0)
		return;

	log(WLOG_DEBUG,
	    "[encoder info] "
	    "maxOutBufBytes : %u, "
	    "maxAncBytes    : %u, "
	    "inBufFillLevel : %u, "
	    "inputChannels  : %u, "
	    "frameLength    : %u, "
#ifdef MODE_7_1_BACK
	    "nDelay         : %u, "
	    "nDelayCore     : %u, "
#endif
	    "confBuf[64]    : %s, "
	    "confSize       : %u",
	    info->maxOutBufBytes, info->maxAncBytes, info->inBufFillLevel, info->inputChannels,
	    info->frameLength,
#ifdef MODE_7_1_BACK
	    info->nDelay, info->nDelayCore,
#endif
	    confBuf, info->confSize);
}

static const char* aac_enc_param_str(AACENC_PARAM param)
{
	switch (param)
	{
		case AACENC_AOT:
			return "AACENC_AOT";
		case AACENC_BITRATE:
			return "AACENC_BITRATE";
		case AACENC_BITRATEMODE:
			return "AACENC_BITRATEMODE";
		case AACENC_SAMPLERATE:
			return "AACENC_SAMPLERATE";
		case AACENC_SBR_MODE:
			return "AACENC_SBR_MODE";
		case AACENC_GRANULE_LENGTH:
			return "AACENC_GRANULE_LENGTH";
		case AACENC_CHANNELMODE:
			return "AACENC_CHANNELMODE";
		case AACENC_CHANNELORDER:
			return "AACENC_CHANNELORDER";
		case AACENC_SBR_RATIO:
			return "AACENC_SBR_RATIO";
		case AACENC_AFTERBURNER:
			return "AACENC_AFTERBURNER";
		case AACENC_BANDWIDTH:
			return "AACENC_BANDWIDTH";
		case AACENC_PEAK_BITRATE:
			return "AACENC_PEAK_BITRATE";
		case AACENC_TRANSMUX:
			return "AACENC_TRANSMUX";
		case AACENC_HEADER_PERIOD:
			return "AACENC_HEADER_PERIOD";
		case AACENC_SIGNALING_MODE:
			return "AACENC_SIGNALING_MODE";
		case AACENC_TPSUBFRAMES:
			return "AACENC_TPSUBFRAMES";
		case AACENC_AUDIOMUXVER:
			return "AACENC_AUDIOMUXVER";
		case AACENC_PROTECTION:
			return "AACENC_PROTECTION";
		case AACENC_ANCILLARY_BITRATE:
			return "AACENC_ANCILLARY_BITRATE";
		case AACENC_METADATA_MODE:
			return "AACENC_METADATA_MODE";
		case AACENC_CONTROL_STATE:
			return "AACENC_CONTROL_STATE";
		default:
			return "AACENC_UNKNOWN";
	}
}

int fdk_aac_dsp_impl_init(void** handle, int encoder, fdk_log_fkt_t log)
{
	assert(handle);
	assert(log);

	if (encoder)
	{
		HANDLE_AACENCODER* h = (HANDLE_AACENCODER*)handle;
		AACENC_ERROR err = aacEncOpen(h, 0, 0);
		if (err != AACENC_OK)
		{
			log(WLOG_ERROR, "aacEncOpen failed with %s", enc_err_str(err));
			return 0;
		}
	}
	else
	{
		HANDLE_AACDECODER* h = (HANDLE_AACDECODER*)handle;
		assert(NULL == *h);

		*h = aacDecoder_Open(TT_MP4_RAW, 1);
		if (!*h)
		{
			log(WLOG_ERROR, "aacDecoder_Open failed");
			return 0;
		}
	}
	return 1;
}

void fdk_aac_dsp_impl_uninit(void** handle, int encoder, fdk_log_fkt_t log)
{
	assert(handle);
	assert(log);

	if (encoder)
	{
		HANDLE_AACENCODER* h = (HANDLE_AACENCODER*)handle;
		AACENC_ERROR err = aacEncClose(h);
		if (err != AACENC_OK)
			log(WLOG_ERROR, "aacEncClose failed with %s", enc_err_str(err));
	}
	else
	{
		HANDLE_AACDECODER* h = (HANDLE_AACDECODER*)handle;
		if (h)
			aacDecoder_Close(*h);
	}

	*handle = NULL;
}

ssize_t fdk_aac_dsp_impl_decode_read(void* handle, void* dst, size_t dstSize, fdk_log_fkt_t log)
{
	assert(handle);
	assert((dstSize / sizeof(INT_PCM)) <= INT_MAX);

	const INT nrsamples = (INT)(dstSize / sizeof(INT_PCM));
	UINT flags = 0;
	HANDLE_AACDECODER self = (HANDLE_AACDECODER)handle;
	AAC_DECODER_ERROR err = aacDecoder_DecodeFrame(self, dst, nrsamples, flags);
	switch (err)
	{
		case AAC_DEC_OK:
			return fdk_aac_dsp_impl_stream_info(handle, 0, log);
		case AAC_DEC_NOT_ENOUGH_BITS:
			return 0;
		default:
			log(WLOG_ERROR, "aacDecoder_DecodeFrame failed with %s", dec_err_str(err));
			return -1;
	}
}

static unsigned get_channelmode(unsigned channels)
{
	switch (channels)
	{
		case 1:
			return MODE_1;
		case 2:
			return MODE_2;
		case 3:
			return MODE_1_2;
		case 4:
			return MODE_1_2_1;
		case 5:
			return MODE_1_2_2;
		case 6:
			return MODE_1_2_2_1;
		case 7:
			return MODE_1_2_2_2_1;
#ifdef MODE_7_1_BACK
		case 8:
			return MODE_7_1_BACK;
#endif

		default:
			return MODE_2;
	}
}

int fdk_aac_dsp_impl_config(void* handle, size_t* pbuffersize, int encoder, unsigned samplerate,
                            unsigned channels, unsigned bytes_per_second,
                            unsigned frames_per_packet, fdk_log_fkt_t log)
{
	assert(handle);
	assert(log);
	assert(pbuffersize);

	log(WLOG_DEBUG,
	    "fdk_aac_dsp_impl_config: samplerate: %ld, channels: %ld, bytes_pers_second: %ld",
	    samplerate, channels, bytes_per_second);

	struct t_param_pair
	{
		AACENC_PARAM param;
		UINT value;
	};

	const struct t_param_pair params[] = { { AACENC_AOT, 2 },
		                                   { AACENC_SAMPLERATE, samplerate },
		                                   { AACENC_CHANNELMODE, get_channelmode(channels) },
		                                   { AACENC_CHANNELORDER, 0 },
		                                   { AACENC_BITRATE, bytes_per_second * 8 },
		                                   { AACENC_TRANSMUX, 0 },
		                                   { AACENC_AFTERBURNER, 1 } };
	HANDLE_AACENCODER self = NULL;
	if (encoder)
		self = (HANDLE_AACENCODER)handle;
	else
	{
		AACENC_ERROR err = aacEncOpen(&self, 0, channels);
		if (err != AACENC_OK)
		{
			log(WLOG_ERROR, "aacEncOpen failed with %s", enc_err_str(err));
			return -1;
		}
	}

	for (size_t x = 0; x < sizeof(params) / sizeof(params[0]); x++)
	{
		const struct t_param_pair* param = &params[x];

		AACENC_ERROR err = aacEncoder_SetParam(self, param->param, param->value);
		if (err != AACENC_OK)
		{
			log(WLOG_ERROR, "aacEncoder_SetParam(%s, %d) failed with %s",
			    aac_enc_param_str(param->param), param->value, enc_err_str(err));
			return -1;
		}
	}

	AACENC_ERROR err = aacEncEncode(self, NULL, NULL, NULL, NULL);
	if (err != AACENC_OK)
	{
		log(WLOG_ERROR, "aacEncEncode failed with %s", enc_err_str(err));
		return -1;
	}

	AACENC_InfoStruct info = { 0 };
	err = aacEncInfo(self, &info);
	if (err != AACENC_OK)
	{
		log(WLOG_ERROR, "aacEncInfo failed with %s", enc_err_str(err));
		return -1;
	}

	if (encoder)
	{
		*pbuffersize = info.maxOutBufBytes;
		log_enc_info(&info, log);
		return 0;
	}
	else
	{
		err = aacEncClose(&self);
		if (err != AACENC_OK)
			log(WLOG_WARN, "aacEncClose failed with %s", enc_err_str(err));

		*pbuffersize = info.frameLength * info.inputChannels * sizeof(INT_PCM);

		HANDLE_AACDECODER aacdec = (HANDLE_AACDECODER)handle;

		UCHAR* asc[] = { info.confBuf };
		UINT ascSize[] = { info.confSize };

		assert(handle);

		AAC_DECODER_ERROR decerr = aacDecoder_ConfigRaw(aacdec, asc, ascSize);
		if (decerr != AAC_DEC_OK)
		{
			log(WLOG_ERROR, "aacDecoder_ConfigRaw failed with %s", dec_err_str(decerr));
			return -1;
		}
		return 0;
	}
}

ssize_t fdk_aac_dsp_impl_decode_fill(void* handle, const void* data, size_t size, fdk_log_fkt_t log)
{
	assert(handle);
	assert(log);

	UINT leftBytes = size;
	HANDLE_AACDECODER self = (HANDLE_AACDECODER)handle;

	union
	{
		const void* cpv;
		UCHAR* puc;
	} cnv;
	cnv.cpv = data;
	UCHAR* pBuffer[] = { cnv.puc };
	const UINT bufferSize[] = { size };

	assert(handle);
	assert(data || (size == 0));

	AAC_DECODER_ERROR err = aacDecoder_Fill(self, pBuffer, bufferSize, &leftBytes);
	if (err != AAC_DEC_OK)
	{
		log(WLOG_ERROR, "aacDecoder_Fill failed with %s", dec_err_str(err));
		return -1;
	}
	return leftBytes;
}

ssize_t fdk_aac_dsp_impl_stream_info(void* handle, int encoder, fdk_log_fkt_t log)
{
	assert(handle);
	assert(log);

	if (encoder)
	{
		AACENC_InfoStruct info = { 0 };
		HANDLE_AACENCODER self = (HANDLE_AACENCODER)handle;
		AACENC_ERROR err = aacEncInfo(self, &info);
		if (err != AACENC_OK)
		{
			log(WLOG_ERROR, "aacEncInfo failed with %s", enc_err_str(err));
			return -1;
		}
		return info.maxOutBufBytes;
	}
	else
	{
		HANDLE_AACDECODER self = (HANDLE_AACDECODER)handle;
		CStreamInfo* info = aacDecoder_GetStreamInfo(self);
		if (!info)
		{
			log(WLOG_ERROR, "aacDecoder_GetStreamInfo failed");
			return -1;
		}

		return sizeof(INT_PCM) * info->numChannels * info->frameSize;
	}
}

ssize_t fdk_aac_dsp_impl_encode(void* handle, const void* data, size_t size, void* dst,
                                size_t dstSize, fdk_log_fkt_t log)
{
	INT inSizes[] = { size };
	INT inElSizes[] = { sizeof(INT_PCM) };
	INT inIdentifiers[] = { IN_AUDIO_DATA };
	union
	{
		const void* cpv;
		void* pv;
	} cnv;
	cnv.cpv = data;
	void* inBuffers[] = { cnv.pv };

	const AACENC_BufDesc inBufDesc = {
		.numBufs = 1,
		.bufs = inBuffers,
		.bufferIdentifiers = inIdentifiers,
		.bufSizes = inSizes,
		.bufElSizes = inElSizes /* TODO: 8/16 bit input? */
	};

	INT outSizes[] = { dstSize };
	INT outElSizes[] = { 1 };
	INT outIdentifiers[] = { OUT_BITSTREAM_DATA };
	void* outBuffers[] = { dst };
	const AACENC_BufDesc outBufDesc = { .numBufs = 1,
		                                .bufs = outBuffers,
		                                .bufferIdentifiers = outIdentifiers,
		                                .bufSizes = outSizes,
		                                .bufElSizes = outElSizes };

	const AACENC_InArgs inArgs = { .numInSamples =
		                               size / sizeof(INT_PCM), /* TODO: 8/16 bit input? */
		                           .numAncBytes = 0 };
	AACENC_OutArgs outArgs = { 0 };

	HANDLE_AACENCODER self = (HANDLE_AACENCODER)handle;

	assert(handle);
	assert(log);

	AACENC_ERROR err = aacEncEncode(self, &inBufDesc, &outBufDesc, &inArgs, &outArgs);
	if (err != AACENC_OK)
	{
		log(WLOG_ERROR, "aacEncEncode failed with %s", enc_err_str(err));
		return -1;
	}
	return outArgs.numOutBytes;
}
