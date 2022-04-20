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
#include <inttypes.h>

#include <fdk-aac/aacdecoder_lib.h>
#include <fdk-aac/aacenc_lib.h>

#include "dsp_fdk_impl.h"

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
		case AACENC_INIT_MPS_ERROR:
			return "AACENC_INIT_MPS_ERROR";
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

static void log_enc_info(const AACENC_InfoStruct* info, void (*log)(const char* fmt, ...))
{
	size_t x;
	char confBuf[1024] = { 0 };

	assert(info);
	assert(log);

	strcat(confBuf, "{");
	for (x = 0; x < 64; x++)
	{
		char tmp[12] = { 0 };
		sprintf(tmp, "0x%02x", (int)info->confBuf[x]);
		if (x > 0)
			strcat(confBuf, ", ");
		strcat(confBuf, tmp);
	}
	strcat(confBuf, "}");

	log("[encoder info] "
	    "maxOutBufBytes : %u, "
	    "maxAncBytes    : %u, "
	    "inBufFillLevel : %u, "
	    "inputChannels  : %u, "
	    "frameLength    : %u, "
	    "nDelay         : %u, "
	    "nDelayCore     : %u, "
	    "confBuf[64]    : %s, "
	    "confSize       : %u",
	    info->maxOutBufBytes, info->maxAncBytes, info->inBufFillLevel, info->inputChannels,
	    info->frameLength, info->nDelay, info->nDelayCore, confBuf, info->confSize);
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

int fdk_aac_dsp_impl_init(void** handle, int encoder, void (*log)(const char* fmt, ...))
{
	assert(handle);
	assert(log);

	if (encoder)
	{
		HANDLE_AACENCODER* h = handle;
		AACENC_ERROR err = aacEncOpen(h, 0, 0);
		if (err != AACENC_OK)
		{
			log("aacEncOpen failed with %s", enc_err_str(err));
			return 0;
		}
	}
	else
	{
		HANDLE_AACDECODER* h = handle;
		assert(NULL == *h);

		*h = aacDecoder_Open(TT_MP4_RAW, 1);
		if (!*h)
		{
			log("aacDecoder_Open failed");
			return 0;
		}
	}
	return 1;
}

void fdk_aac_dsp_impl_uninit(void** handle, int encoder, void (*log)(const char* fmt, ...))
{
	assert(handle);
	assert(log);

	if (encoder)
	{
		HANDLE_AACENCODER* h = handle;
		AACENC_ERROR err = aacEncClose(h);
		if (err != AACENC_OK)
			log("aacEncClose failed with %s", enc_err_str(err));
	}
	else
	{
		HANDLE_AACDECODER* h = handle;
		if (h)
			aacDecoder_Close(*h);
	}

	*handle = NULL;
}

int fdk_aac_dsp_impl_decode_read(void* handle, void* dst, size_t dstSize,
                                 void (*log)(const char* fmt, ...))
{
	assert(handle);

	UINT flags = AACDEC_FLUSH | AACDEC_CONCEAL;
	HANDLE_AACDECODER self = (HANDLE_AACDECODER)handle;
	AAC_DECODER_ERROR err = aacDecoder_DecodeFrame(self, dst, dstSize, flags);
	if (err != AAC_DEC_OK)
	{
		log("aacDecoder_DecodeFrame failed with %s", dec_err_str(err));
		return 0;
		}

	return 1;
}

int fdk_aac_dsp_impl_config(void* handle, int encoder, const unsigned char* data, size_t size,
                            void (*log)(const char* fmt, ...))
{
	assert(handle);
	assert(log);

	if (encoder)
	{
		size_t x;
		struct t_param_pair
		{
			AACENC_PARAM param;
			UINT value;
		};

		const struct t_param_pair params[] = { { AACENC_AOT, 0 },
			                                   { AACENC_BITRATE, 0 },
			                                   { AACENC_BITRATEMODE, 0 },
			                                   { AACENC_SAMPLERATE, 0 },
			                                   { AACENC_SBR_MODE, 0 },
			                                   { AACENC_GRANULE_LENGTH, 0 },
			                                   { AACENC_CHANNELMODE, 0 },
			                                   { AACENC_CHANNELORDER, 0 },
			                                   { AACENC_SBR_RATIO, 0 },
			                                   { AACENC_AFTERBURNER, 0 },
			                                   { AACENC_BANDWIDTH, 0 },
			                                   { AACENC_PEAK_BITRATE, 0 },
			                                   { AACENC_TRANSMUX, 0 },
			                                   { AACENC_HEADER_PERIOD, 0 },
			                                   { AACENC_SIGNALING_MODE, 0 },
			                                   { AACENC_TPSUBFRAMES, 0 },
			                                   { AACENC_AUDIOMUXVER, 0 },
			                                   { AACENC_PROTECTION, 0 },
			                                   { AACENC_ANCILLARY_BITRATE, 0 },
			                                   { AACENC_METADATA_MODE, 0 },
			                                   { AACENC_CONTROL_STATE, 0 } };
		HANDLE_AACENCODER self = (HANDLE_AACENCODER)handle;

		for (x = 0; x < sizeof(params) / sizeof(params[0]); x++)
		{
			const struct t_param_pair* param = &params[x];

			AACENC_ERROR err = aacEncoder_SetParam(self, param->param, param->value);
			if (err != AACENC_OK)
			{
				log("aacEncoder_SetParam [%s] failed with %s", aac_enc_param_str(param->param),
				    enc_err_str(err));
				return -1;
			}
		}

		AACENC_InfoStruct info = { 0 };
		AACENC_ERROR err = aacEncInfo(self, &info);
		if (err != AACENC_OK)
		{
			log("aacEncInfo failed with %s", enc_err_str(err));
			return -1;
		}
		log_enc_info(&info, log);
		return 0;
	}
	else
	{
		AAC_DECODER_ERROR err;
		HANDLE_AACDECODER self = (HANDLE_AACDECODER)handle;

		UCHAR* conf[] = { data };

		UINT conf_len = size;

		assert(handle);
		assert(data || (size == 0));

		err = aacDecoder_ConfigRaw(self, conf, &conf_len);
		if (err != AAC_DEC_OK)
		{
			log("aacDecoder_AncDataInit failed with %s", dec_err_str(err));
			return -1;
		}
		CStreamInfo* info = aacDecoder_GetStreamInfo(self);
		if (!info)
		{
			log("aacDecoder_GetStreamInfo failed");
			return -1;
		}
		log_dec_info(info, log);
		return 0;
	}
}

ssize_t fdk_aac_dsp_impl_decode_fill(void* handle, const void* data, size_t size,
                                     void (*log)(const char* fmt, ...))
{
	assert(handle);
	assert(log);

		UINT valid = 0;
		AAC_DECODER_ERROR err;
		HANDLE_AACDECODER self = (HANDLE_AACDECODER)handle;
		UCHAR* pBuffer[] = { data };
		const UINT bufferSize[] = { size };

		assert(handle);
		assert(data || (size == 0));

		err = aacDecoder_Fill(self, pBuffer, bufferSize, &valid);
		if (err != AAC_DEC_OK)
		{
			log("aacDecoder_Fill failed with %s", dec_err_str(err));
			return -1;
		}
	    return size - valid;
}

ssize_t fdk_aac_dsp_impl_stream_info(void* handle, int encoder, void (*log)(const char* fmt, ...))
{
	assert(handle);
	assert(log);

	if (encoder)
	{
		AACENC_InfoStruct info = { 0 };
		HANDLE_AACENCODER self = (HANDLE_AACENCODER)handle;
		AACENC_ERROR err = aacEncInfo(self, &info);
		if (err != AAC_DEC_OK)
		{
			log("aacEncInfo failed with %s", enc_err_str(err));
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
			log("aacDecoder_GetStreamInfo failed");
			return -1;
		}
		log_dec_info(info, log);
		return info->frameSize;
	}
}

ssize_t fdk_aac_dsp_impl_encode(void* handle, const void* data, size_t size, void* dst,
                                size_t dstSize, void (*log)(const char* fmt, ...))
{
	const AACENC_BufDesc inBufDesc = {
		.numBufs = 1,
		.bufs = { data },
		.bufferIdentifiers = { IN_AUDIO_DATA },
		.bufSizes = { size },
		.bufElSizes = { 2 } /* TODO: 8/16 bit input? */
	};
	const AACENC_BufDesc outBufDesc = { .numBufs = 1,
		                                .bufs = { dst },
		                                .bufferIdentifiers = { OUT_BITSTREAM_DATA },
		                                .bufSizes = { dstSize },
		                                .bufElSizes = { 1 } };
	const AACENC_InArgs inArgs = { .numInSamples = size / 2, /* TODO: 8/16 bit input? */
		                           .numAncBytes = 0 };
	AACENC_OutArgs outArgs = { 0 };

	HANDLE_AACENCODER self = (HANDLE_AACENCODER)handle;

	assert(handle);
	assert(log);

	AACENC_ERROR err = aacEncEncode(self, &inBufDesc, &outBufDesc, &inArgs, &outArgs);
	if (err != AACENC_OK)
	{
		log("aacEncEncode failed with %s", enc_err_str(err));
		return -1;
	}
	return outArgs.numOutBytes;
}
