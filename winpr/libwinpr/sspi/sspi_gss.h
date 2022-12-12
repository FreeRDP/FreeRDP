/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Generic Security Service Application Program Interface (GSSAPI)
 *
 * Copyright 2015 ANSSI, Author Thomas Calderon
 * Copyright 2015 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
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

#ifndef WINPR_SSPI_GSS_PRIVATE_H
#define WINPR_SSPI_GSS_PRIVATE_H

#include <winpr/sspi.h>
#include <winpr/asn1.h>

#ifdef WITH_KRB5_MIT
#include <krb5/krb5.h>
typedef krb5_data sspi_gss_data;
#elif defined(WITH_KRB5_HEIMDAL)
#include <krb5.h>
typedef krb5_data sspi_gss_data;
#else
typedef struct
{
	int32_t magic;
	unsigned int length;
	char* data;
} sspi_gss_data;
#endif

#define SSPI_GSS_C_DELEG_FLAG 1
#define SSPI_GSS_C_MUTUAL_FLAG 2
#define SSPI_GSS_C_REPLAY_FLAG 4
#define SSPI_GSS_C_SEQUENCE_FLAG 8
#define SSPI_GSS_C_CONF_FLAG 16
#define SSPI_GSS_C_INTEG_FLAG 32

#define FLAG_SENDER_IS_ACCEPTOR 0x01
#define FLAG_WRAP_CONFIDENTIAL 0x02
#define FLAG_ACCEPTOR_SUBKEY 0x04

#define KG_USAGE_ACCEPTOR_SEAL 22
#define KG_USAGE_ACCEPTOR_SIGN 23
#define KG_USAGE_INITIATOR_SEAL 24
#define KG_USAGE_INITIATOR_SIGN 25

#define TOK_ID_AP_REQ 0x0100
#define TOK_ID_AP_REP 0x0200
#define TOK_ID_ERROR 0x0300
#define TOK_ID_TGT_REQ 0x0400
#define TOK_ID_TGT_REP 0x0401

#define TOK_ID_MIC 0x0404
#define TOK_ID_WRAP 0x0504
#define TOK_ID_MIC_V1 0x0101
#define TOK_ID_WRAP_V1 0x0201

#define GSS_CHECKSUM_TYPE 0x8003

static INLINE BOOL sspi_gss_oid_compare(const WinPrAsn1_OID* oid1, const WinPrAsn1_OID* oid2)
{
	WINPR_ASSERT(oid1);
	WINPR_ASSERT(oid2);

	return (oid1->len == oid2->len) && (memcmp(oid1->data, oid2->data, oid1->len) == 0);
}

BOOL sspi_gss_wrap_token(SecBuffer* buf, const WinPrAsn1_OID* oid, uint16_t tok_id,
                         const sspi_gss_data* token);
BOOL sspi_gss_unwrap_token(const SecBuffer* buf, WinPrAsn1_OID* oid, uint16_t* tok_id,
                           sspi_gss_data* token);

#endif /* WINPR_SSPI_GSS_PRIVATE_H */
