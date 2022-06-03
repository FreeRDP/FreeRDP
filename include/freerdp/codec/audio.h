/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Formats
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CODEC_AUDIO_H
#define FREERDP_CODEC_AUDIO_H

#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef _WIN32
#include <mmreg.h>
#endif

struct AUDIO_FORMAT
{
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT32 nAvgBytesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
	UINT16 cbSize;
	BYTE* data;
};
typedef struct AUDIO_FORMAT AUDIO_FORMAT;

#define SNDC_CLOSE 1
#define SNDC_WAVE 2
#define SNDC_SETVOLUME 3
#define SNDC_SETPITCH 4
#define SNDC_WAVECONFIRM 5
#define SNDC_TRAINING 6
#define SNDC_FORMATS 7
#define SNDC_CRYPTKEY 8
#define SNDC_WAVEENCRYPT 9
#define SNDC_UDPWAVE 10
#define SNDC_UDPWAVELAST 11
#define SNDC_QUALITYMODE 12
#define SNDC_WAVE2 13

#define TSSNDCAPS_ALIVE 1
#define TSSNDCAPS_VOLUME 2
#define TSSNDCAPS_PITCH 4

#define DYNAMIC_QUALITY 0x0000
#define MEDIUM_QUALITY 0x0001
#define HIGH_QUALITY 0x0002

/*
 * Format Tags:
 * http://tools.ietf.org/html/rfc2361
 */

#ifndef __MINGW32__
#define WAVE_FORMAT_UNKNOWN 0x0000

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 0x0001
#endif

#define WAVE_FORMAT_ADPCM 0x0002
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_VSELP 0x0004
#define WAVE_FORMAT_IBM_CVSD 0x0005
#define WAVE_FORMAT_ALAW 0x0006
#define WAVE_FORMAT_MULAW 0x0007
#define WAVE_FORMAT_OKI_ADPCM 0x0010
#define WAVE_FORMAT_DVI_ADPCM 0x0011
#define WAVE_FORMAT_MEDIASPACE_ADPCM 0x0012
#define WAVE_FORMAT_SIERRA_ADPCM 0x0013
#define WAVE_FORMAT_G723_ADPCM 0x0014
#define WAVE_FORMAT_DIGISTD 0x0015
#define WAVE_FORMAT_DIGIFIX 0x0016
#define WAVE_FORMAT_DIALOGIC_OKI_ADPCM 0x0017
#define WAVE_FORMAT_MEDIAVISION_ADPCM 0x0018
#define WAVE_FORMAT_CU_CODEC 0x0019
#define WAVE_FORMAT_YAMAHA_ADPCM 0x0020
#define WAVE_FORMAT_SONARC 0x0021
#define WAVE_FORMAT_DSPGROUP_TRUESPEECH 0x0022
#define WAVE_FORMAT_ECHOSC1 0x0023
#define WAVE_FORMAT_AUDIOFILE_AF36 0x0024
#define WAVE_FORMAT_APTX 0x0025
#define WAVE_FORMAT_AUDIOFILE_AF10 0x0026
#define WAVE_FORMAT_PROSODY_1612 0x0027
#define WAVE_FORMAT_LRC 0x0028
#define WAVE_FORMAT_DOLBY_AC2 0x0030
#define WAVE_FORMAT_GSM610 0x0031
#define WAVE_FORMAT_MSNAUDIO 0x0032
#define WAVE_FORMAT_ANTEX_ADPCME 0x0033
#define WAVE_FORMAT_CONTROL_RES_VQLPC 0x0034
#define WAVE_FORMAT_DIGIREAL 0x0035
#define WAVE_FORMAT_DIGIADPCM 0x0036
#define WAVE_FORMAT_CONTROL_RES_CR10 0x0037
#define WAVE_FORMAT_NMS_VBXADPCM 0x0038
#define WAVE_FORMAT_ROLAND_RDAC 0x0039
#define WAVE_FORMAT_ECHOSC3 0x003A
#define WAVE_FORMAT_ROCKWELL_ADPCM 0x003B
#define WAVE_FORMAT_ROCKWELL_DIGITALK 0x003C
#define WAVE_FORMAT_XEBEC 0x003D
#define WAVE_FORMAT_G721_ADPCM 0x0040
#define WAVE_FORMAT_G728_CELP 0x0041
#define WAVE_FORMAT_MSG723 0x0042
#define WAVE_FORMAT_MPEG 0x0050
#define WAVE_FORMAT_RT24 0x0052
#define WAVE_FORMAT_PAC 0x0053

#ifndef WAVE_FORMAT_MPEGLAYER3
#define WAVE_FORMAT_MPEGLAYER3 0x0055
#endif

#define WAVE_FORMAT_LUCENT_G723 0x0059
#define WAVE_FORMAT_CIRRUS 0x0060
#define WAVE_FORMAT_ESPCM 0x0061
#define WAVE_FORMAT_VOXWARE 0x0062
#define WAVE_FORMAT_CANOPUS_ATRAC 0x0063
#define WAVE_FORMAT_G726_ADPCM 0x0064
#define WAVE_FORMAT_G722_ADPCM 0x0065
#define WAVE_FORMAT_DSAT 0x0066
#define WAVE_FORMAT_DSAT_DISPLAY 0x0067
#define WAVE_FORMAT_VOXWARE_BYTE_ALIGNED 0x0069
#define WAVE_FORMAT_VOXWARE_AC8 0x0070
#define WAVE_FORMAT_VOXWARE_AC10 0x0071
#define WAVE_FORMAT_VOXWARE_AC16 0x0072
#define WAVE_FORMAT_VOXWARE_AC20 0x0073
#define WAVE_FORMAT_VOXWARE_RT24 0x0074
#define WAVE_FORMAT_VOXWARE_RT29 0x0075
#define WAVE_FORMAT_VOXWARE_RT29HW 0x0076
#define WAVE_FORMAT_VOXWARE_VR12 0x0077
#define WAVE_FORMAT_VOXWARE_VR18 0x0078
#define WAVE_FORMAT_VOXWARE_TQ40 0x0079
#define WAVE_FORMAT_SOFTSOUND 0x0080
#define WAVE_FORMAT_VOXWARE_TQ60 0x0081
#define WAVE_FORMAT_MSRT24 0x0082
#define WAVE_FORMAT_G729A 0x0083
#define WAVE_FORMAT_MVI_MV12 0x0084
#define WAVE_FORMAT_DF_G726 0x0085
#define WAVE_FORMAT_DF_GSM610 0x0086
#define WAVE_FORMAT_ISIAUDIO 0x0088
#define WAVE_FORMAT_ONLIVE 0x0089
#define WAVE_FORMAT_SBC24 0x0091
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#define WAVE_FORMAT_ZYXEL_ADPCM 0x0097
#define WAVE_FORMAT_PHILIPS_LPCBB 0x0098
#define WAVE_FORMAT_PACKED 0x0099
#define WAVE_FORMAT_RHETOREX_ADPCM 0x0100
#define WAVE_FORMAT_IRAT 0x0101
#define WAVE_FORMAT_VIVO_G723 0x0111
#define WAVE_FORMAT_VIVO_SIREN 0x0112
#define WAVE_FORMAT_DIGITAL_G723 0x0123
#define WAVE_FORMAT_WMAUDIO2 0x0161
#define WAVE_FORMAT_WMAUDIO3 0x0162
#define WAVE_FORMAT_WMAUDIO_LOSSLESS 0x0163
#define WAVE_FORMAT_CREATIVE_ADPCM 0x0200
#define WAVE_FORMAT_CREATIVE_FASTSPEECH8 0x0202
#define WAVE_FORMAT_CREATIVE_FASTSPEECH10 0x0203
#define WAVE_FORMAT_QUARTERDECK 0x0220
#define WAVE_FORMAT_FM_TOWNS_SND 0x0300
#define WAVE_FORMAT_BTV_DIGITAL 0x0400
#define WAVE_FORMAT_VME_VMPCM 0x0680
#define WAVE_FORMAT_OLIGSM 0x1000
#define WAVE_FORMAT_OLIADPCM 0x1001
#define WAVE_FORMAT_OLICELP 0x1002
#define WAVE_FORMAT_OLISBC 0x1003
#define WAVE_FORMAT_OLIOPR 0x1004
#define WAVE_FORMAT_LH_CODEC 0x1100
#define WAVE_FORMAT_NORRIS 0x1400
#define WAVE_FORMAT_SOUNDSPACE_MUSICOMPRESS 0x1500
#define WAVE_FORMAT_DVM 0x2000
#endif /* !__MINGW32__ */
#define WAVE_FORMAT_AAC_MS 0xA106

/**
 * Audio Format Functions
 */

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API UINT32 audio_format_compute_time_length(const AUDIO_FORMAT* format, size_t size);

	FREERDP_API char* audio_format_get_tag_string(UINT16 wFormatTag);

	FREERDP_API void audio_format_print(wLog* log, DWORD level, const AUDIO_FORMAT* format);
	FREERDP_API void audio_formats_print(wLog* log, DWORD level, const AUDIO_FORMAT* formats,
	                                     UINT16 count);

	FREERDP_API BOOL audio_format_read(wStream* s, AUDIO_FORMAT* format);
	FREERDP_API BOOL audio_format_write(wStream* s, const AUDIO_FORMAT* format);
	FREERDP_API BOOL audio_format_copy(const AUDIO_FORMAT* srcFormat, AUDIO_FORMAT* dstFormat);
	FREERDP_API BOOL audio_format_compatible(const AUDIO_FORMAT* with, const AUDIO_FORMAT* what);

	FREERDP_API AUDIO_FORMAT* audio_format_new(void);
	FREERDP_API AUDIO_FORMAT* audio_formats_new(size_t count);

	FREERDP_API void audio_format_free(AUDIO_FORMAT* format);
	FREERDP_API void audio_formats_free(AUDIO_FORMAT* formats, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_AUDIO_H */
