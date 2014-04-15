/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Core
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

#ifndef __RDP_H
#define __RDP_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mcs.h"
#include "tpkt.h"
#include "bulk.h"
#include "fastpath.h"
#include "tpdu.h"
#include "nego.h"
#include "input.h"
#include "update.h"
#include "license.h"
#include "errinfo.h"
#include "autodetect.h"
#include "heartbeat.h"
#include "multitransport.h"
#include "security.h"
#include "transport.h"
#include "connection.h"
#include "redirection.h"
#include "capabilities.h"
#include "channels.h"

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/utils/debug.h>

#include <winpr/stream.h>

/* Security Header Flags */
#define SEC_EXCHANGE_PKT					0x0001
#define SEC_TRANSPORT_REQ					0x0002
#define SEC_TRANSPORT_RSP					0x0004
#define SEC_ENCRYPT						0x0008
#define SEC_RESET_SEQNO						0x0010
#define SEC_IGNORE_SEQNO					0x0020
#define	SEC_INFO_PKT						0x0040
#define	SEC_LICENSE_PKT						0x0080
#define SEC_LICENSE_ENCRYPT_CS					0x0200
#define SEC_LICENSE_ENCRYPT_SC					0x0200
#define SEC_REDIRECTION_PKT					0x0400
#define SEC_SECURE_CHECKSUM					0x0800
#define SEC_AUTODETECT_REQ					0x1000
#define SEC_AUTODETECT_RSP					0x2000
#define SEC_HEARTBEAT						0x4000
#define SEC_FLAGSHI_VALID					0x8000

#define SEC_PKT_CS_MASK						(SEC_EXCHANGE_PKT | SEC_INFO_PKT)
#define SEC_PKT_SC_MASK						(SEC_LICENSE_PKT | SEC_REDIRECTION_PKT)
#define SEC_PKT_MASK						(SEC_PKT_CS_MASK | SEC_PKT_SC_MASK)

#define RDP_SECURITY_HEADER_LENGTH				4
#define RDP_SHARE_CONTROL_HEADER_LENGTH				6
#define RDP_SHARE_DATA_HEADER_LENGTH				12
#define RDP_PACKET_HEADER_MAX_LENGTH				(TPDU_DATA_LENGTH + MCS_SEND_DATA_HEADER_MAX_LENGTH)

#define PDU_TYPE_DEMAND_ACTIVE					0x1
#define PDU_TYPE_CONFIRM_ACTIVE					0x3
#define PDU_TYPE_DEACTIVATE_ALL					0x6
#define PDU_TYPE_DATA						0x7
#define PDU_TYPE_SERVER_REDIRECTION				0xA

#define FINALIZE_SC_SYNCHRONIZE_PDU				0x01
#define FINALIZE_SC_CONTROL_COOPERATE_PDU			0x02
#define FINALIZE_SC_CONTROL_GRANTED_PDU				0x04
#define FINALIZE_SC_FONT_MAP_PDU				0x08
#define FINALIZE_SC_COMPLETE					0x0F

/* Data PDU Types */
#define DATA_PDU_TYPE_UPDATE					0x02
#define DATA_PDU_TYPE_CONTROL					0x14
#define DATA_PDU_TYPE_POINTER					0x1B
#define DATA_PDU_TYPE_INPUT					0x1C
#define DATA_PDU_TYPE_SYNCHRONIZE				0x1F
#define DATA_PDU_TYPE_REFRESH_RECT				0x21
#define DATA_PDU_TYPE_PLAY_SOUND				0x22
#define DATA_PDU_TYPE_SUPPRESS_OUTPUT				0x23
#define DATA_PDU_TYPE_SHUTDOWN_REQUEST				0x24
#define DATA_PDU_TYPE_SHUTDOWN_DENIED				0x25
#define DATA_PDU_TYPE_SAVE_SESSION_INFO				0x26
#define DATA_PDU_TYPE_FONT_LIST					0x27
#define DATA_PDU_TYPE_FONT_MAP					0x28
#define DATA_PDU_TYPE_SET_KEYBOARD_INDICATORS			0x29
#define DATA_PDU_TYPE_BITMAP_CACHE_PERSISTENT_LIST		0x2B
#define DATA_PDU_TYPE_BITMAP_CACHE_ERROR			0x2C
#define DATA_PDU_TYPE_SET_KEYBOARD_IME_STATUS			0x2D
#define DATA_PDU_TYPE_OFFSCREEN_CACHE_ERROR			0x2E
#define DATA_PDU_TYPE_SET_ERROR_INFO				0x2F
#define DATA_PDU_TYPE_DRAW_NINEGRID_ERROR			0x30
#define DATA_PDU_TYPE_DRAW_GDIPLUS_ERROR			0x31
#define DATA_PDU_TYPE_ARC_STATUS				0x32
#define DATA_PDU_TYPE_STATUS_INFO				0x36
#define DATA_PDU_TYPE_MONITOR_LAYOUT				0x37
#define DATA_PDU_TYPE_FRAME_ACKNOWLEDGE				0x38

/* Stream Identifiers */
#define STREAM_UNDEFINED					0x00
#define STREAM_LOW						0x01
#define STREAM_MED						0x02
#define STREAM_HI						0x04

struct rdp_rdp
{
	int state;
	freerdp* instance;
	rdpContext* context;
	rdpMcs* mcs;
	rdpNego* nego;
	rdpBulk* bulk;
	rdpInput* input;
	rdpUpdate* update;
	rdpFastPath* fastpath;
	rdpLicense* license;
	rdpRedirection* redirection;
	rdpSettings* settings;
	rdpTransport* transport;
	rdpAutoDetect* autodetect;
	rdpHeartbeat* heartbeat;
	rdpMultitransport* multitransport;
	struct crypto_rc4_struct* rc4_decrypt_key;
	int decrypt_use_count;
	int decrypt_checksum_use_count;
	struct crypto_rc4_struct* rc4_encrypt_key;
	int encrypt_use_count;
	int encrypt_checksum_use_count;
	struct crypto_des3_struct* fips_encrypt;
	struct crypto_des3_struct* fips_decrypt;
	struct crypto_hmac_struct* fips_hmac;
	UINT32 sec_flags;
	BOOL do_crypt;
	BOOL do_secure_checksum;
	BYTE sign_key[16];
	BYTE decrypt_key[16];
	BYTE encrypt_key[16];
	BYTE decrypt_update_key[16];
	BYTE encrypt_update_key[16];
	int rc4_key_len;
	BYTE fips_sign_key[20];
	BYTE fips_encrypt_key[24];
	BYTE fips_decrypt_key[24];
	UINT32 errorInfo;
	UINT32 finalize_sc_pdus;
	BOOL disconnect;
	BOOL resendFocus;
	BOOL deactivation_reactivation;
	BOOL AwaitCapabilities;
	rdpSettings* settingsCopy;
};

BOOL rdp_read_security_header(wStream* s, UINT16* flags);
void rdp_write_security_header(wStream* s, UINT16 flags);

BOOL rdp_read_share_control_header(wStream* s, UINT16* length, UINT16* type, UINT16* channel_id);
void rdp_write_share_control_header(wStream* s, UINT16 length, UINT16 type, UINT16 channel_id);

BOOL rdp_read_share_data_header(wStream* s, UINT16* length, BYTE* type, UINT32* share_id, 
			BYTE *compressed_type, UINT16 *compressed_len);

void rdp_write_share_data_header(wStream* s, UINT16 length, BYTE type, UINT32 share_id);

int rdp_init_stream(rdpRdp* rdp, wStream* s);
wStream* rdp_send_stream_init(rdpRdp* rdp);

BOOL rdp_read_header(rdpRdp* rdp, wStream* s, UINT16* length, UINT16* channel_id);
void rdp_write_header(rdpRdp* rdp, wStream* s, UINT16 length, UINT16 channel_id);

int rdp_init_stream_pdu(rdpRdp* rdp, wStream* s);
BOOL rdp_send_pdu(rdpRdp* rdp, wStream* s, UINT16 type, UINT16 channel_id);

wStream* rdp_data_pdu_init(rdpRdp* rdp);
BOOL rdp_send_data_pdu(rdpRdp* rdp, wStream* s, BYTE type, UINT16 channel_id);
int rdp_recv_data_pdu(rdpRdp* rdp, wStream* s);

BOOL rdp_send(rdpRdp* rdp, wStream* s, UINT16 channelId);

int rdp_send_channel_data(rdpRdp* rdp, UINT16 channelId, BYTE* data, int size);

wStream* rdp_message_channel_pdu_init(rdpRdp* rdp);
BOOL rdp_send_message_channel_pdu(rdpRdp* rdp, wStream* s, UINT16 sec_flags);
int rdp_recv_message_channel_pdu(rdpRdp* rdp, wStream* s);

int rdp_recv_out_of_sequence_pdu(rdpRdp* rdp, wStream* s);

void rdp_set_blocking_mode(rdpRdp* rdp, BOOL blocking);
int rdp_check_fds(rdpRdp* rdp);

rdpRdp* rdp_new(rdpContext* context);
void rdp_reset(rdpRdp* rdp);
void rdp_free(rdpRdp* rdp);

#ifdef WITH_DEBUG_RDP
#define DEBUG_RDP(fmt, ...) DEBUG_CLASS(RDP, fmt, ## __VA_ARGS__)
#else
#define DEBUG_RDP(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

BOOL rdp_decrypt(rdpRdp* rdp, wStream* s, int length, UINT16 securityFlags);

BOOL rdp_set_error_info(rdpRdp* rdp, UINT32 errorInfo);

#endif /* __RDP_H */
