/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Constants
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Hewlett-Packard Development Company, L.P.
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

#ifndef __TSMF_CONSTANTS_H
#define __TSMF_CONSTANTS_H

#define GUID_SIZE 16
#define TSMF_BUFFER_PADDING_SIZE 8

/* Interface IDs defined in [MS-RDPEV]. There's no constant names in the MS
   documentation, so we create them on our own. */
#define TSMF_INTERFACE_DEFAULT                  0x00000000
#define TSMF_INTERFACE_CLIENT_NOTIFICATIONS     0x00000001
#define TSMF_INTERFACE_CAPABILITIES             0x00000002

/* Interface ID Mask */
#define STREAM_ID_STUB      0x80000000
#define STREAM_ID_PROXY     0x40000000
#define STREAM_ID_NONE      0x00000000

/* Functon ID */
/* Common IDs for all interfaces are as follows. */
#define RIMCALL_RELEASE                     0x00000001
#define RIMCALL_QUERYINTERFACE              0x00000002
/* Capabilities Negotiator Interface IDs are as follows. */
#define RIM_EXCHANGE_CAPABILITY_REQUEST     0x00000100
/* The Client Notifications Interface ID is as follows. */
#define PLAYBACK_ACK                        0x00000100
#define CLIENT_EVENT_NOTIFICATION           0x00000101
/* Server Data Interface IDs are as follows. */
#define EXCHANGE_CAPABILITIES_REQ           0x00000100
#define SET_CHANNEL_PARAMS                  0x00000101
#define ADD_STREAM                          0x00000102
#define ON_SAMPLE                           0x00000103
#define SET_VIDEO_WINDOW                    0x00000104
#define ON_NEW_PRESENTATION                 0x00000105
#define SHUTDOWN_PRESENTATION_REQ           0x00000106
#define SET_TOPOLOGY_REQ                    0x00000107
#define CHECK_FORMAT_SUPPORT_REQ            0x00000108
#define ON_PLAYBACK_STARTED                 0x00000109
#define ON_PLAYBACK_PAUSED                  0x0000010a
#define ON_PLAYBACK_STOPPED                 0x0000010b
#define ON_PLAYBACK_RESTARTED               0x0000010c
#define ON_PLAYBACK_RATE_CHANGED            0x0000010d
#define ON_FLUSH                            0x0000010e
#define ON_STREAM_VOLUME                    0x0000010f
#define ON_CHANNEL_VOLUME                   0x00000110
#define ON_END_OF_STREAM                    0x00000111
#define SET_ALLOCATOR                       0x00000112
#define NOTIFY_PREROLL                      0x00000113
#define UPDATE_GEOMETRY_INFO                0x00000114
#define REMOVE_STREAM                       0x00000115
#define SET_SOURCE_VIDEO_RECT               0x00000116

/* Supported platform */
#define MMREDIR_CAPABILITY_PLATFORM_MF      0x00000001
#define MMREDIR_CAPABILITY_PLATFORM_DSHOW   0x00000002
#define MMREDIR_CAPABILITY_PLATFORM_OTHER   0x00000004

/* TSMM_CLIENT_EVENT Constants */
#define TSMM_CLIENT_EVENT_ENDOFSTREAM       0x0064
#define TSMM_CLIENT_EVENT_STOP_COMPLETED    0x00C8
#define TSMM_CLIENT_EVENT_START_COMPLETED   0x00C9
#define TSMM_CLIENT_EVENT_MONITORCHANGED    0x012C

/* TS_MM_DATA_SAMPLE.SampleExtensions */
#define TSMM_SAMPLE_EXT_CLEANPOINT          0x00000001
#define TSMM_SAMPLE_EXT_DISCONTINUITY       0x00000002
#define TSMM_SAMPLE_EXT_INTERLACED          0x00000004
#define TSMM_SAMPLE_EXT_BOTTOMFIELDFIRST    0x00000008
#define TSMM_SAMPLE_EXT_REPEATFIELDFIRST    0x00000010
#define TSMM_SAMPLE_EXT_SINGLEFIELD         0x00000020
#define TSMM_SAMPLE_EXT_DERIVEDFROMTOPFIELD 0x00000040
#define TSMM_SAMPLE_EXT_HAS_NO_TIMESTAMPS   0x00000080
#define TSMM_SAMPLE_EXT_RELATIVE_TIMESTAMPS 0x00000100
#define TSMM_SAMPLE_EXT_ABSOLUTE_TIMESTAMPS 0x00000200

/* MajorType */
#define TSMF_MAJOR_TYPE_UNKNOWN             0
#define TSMF_MAJOR_TYPE_VIDEO               1
#define TSMF_MAJOR_TYPE_AUDIO               2

/* SubType */
#define TSMF_SUB_TYPE_UNKNOWN               0
#define TSMF_SUB_TYPE_WVC1                  1
#define TSMF_SUB_TYPE_WMA2                  2
#define TSMF_SUB_TYPE_WMA9                  3
#define TSMF_SUB_TYPE_MP3                   4
#define TSMF_SUB_TYPE_MP2A                  5
#define TSMF_SUB_TYPE_MP2V                  6
#define TSMF_SUB_TYPE_WMV3                  7
#define TSMF_SUB_TYPE_AAC                   8
#define TSMF_SUB_TYPE_H264                  9
#define TSMF_SUB_TYPE_AVC1                 10
#define TSMF_SUB_TYPE_AC3                  11
#define TSMF_SUB_TYPE_WMV2                 12
#define TSMF_SUB_TYPE_WMV1                 13
#define TSMF_SUB_TYPE_MP1V                 14
#define TSMF_SUB_TYPE_MP1A                 15
#define TSMF_SUB_TYPE_YUY2                 16
#define TSMF_SUB_TYPE_MP43                 17
#define TSMF_SUB_TYPE_MP4S                 18
#define TSMF_SUB_TYPE_MP42                 19
#define TSMF_SUB_TYPE_OGG                  20
#define TSMF_SUB_TYPE_SPEEX                21
#define TSMF_SUB_TYPE_THEORA               22
#define TSMF_SUB_TYPE_FLAC                 23
#define TSMF_SUB_TYPE_VP8                  24
#define TSMF_SUB_TYPE_VP9                  25
#define TSMF_SUB_TYPE_H263                 26
#define TSMF_SUB_TYPE_M4S2                 27
#define TSMF_SUB_TYPE_WMA1                 28

/* FormatType */
#define TSMF_FORMAT_TYPE_UNKNOWN            0
#define TSMF_FORMAT_TYPE_MFVIDEOFORMAT      1
#define TSMF_FORMAT_TYPE_WAVEFORMATEX       2
#define TSMF_FORMAT_TYPE_MPEG2VIDEOINFO     3
#define TSMF_FORMAT_TYPE_VIDEOINFO2         4
#define TSMF_FORMAT_TYPE_MPEG1VIDEOINFO     5

#endif

