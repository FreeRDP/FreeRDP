/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "mcs.h"
#include "tpkt.h"
#include "fastpath.h"
#include "tpdu.h"
#include "nego.h"
#include "input.h"
#include "update.h"
#include "license.h"
#include "errinfo.h"
#include "extension.h"
#include "security.h"
#include "transport.h"
#include "connection.h"
#include "redirection.h"
#include "capabilities.h"
#include "channel.h"
#include "mppc.h"

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>

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

#define SEC_PKT_CS_MASK			(SEC_EXCHANGE_PKT | SEC_INFO_PKT)
#define SEC_PKT_SC_MASK			(SEC_LICENSE_PKT | SEC_REDIRECTION_PKT)
#define SEC_PKT_MASK			(SEC_PKT_CS_MASK | SEC_PKT_SC_MASK)

#define RDP_SECURITY_HEADER_LENGTH	4
#define RDP_SHARE_CONTROL_HEADER_LENGTH	6
#define RDP_SHARE_DATA_HEADER_LENGTH	12
#define RDP_PACKET_HEADER_MAX_LENGTH	(TPDU_DATA_LENGTH + MCS_SEND_DATA_HEADER_MAX_LENGTH)

#define PDU_TYPE_DEMAND_ACTIVE		0x1
#define PDU_TYPE_CONFIRM_ACTIVE		0x3
#define PDU_TYPE_DEACTIVATE_ALL		0x6
#define PDU_TYPE_DATA			0x7
#define PDU_TYPE_SERVER_REDIRECTION	0xA

#define FINALIZE_SC_SYNCHRONIZE_PDU		0x01
#define FINALIZE_SC_CONTROL_COOPERATE_PDU	0x02
#define FINALIZE_SC_CONTROL_GRANTED_PDU		0x04
#define FINALIZE_SC_FONT_MAP_PDU		0x08
#define FINALIZE_SC_COMPLETE			0x0F

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

/* Compression Types */
#define PACKET_COMPRESSED		0x20
#define PACKET_AT_FRONT			0x40
#define PACKET_FLUSHED			0x80
#define PACKET_COMPR_TYPE_8K		0x00
#define PACKET_COMPR_TYPE_64K		0x01
#define PACKET_COMPR_TYPE_RDP6		0x02
#define PACKET_COMPR_TYPE_RDP61		0x03
#define CompressionTypeMask		0x0F

/* Stream Identifiers */
#define STREAM_UNDEFINED		0x00
#define STREAM_LOW			0x01
#define STREAM_MED			0x02
#define STREAM_HI			0x04

struct rdp_rdp
{
	int state;
	freerdp* instance;
	struct rdp_mcs* mcs;
	struct rdp_nego* nego;
	struct rdp_input* input;
	struct rdp_update* update;
	struct rdp_fastpath* fastpath;
	struct rdp_license* license;
	struct rdp_redirection* redirection;
	struct rdp_settings* settings;
	struct rdp_transport* transport;
	struct rdp_extension* extension;
	struct rdp_mppc* mppc;
	struct crypto_rc4_struct* rc4_decrypt_key;
	int decrypt_use_count;
	struct crypto_rc4_struct* rc4_encrypt_key;
	int encrypt_use_count;
	struct crypto_des3_struct* fips_encrypt;
	struct crypto_des3_struct* fips_decrypt;
	struct crypto_hmac_struct* fips_hmac;
	uint32 sec_flags;
	boolean do_crypt;
	boolean do_secure_checksum;
	uint8 sign_key[16];
	uint8 decrypt_key[16];
	uint8 encrypt_key[16];
	uint8 decrypt_update_key[16];
	uint8 encrypt_update_key[16];
	int rc4_key_len;
	uint8 fips_sign_key[20];
	uint8 fips_encrypt_key[24];
	uint8 fips_decrypt_key[24];
	uint32 errorInfo;
	uint32 finalize_sc_pdus;
	boolean disconnect;
};

void rdp_read_security_header(STREAM* s, uint16* flags);
void rdp_write_security_header(STREAM* s, uint16 flags);

boolean rdp_read_share_control_header(STREAM* s, uint16* length, uint16* type, uint16* channel_id);
void rdp_write_share_control_header(STREAM* s, uint16 length, uint16 type, uint16 channel_id);

boolean rdp_read_share_data_header(STREAM* s, uint16* length, uint8* type, uint32* share_id, 
			uint8 *compressed_type, uint16 *compressed_len);

void rdp_write_share_data_header(STREAM* s, uint16 length, uint8 type, uint32 share_id);

STREAM* rdp_send_stream_init(rdpRdp* rdp);

boolean rdp_read_header(rdpRdp* rdp, STREAM* s, uint16* length, uint16* channel_id);
void rdp_write_header(rdpRdp* rdp, STREAM* s, uint16 length, uint16 channel_id);

STREAM* rdp_pdu_init(rdpRdp* rdp);
boolean rdp_send_pdu(rdpRdp* rdp, STREAM* s, uint16 type, uint16 channel_id);

STREAM* rdp_data_pdu_init(rdpRdp* rdp);
boolean rdp_send_data_pdu(rdpRdp* rdp, STREAM* s, uint8 type, uint16 channel_id);
void rdp_recv_data_pdu(rdpRdp* rdp, STREAM* s);

boolean rdp_send(rdpRdp* rdp, STREAM* s, uint16 channel_id);
void rdp_recv(rdpRdp* rdp);

int rdp_send_channel_data(rdpRdp* rdp, int channel_id, uint8* data, int size);

boolean rdp_recv_out_of_sequence_pdu(rdpRdp* rdp, STREAM* s);

void rdp_set_blocking_mode(rdpRdp* rdp, boolean blocking);
int rdp_check_fds(rdpRdp* rdp);

rdpRdp* rdp_new(freerdp* instance);
void rdp_free(rdpRdp* rdp);

#ifdef WITH_DEBUG_RDP
#define DEBUG_RDP(fmt, ...) DEBUG_CLASS(RDP, fmt, ## __VA_ARGS__)
#else
#define DEBUG_RDP(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

boolean rdp_decrypt(rdpRdp* rdp, STREAM* s, int length, uint16 securityFlags);

#endif /* __RDP_H */
