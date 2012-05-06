/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Kerberos Auth Protocol DER Encode
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kerberos_encode.h"

int krb_encode_sequence_tag(STREAM* s, uint32 len)
{
	uint8* bm;
	uint32 totlen;
	totlen = _GET_BYTE_LENGTH(len + 2) + 1;
	stream_rewind(s, totlen);
	stream_get_mark(s, bm);
	der_write_sequence_tag(s, len);
	stream_set_mark(s, bm);
	return totlen;
}

int krb_encode_contextual_tag(STREAM* s, uint8 tag, uint32 len)
{
	uint8* bm;
	uint32 totlen;
	totlen = _GET_BYTE_LENGTH(len) + 1;
	stream_rewind(s, totlen);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, tag, len, true);
	stream_set_mark(s, bm);
	return totlen;
}

int krb_encode_application_tag(STREAM* s, uint8 tag, uint32 len)
{
	uint8* bm;
	uint32 totlen;
	totlen = _GET_BYTE_LENGTH(len) + 1;
	stream_rewind(s, totlen);
	stream_get_mark(s, bm);
	der_write_application_tag(s, tag, len);
	stream_set_mark(s, bm);
	return totlen;
}

int krb_encode_recordmark(STREAM* s, uint32 len)
{
	uint8* bm;
	stream_rewind(s, 4);
	stream_get_mark(s, bm);
	stream_write_uint32_be(s, len);
	stream_set_mark(s, bm);
	return 4;
}

int krb_encode_cname(STREAM* s, uint8 tag, char* cname)
{
	uint8* bm;
	uint32 len;
	char* names[2];

	names[0] = cname;
	names[1] = NULL;

	len = strlen(cname) + 15;
	stream_rewind(s, len);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, tag, len - 2, true);	
	der_write_principal_name(s, NAME_TYPE_PRINCIPAL, names);
	stream_set_mark(s, bm);

	return len;
}

int krb_encode_sname(STREAM* s, uint8 tag, char* sname)
{
	uint8* bm;
	char* str;
	char* names[3];
	uint32 len, tmp;
	
	len = strlen(sname) - 1 + 17;
	
	stream_rewind(s, len);
	stream_get_mark(s, bm);
	
	der_write_contextual_tag(s, tag, len - 2, true);

	tmp = strchr(sname, '/') - sname;

	str = (char*) xzalloc(tmp + 1);
	strncpy(str, sname, tmp);
	
	names[0] = str;
	names[1] = sname + tmp + 1;
	names[2] = NULL;

	der_write_principal_name(s, NAME_TYPE_SERVICE, names);
	
	xfree(str);
	stream_set_mark(s, bm);
	
	return len;
}

int krb_encode_uint8(STREAM* s, uint8 tag, uint8 val)
{
	uint8* bm;
	stream_rewind(s, 5);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, tag, 3, true);
	der_write_integer(s, val);
	stream_set_mark(s, bm);
	return 5;
}

int krb_encode_integer(STREAM* s, uint8 tag, int val)
{
	uint8* bm;
	uint32 totlen;
	totlen = der_skip_integer(val);
	stream_rewind(s, totlen + 2);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, tag, totlen, true);
	der_write_integer(s, val);
	stream_set_mark(s, bm);
	return (totlen + 2);
}

int krb_encode_options(STREAM* s, uint8 tag, uint32 options)
{
	uint8* bm;
	stream_rewind(s, 9);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, tag, 7, true);
	der_write_bit_string_tag(s, 5, 0);
	stream_write_uint32_be(s, options);
	stream_set_mark(s, bm);
	return 9;
}

int krb_encode_string(STREAM* s, uint8 tag, char* str)
{
	uint8* bm;
	uint32 len;
	len = strlen(str);
	stream_rewind(s, len + 4);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, tag, len + 2, true);
	der_write_general_string(s, str);
	stream_set_mark(s, bm);
	return (len + 4);
}

int krb_encode_time(STREAM* s, uint8 tag, char* strtime)
{
	uint8* bm;
	stream_rewind(s, 19);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, tag, 17, true);
	der_write_generalized_time(s, strtime);
	stream_set_mark(s, bm);
	return 19;
}

int krb_encode_octet_string(STREAM* s, uint8* string, uint32 len)
{
	uint8* bm;
	uint32 totlen;
	totlen = len + _GET_BYTE_LENGTH(len) + 1;
	stream_rewind(s, totlen);
	stream_get_mark(s, bm);
	der_write_octet_string(s, string, len);
	stream_set_mark(s, bm);
	return totlen;
}

int krb_encode_encrypted_data(STREAM* s, KrbENCData* enc_data)
{
	uint32 totlen;

	/* Encrypted Data[2] */
	totlen = krb_encode_octet_string(s, (enc_data->encblob).data, (enc_data->encblob).length);
	totlen += krb_encode_contextual_tag(s, 2, totlen);

	/* Encrypted key version no[1] */
	if(enc_data->kvno != -1)
		totlen += krb_encode_uint8(s, 1, enc_data->kvno);

	/* Encrypted Type[0] */
	totlen += krb_encode_integer(s, 0, enc_data->enctype);

	totlen += krb_encode_sequence_tag(s, totlen);
	return totlen;
}

int krb_encode_checksum(STREAM* s, rdpBlob* cksum, int cktype)
{
	uint32 totlen;

	/* Checksum Data[1] */
	totlen = krb_encode_octet_string(s, cksum->data, cksum->length);
	totlen += krb_encode_contextual_tag(s, 1, totlen);

	/* Checksum Type[0] */
	totlen += krb_encode_integer(s, 0, cktype);

	totlen += krb_encode_sequence_tag(s, totlen);
	return totlen;
}

int krb_encode_padata(STREAM* s, PAData** pa_data)
{
	uint32 totlen;
	uint32 curlen;
	PAData** lpa_data;

	totlen = 0;
	lpa_data = pa_data;

	while (*lpa_data != NULL)
	{
		/* padata value */
		curlen = krb_encode_octet_string(s, ((*lpa_data)->value).data, ((*lpa_data)->value).length);
		curlen += krb_encode_contextual_tag(s, 2, curlen);

		/* padata type */
		curlen += krb_encode_integer(s, 1, ((*lpa_data)->type));
		curlen += krb_encode_sequence_tag(s, curlen);
		totlen += curlen;
		lpa_data++;
	}

	totlen += krb_encode_sequence_tag(s, totlen);

	return totlen;
}


int krb_encode_authenticator(STREAM* s, Authenticator *krb_auth)
{
	uint8* bm;
	uint32 totlen, curlen;

	/* seq no[7] */
	stream_rewind(s, 8);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, 7, 6, true);
	der_write_integer(s, krb_auth->seqno);
	totlen = 8;

	stream_set_mark(s, bm);
	
	/* ctime[5] */
	totlen += krb_encode_time(s, 5, krb_auth->ctime);
	
	/* cusec[4] */
	totlen += krb_encode_integer(s, 4, krb_auth->cusec);

	/* cksum[3] */
	curlen = krb_encode_checksum(s, krb_auth->cksum, krb_auth->cksumtype);
	totlen += krb_encode_contextual_tag(s, 3, curlen) + curlen;

	/* cname[2] */
	totlen += krb_encode_cname(s, 2, krb_auth->cname);

	/* crealm[1] */
	totlen += krb_encode_string(s, 1, krb_auth->crealm);

	/* avno[0] */
	totlen += krb_encode_uint8(s, 0, krb_auth->avno);

	totlen += krb_encode_sequence_tag(s, totlen);
	totlen += krb_encode_application_tag(s, 2, totlen);

	return totlen;
}

int krb_encode_ticket(STREAM* s, uint8 tag, Ticket* ticket)
{
	uint32 totlen;

	/* Encrypted DATA[3] */
	totlen = krb_encode_encrypted_data(s, &(ticket->enc_part));
	totlen += krb_encode_contextual_tag(s, 3, totlen);

	/* sname[2] */
	totlen += krb_encode_sname(s, 2, ticket->sname);

	/* realm[1] */
	totlen += krb_encode_string(s, 1, ticket->realm);

	/* ticket vno[0] */
	totlen += krb_encode_uint8(s, 0, ticket->tktvno);

	totlen += krb_encode_sequence_tag(s, totlen);
	totlen += krb_encode_application_tag(s, 1, totlen);
	totlen += krb_encode_contextual_tag(s, tag, totlen);

	return totlen;
}

int krb_encode_req_body(STREAM* s, KDCReqBody* req_body, int msgtype)
{
	uint8* bm;
	uint32 totlen;
	totlen = 0;

	/* ETYPE[8] we support des-cbc-crc, rc4-hmac and aes-128-ctc-hmac-sha1-96 only */
	stream_rewind(s, 10);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, 8, 8, true);
	der_write_sequence_tag(s, 6);
	der_write_integer(s, ETYPE_RC4_HMAC);
	der_write_integer(s, ETYPE_DES_CBC_CRC);
	totlen += 10;
	
	stream_set_mark(s, bm);

	/* NONCE[7] */
	stream_rewind(s, 8);
	stream_get_mark(s, bm);
	der_write_contextual_tag(s, 7, 6, true);
	der_write_integer(s, req_body->nonce);
	totlen += 8;

	stream_set_mark(s, bm);

	/* till[5] and rtime (tag 6)*/
	totlen += krb_encode_time(s, 6, req_body->rtime);
	totlen += krb_encode_time(s, 5, req_body->till);

	/* SNAME[3]*/
	totlen += krb_encode_sname(s, 3, req_body->sname);

	/* REALM[2] */
	totlen += krb_encode_string(s, 2, req_body->realm);

	/* CNAME[1] */
	totlen += krb_encode_cname(s, 1, req_body->cname);

	/* KDCOptions[0]*/
	totlen += krb_encode_options(s, 0, req_body->kdc_options);

	/* KDC_BODY */
	totlen += krb_encode_sequence_tag(s, totlen);

	return totlen;
}

int krb_encode_apreq(STREAM* s, KrbAPREQ* krb_apreq)
{
	uint32 totlen;

	/* Encrypted Authenticator[4] */
 	totlen = krb_encode_encrypted_data(s, &(krb_apreq->enc_auth));
	totlen += krb_encode_contextual_tag(s, 4, totlen);

	/* Ticket[3] */
	totlen += krb_encode_ticket(s, 3, krb_apreq->ticket);

	/* APOPTIONS[2] */
	totlen += krb_encode_options(s, 2, krb_apreq->ap_options);

	/* MSGTYPE[1] */
	totlen += krb_encode_uint8(s , 1, krb_apreq->type);

	/* VERSION NO[0] */
	totlen += krb_encode_uint8(s, 0, krb_apreq->pvno);
	
	totlen += krb_encode_sequence_tag(s, totlen);
	totlen += krb_encode_application_tag(s, krb_apreq->type, totlen);

	return totlen;
}

int krb_encode_tgtreq(STREAM* s, KrbTGTREQ* krb_tgtreq)
{
	uint32 totlen;
	totlen = 0;

	/* realm[3] optional */
	if(krb_tgtreq->realm != NULL)
		totlen += krb_encode_string(s, 3, krb_tgtreq->realm);

	/* sname[2] optional */
	if(krb_tgtreq->sname != NULL)
		totlen += krb_encode_sname(s, 3, krb_tgtreq->sname);

	/* msgtype[1] */
	totlen += krb_encode_uint8(s , 1, krb_tgtreq->type);

	/* pvno[0] */
	totlen += krb_encode_uint8(s, 0, krb_tgtreq->pvno);

	totlen += krb_encode_sequence_tag(s, totlen);

	return totlen;
}
