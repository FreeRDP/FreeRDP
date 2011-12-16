/**
 * FreeRDP: A Remote Desktop Protocol Client
 * NT LAN Manager Security Support Provider (NTLMSSP)
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

#ifndef __NTLMSSP_H
#define __NTLMSSP_H

#include "credssp.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/blob.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/unicode.h>

struct _AV_PAIR
{
	uint16 length;
	uint8* value;
};
typedef struct _AV_PAIR AV_PAIR;

struct _AV_PAIRS
{
	AV_PAIR NbComputerName;
	AV_PAIR NbDomainName;
	AV_PAIR DnsComputerName;
	AV_PAIR DnsDomainName;
	AV_PAIR DnsTreeName;
	AV_PAIR Timestamp;
	AV_PAIR Restrictions;
	AV_PAIR TargetName;
	AV_PAIR ChannelBindings;
	uint32 Flags;
};
typedef struct _AV_PAIRS AV_PAIRS;

enum _AV_ID
{
	MsvAvEOL,
	MsvAvNbComputerName,
	MsvAvNbDomainName,
	MsvAvDnsComputerName,
	MsvAvDnsDomainName,
	MsvAvDnsTreeName,
	MsvAvFlags,
	MsvAvTimestamp,
	MsvAvRestrictions,
	MsvAvTargetName,
	MsvChannelBindings
};
typedef enum _AV_ID AV_ID;

enum _NTLMSSP_STATE
{
	NTLMSSP_STATE_INITIAL,
	NTLMSSP_STATE_NEGOTIATE,
	NTLMSSP_STATE_CHALLENGE,
	NTLMSSP_STATE_AUTHENTICATE,
	NTLMSSP_STATE_FINAL
};
typedef enum _NTLMSSP_STATE NTLMSSP_STATE;

struct _NTLMSSP
{
	NTLMSSP_STATE state;
	rdpBlob password;
	rdpBlob username;
	rdpBlob domain;
	rdpBlob workstation;
	rdpBlob target_info;
	rdpBlob target_name;
	rdpBlob spn;
	UNICONV *uniconv;
	uint32 negotiate_flags;
	uint8 timestamp[8];
	uint8 server_challenge[8];
	uint8 client_challenge[8];
	uint8 session_base_key[16];
	uint8 key_exchange_key[16];
	uint8 random_session_key[16];
	uint8 exported_session_key[16];
	uint8 encrypted_random_session_key[16];
	uint8 client_signing_key[16];
	uint8 client_sealing_key[16];
	uint8 server_signing_key[16];
	uint8 server_sealing_key[16];
	uint8 message_integrity_check[16];
	rdpBlob nt_challenge_response;
	rdpBlob lm_challenge_response;
	rdpBlob negotiate_message;
	rdpBlob challenge_message;
	rdpBlob authenticate_message;
	CryptoRc4 send_rc4_seal;
	CryptoRc4 recv_rc4_seal;
	AV_PAIRS *av_pairs;
	int send_seq_num;
	int recv_seq_num;
	int ntlm_v2;
};
typedef struct _NTLMSSP NTLMSSP;

void ntlmssp_set_username(NTLMSSP* ntlmssp, char* username);
void ntlmssp_set_domain(NTLMSSP* ntlmssp, char* domain);
void ntlmssp_set_password(NTLMSSP* ntlmssp, char* password);
void ntlmssp_set_workstation(NTLMSSP* ntlmssp, char* workstation);

void ntlmssp_generate_client_challenge(NTLMSSP* ntlmssp);
void ntlmssp_generate_key_exchange_key(NTLMSSP* ntlmssp);
void ntlmssp_generate_random_session_key(NTLMSSP* ntlmssp);
void ntlmssp_generate_exported_session_key(NTLMSSP* ntlmssp);
void ntlmssp_encrypt_random_session_key(NTLMSSP* ntlmssp);

void ntlmssp_generate_timestamp(NTLMSSP* ntlmssp);
void ntlmssp_generate_client_signing_key(NTLMSSP* ntlmssp);
void ntlmssp_generate_server_signing_key(NTLMSSP* ntlmssp);
void ntlmssp_generate_client_sealing_key(NTLMSSP* ntlmssp);
void ntlmssp_generate_server_sealing_key(NTLMSSP* ntlmssp);
void ntlmssp_init_rc4_seal_states(NTLMSSP* ntlmssp);

void ntlmssp_compute_lm_hash(char* password, char* hash);
void ntlmssp_compute_ntlm_hash(rdpBlob* password, char* hash);
void ntlmssp_compute_ntlm_v2_hash(NTLMSSP* ntlmssp, char* hash);

void ntlmssp_compute_lm_response(char* password, char* challenge, char* response);
void ntlmssp_compute_lm_v2_response(NTLMSSP* ntlmssp);
void ntlmssp_compute_ntlm_v2_response(NTLMSSP* ntlmssp);

void ntlmssp_populate_av_pairs(NTLMSSP* ntlmssp);
void ntlmssp_input_av_pairs(NTLMSSP* ntlmssp, STREAM* s);
void ntlmssp_output_av_pairs(NTLMSSP* ntlmssp, STREAM* s);
void ntlmssp_free_av_pairs(NTLMSSP* ntlmssp);

void ntlmssp_compute_message_integrity_check(NTLMSSP* ntlmssp);

void ntlmssp_encrypt_message(NTLMSSP* ntlmssp, rdpBlob* msg, rdpBlob* encrypted_msg, uint8* signature);
int ntlmssp_decrypt_message(NTLMSSP* ntlmssp, rdpBlob* encrypted_msg, rdpBlob* msg, uint8* signature);

int ntlmssp_recv(NTLMSSP* ntlmssp, STREAM* s);
int ntlmssp_send(NTLMSSP* ntlmssp, STREAM* s);

NTLMSSP* ntlmssp_new();
void ntlmssp_init(NTLMSSP* ntlmssp);
void ntlmssp_free(NTLMSSP* ntlmssp);

#ifdef WITH_DEBUG_NLA
#define DEBUG_NLA(fmt, ...) DEBUG_CLASS(NLA, fmt, ## __VA_ARGS__)
#else
#define DEBUG_NLA(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __NTLMSSP_H */
