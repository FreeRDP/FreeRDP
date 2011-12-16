/**
 * FreeRDP: A Remote Desktop Protocol Client
 * T.124 Generic Conference Control (GCC)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __GCC_H
#define __GCC_H

#include "per.h"
#include "mcs.h"

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>

/* Client to Server (CS) data blocks */
#define CS_CORE		0xC001
#define CS_SECURITY	0xC002
#define CS_NET		0xC003
#define CS_CLUSTER	0xC004
#define CS_MONITOR	0xC005

/* Server to Client (SC) data blocks */
#define SC_CORE		0x0C01
#define SC_SECURITY	0x0C02
#define SC_NET		0x0C03

/* RDP version */
#define RDP_VERSION_4		0x00080001
#define RDP_VERSION_5_PLUS	0x00080004

/* Color depth */
#define RNS_UD_COLOR_4BPP	0xCA00
#define RNS_UD_COLOR_8BPP	0xCA01
#define RNS_UD_COLOR_16BPP_555	0xCA02
#define RNS_UD_COLOR_16BPP_565	0xCA03
#define RNS_UD_COLOR_24BPP	0xCA04

/* Secure Access Sequence */
#define RNS_UD_SAS_DEL		0xAA03

/* Supported Color Depths */
#define RNS_UD_24BPP_SUPPORT	0x0001
#define RNS_UD_16BPP_SUPPORT	0x0002
#define RNS_UD_15BPP_SUPPORT	0x0004
#define RNS_UD_32BPP_SUPPORT	0x0008

/* Early Capability Flags */
#define RNS_UD_CS_SUPPORT_ERRINFO_PDU		0x0001
#define RNS_UD_CS_WANT_32BPP_SESSION		0x0002
#define RNS_UD_CS_SUPPORT_STATUSINFO_PDU	0x0004
#define RNS_UD_CS_STRONG_ASYMMETRIC_KEYS	0x0008
#define RNS_UD_CS_VALID_CONNECTION_TYPE		0x0020
#define RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU	0x0040

/* Cluster Information Flags */
#define REDIRECTION_SUPPORTED			0x00000001
#define REDIRECTED_SESSIONID_FIELD_VALID	0x00000002
#define REDIRECTED_SMARTCARD			0x00000040

#define REDIRECTION_VERSION1			0x00
#define REDIRECTION_VERSION2			0x01
#define REDIRECTION_VERSION3			0x02
#define REDIRECTION_VERSION4			0x03
#define REDIRECTION_VERSION5			0x04

/* Monitor Flags */
#define MONITOR_PRIMARY				0x00000001

boolean gcc_read_conference_create_request(STREAM* s, rdpSettings* settings);
void gcc_write_conference_create_request(STREAM* s, STREAM* user_data);
boolean gcc_read_conference_create_response(STREAM* s, rdpSettings* settings);
void gcc_write_conference_create_response(STREAM* s, STREAM* user_data);
boolean gcc_read_client_data_blocks(STREAM* s, rdpSettings *settings, int length);
void gcc_write_client_data_blocks(STREAM* s, rdpSettings *settings);
boolean gcc_read_server_data_blocks(STREAM* s, rdpSettings *settings, int length);
void gcc_write_server_data_blocks(STREAM* s, rdpSettings *settings);
boolean gcc_read_user_data_header(STREAM* s, uint16* type, uint16* length);
void gcc_write_user_data_header(STREAM* s, uint16 type, uint16 length);
boolean gcc_read_client_core_data(STREAM* s, rdpSettings *settings, uint16 blockLength);
void gcc_write_client_core_data(STREAM* s, rdpSettings *settings);
boolean gcc_read_server_core_data(STREAM* s, rdpSettings *settings);
void gcc_write_server_core_data(STREAM* s, rdpSettings *settings);
boolean gcc_read_client_security_data(STREAM* s, rdpSettings *settings, uint16 blockLength);
void gcc_write_client_security_data(STREAM* s, rdpSettings *settings);
boolean gcc_read_server_security_data(STREAM* s, rdpSettings *settings);
void gcc_write_server_security_data(STREAM* s, rdpSettings *settings);
boolean gcc_read_client_network_data(STREAM* s, rdpSettings *settings, uint16 blockLength);
void gcc_write_client_network_data(STREAM* s, rdpSettings *settings);
boolean gcc_read_server_network_data(STREAM* s, rdpSettings *settings);
void gcc_write_server_network_data(STREAM* s, rdpSettings *settings);
boolean gcc_read_client_cluster_data(STREAM* s, rdpSettings *settings, uint16 blockLength);
void gcc_write_client_cluster_data(STREAM* s, rdpSettings *settings);
boolean gcc_read_client_monitor_data(STREAM* s, rdpSettings *settings, uint16 blockLength);
void gcc_write_client_monitor_data(STREAM* s, rdpSettings *settings);

#endif /* __GCC_H */
