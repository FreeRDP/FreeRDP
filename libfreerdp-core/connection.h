/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Connection Sequence
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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "tpkt.h"
#include "tpdu.h"
#include "nego.h"
#include "mcs.h"
#include "transport.h"

#include <freerdp/settings.h>
#include <freerdp/utils/memory.h>

/* Client Address Family */
#define ADDRESS_FAMILY_INET		0x0002
#define ADDRESS_FAMILY_INET6		0x0017

/* Client Info Packet Flags */
#define INFO_MOUSE			0x00000001
#define INFO_DISABLECTRLALTDEL		0x00000002
#define INFO_AUTOLOGON			0x00000008
#define INFO_UNICODE			0x00000010
#define INFO_MAXIMIZESHELL		0x00000020
#define INFO_LOGONNOTIFY		0x00000040
#define INFO_COMPRESSION		0x00000080
#define INFO_ENABLEWINDOWSKEY		0x00000100
#define INFO_REMOTECONSOLEAUDIO		0x00002000
#define INFO_FORCE_ENCRYPTED_CS_PDU	0x00004000
#define INFO_RAIL			0x00008000
#define INFO_LOGONERRORS		0x00010000
#define INFO_MOUSE_HAS_WHEEL		0x00020000
#define INFO_PASSWORD_IS_SC_PIN		0x00040000
#define INFO_NOAUDIOPLAYBACK		0x00080000
#define INFO_USING_SAVED_CREDS		0x00100000
#define RNS_INFO_AUDIOCAPTURE		0x00200000
#define RNS_INFO_VIDEO_DISABLE		0x00400000
#define CompressionTypeMask		0x00001E00
#define PACKET_COMPR_TYPE_8K		0x00000100
#define PACKET_COMPR_TYPE_64K		0x00000200
#define PACKET_COMPR_TYPE_RDP6		0x00000300
#define PACKET_COMPR_TYPE_RDP61		0x00000400

/* Security Header Flags */
#define SEC_EXCHANGE_PKT		0x0001
#define SEC_ENCRYPT			0x0008
#define SEC_RESET_SEQNO			0x0010
#define	SEC_IGNORE_SEQNO		0x0020
#define	SEC_INFO_PKT			0x0040
#define	SEC_LICENSE_PKT			0x0080
#define SEC_LICENSE_ENCRYPT_CS		0x0200
#define SEC_LICENSE_ENCRYPT_SC		0x0200
#define SEC_REDIRECTION_PKT		0x0400
#define SEC_SECURE_CHECKSUM		0x0800
#define SEC_FLAGSHI_VALID		0x8000

struct rdp_connection
{
	struct rdp_mcs* mcs;
	struct rdp_nego* nego;
	struct rdp_settings* settings;
	struct rdp_transport* transport;
};
typedef struct rdp_connection rdpConnection;

void connection_client_connect(rdpConnection* connection);
void connection_send_client_info(rdpConnection* connection);

void connection_write_system_time(STREAM* s, SYSTEM_TIME* system_time);
void connection_get_client_time_zone(STREAM* s, rdpSettings* settings);
void connection_write_client_time_zone(STREAM* s, rdpSettings* settings);

void connection_write_info_packet(STREAM* s, rdpSettings* settings);
void connection_write_extended_info_packet(STREAM* s, rdpSettings* settings);
void connection_write_auto_reconnect_cookie(STREAM* s, rdpSettings* settings);

rdpConnection* connection_new(rdpSettings* settings);
void connection_free(rdpConnection* connection);

#endif /* __CONNECTION_H */
