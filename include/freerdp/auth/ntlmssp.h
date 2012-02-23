/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NT LAN Manager Security Support Provider (NTLMSSP)
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_AUTH_NTLMSSP_H
#define FREERDP_AUTH_NTLMSSP_H

#include <freerdp/crypto/crypto.h>

#include <freerdp/api.h>
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
	boolean server;
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

FREERDP_API void ntlmssp_set_username(NTLMSSP* ntlmssp, char* username);
FREERDP_API void ntlmssp_set_domain(NTLMSSP* ntlmssp, char* domain);
FREERDP_API void ntlmssp_set_password(NTLMSSP* ntlmssp, char* password);
FREERDP_API void ntlmssp_set_workstation(NTLMSSP* ntlmssp, char* workstation);
FREERDP_API void ntlmssp_set_target_name(NTLMSSP* ntlmssp, char* target_name);

FREERDP_API void ntlmssp_generate_client_challenge(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_generate_server_challenge(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_generate_key_exchange_key(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_generate_random_session_key(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_generate_exported_session_key(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_encrypt_random_session_key(NTLMSSP* ntlmssp);

FREERDP_API void ntlmssp_generate_timestamp(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_generate_client_signing_key(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_generate_server_signing_key(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_generate_client_sealing_key(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_generate_server_sealing_key(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_init_rc4_seal_states(NTLMSSP* ntlmssp);

FREERDP_API void ntlmssp_compute_lm_hash(char* password, char* hash);
FREERDP_API void ntlmssp_compute_ntlm_hash(rdpBlob* password, char* hash);
FREERDP_API void ntlmssp_compute_ntlm_v2_hash(NTLMSSP* ntlmssp, char* hash);

FREERDP_API void ntlmssp_compute_lm_response(char* password, char* challenge, char* response);
FREERDP_API void ntlmssp_compute_lm_v2_response(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_compute_ntlm_v2_response(NTLMSSP* ntlmssp);

FREERDP_API void ntlmssp_populate_av_pairs(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_input_av_pairs(NTLMSSP* ntlmssp, STREAM* s);
FREERDP_API void ntlmssp_output_av_pairs(NTLMSSP* ntlmssp, STREAM* s);
FREERDP_API void ntlmssp_free_av_pairs(NTLMSSP* ntlmssp);

FREERDP_API void ntlmssp_compute_message_integrity_check(NTLMSSP* ntlmssp);

FREERDP_API void ntlmssp_encrypt_message(NTLMSSP* ntlmssp, rdpBlob* msg, rdpBlob* encrypted_msg, uint8* signature);
FREERDP_API int ntlmssp_decrypt_message(NTLMSSP* ntlmssp, rdpBlob* encrypted_msg, rdpBlob* msg, uint8* signature);

FREERDP_API int ntlmssp_recv(NTLMSSP* ntlmssp, STREAM* s);
FREERDP_API int ntlmssp_send(NTLMSSP* ntlmssp, STREAM* s);

FREERDP_API NTLMSSP* ntlmssp_client_new();
FREERDP_API NTLMSSP* ntlmssp_server_new();
FREERDP_API void ntlmssp_init(NTLMSSP* ntlmssp);
FREERDP_API void ntlmssp_free(NTLMSSP* ntlmssp);

#ifdef WITH_DEBUG_NLA
#define DEBUG_NLA(fmt, ...) DEBUG_CLASS(NLA, fmt, ## __VA_ARGS__)
#else
#define DEBUG_NLA(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_AUTH_NTLMSSP_H */
