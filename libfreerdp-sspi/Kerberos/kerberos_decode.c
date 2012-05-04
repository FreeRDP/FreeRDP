/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Kerberos Auth Protocol DER Decode
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

#include "kerberos_decode.h"

int krb_decode_application_tag(STREAM* s, uint8 tag, int *length)
{
	uint8* bm;
	stream_get_mark(s, bm);

	if(!der_read_application_tag(s, tag, length))
	{
		stream_set_mark(s, bm);
		*length = 0;
		return 0;
	}
	return (s->p - bm);
}

int krb_decode_sequence_tag(STREAM* s, int *length)
{
	uint8* bm;
	stream_get_mark(s, bm);

	if(!der_read_sequence_tag(s, length))
	{
		stream_set_mark(s, bm);
		*length = 0;
		return 0;
	}
	return (s->p - bm);
}

int krb_decode_contextual_tag(STREAM* s, uint8 tag, int *length)
{
	uint8* bm;
	stream_get_mark(s, bm);

	if(!der_read_contextual_tag(s, tag, length, true))
	{
		stream_set_mark(s, bm);
		*length = 0;
		return 0;
	}
	return (s->p - bm);
}

int krb_skip_contextual_tag(STREAM* s, uint8 tag)
{
	uint8* bm;
	int length;
	stream_get_mark(s, bm);

	if(!der_read_contextual_tag(s, tag, &length, true))
	{
		stream_set_mark(s, bm);
		length = 0;
		return 0;
	}
	stream_seek(s, length);
	return (s->p - bm);
}

int krb_decode_integer(STREAM* s, uint32* value)
{
	uint8* bm;
	stream_get_mark(s, bm);

	if(!der_read_integer(s, value))
	{
		stream_set_mark(s, bm);
		*value = 0;
		return 0;
	}
	else
		return (s->p - bm);
}

int krb_decode_time(STREAM* s, uint8 tag, char** str)
{
	uint8* bm;
	int tmp;
	stream_get_mark(s, bm);

	if((krb_decode_contextual_tag(s, tag, &tmp) == 0) || (tmp != 17) || !der_read_generalized_time(s, str))
	{
		stream_set_mark(s, bm);
		return 0;
	}
	else
		return (s->p - bm);
}

int krb_decode_int(STREAM* s, uint8 tag, uint32* result)
{
	uint8* bm;
	int totlen, len, tmp;
	totlen = 0;
	stream_get_mark(s, bm);
	
	if((len = krb_decode_contextual_tag(s, tag, &tmp)) == 0)
		goto err;

	totlen += len;

	if(((len = krb_decode_integer(s, result)) == 0) || (tmp != len))
		goto err;

	totlen += len;
	return totlen;

	err:
		stream_set_mark(s, bm);
		*result = 0;
		return 0;
}

int krb_decode_flags(STREAM* s, uint8 tag, uint32* flags)
{
	uint8* bm;
	int len, tmp, verlen;
	verlen = 9;
	stream_get_mark(s, bm);

	if(((len = krb_decode_contextual_tag(s, tag, &tmp)) == 0) || (len != (verlen - tmp)))
		goto err;

	verlen -= len;

	if(!der_read_bit_string(s, &tmp, (uint8*)&len) || (tmp != 5))
		goto err;

	stream_read_uint32_be(s, *flags);
	return 9;

	err:
		stream_set_mark(s, bm);
		return 0;
}

int krb_decode_string(STREAM* s, uint8 tag, char** str)
{
	uint8* bm;
	int totlen, len, tmp;
	totlen = 0;
	stream_get_mark(s, bm);
	
	if((len = krb_decode_contextual_tag(s, tag, &tmp)) == 0)
		goto err;

	totlen += len;

	if(((*str = der_read_general_string(s, &len)) == NULL) || tmp != len)
		goto err;

	totlen += len;
	return totlen;

	err:
		stream_set_mark(s, bm);
		return 0;
}

int krb_decode_octet_string(STREAM* s, uint8 tag, uint8** data, int* length)
{	
	uint8* bm;
	int totlen, len, tmp, verlen;
	totlen = 0;
	stream_get_mark(s, bm);

	if((len = krb_decode_contextual_tag(s, tag, &verlen)) == 0) 
		goto err;

	totlen += len;

	if(!der_read_octet_string(s, &tmp))
	{
		stream_set_mark(s, bm);
		*length = 0;
		return 0;
	}

	totlen += tmp + _GET_BYTE_LENGTH(tmp) + 1;
	verlen -= (tmp + _GET_BYTE_LENGTH(tmp) + 1);
	
	if(verlen != 0)
		goto err;

	*data = xzalloc(tmp);
	stream_read(s, *data, tmp);
	*length = tmp;

	return totlen;

	err:
		stream_set_mark(s, bm);
		return 0;
}


int krb_decode_cname(STREAM* s, uint8 tag, char** str)
{
	uint8* bm;
	int totlen, len, tmp, type, verlen;
	totlen = 0;
	stream_get_mark(s, bm);

	/* Contextual tag */
	if((len = krb_decode_contextual_tag(s, tag, &verlen)) == 0)
		goto err;

	totlen += len;

	/* cname sequence tag */
	if(((len = krb_decode_sequence_tag(s, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	/* cname name type[0] */
	if((verlen <= 0) || ((len = krb_decode_int(s, 0, (uint32*)&type)) == 0) || (type != KRB_NAME_PRINCIPAL))
		goto err;

	totlen += len;
	verlen -= len;

	/* cname name[1] */
	if((verlen <= 0) || ((len = krb_decode_contextual_tag(s, 1, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	if((verlen <= 0) || ((len = krb_decode_sequence_tag(s, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	if(((*str = der_read_general_string(s, &len)) == NULL))
		goto err;

	totlen += len;

	if(verlen != len)
	{
		xfree(*str);
		*str = NULL;
		goto err;
	}

	return totlen;

	err:
		stream_set_mark(s, bm);
		return 0;
}

int krb_decode_sname(STREAM* s, uint8 tag, char** str)
{
	uint8* bm;
	int totlen, len, tmp, type, verlen;
	char* name;
	char* realm;
	totlen = 0;
	stream_get_mark(s, bm);

	/* Contextual tag */
	if((len = krb_decode_contextual_tag(s, tag, &verlen)) == 0)
		goto err;

	totlen += len;

	/* sname sequence tag */
	if(((len = krb_decode_sequence_tag(s, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	/* sname name type[0] */
	if((verlen <= 0) || ((len = krb_decode_int(s, 0, (uint32*)&type)) == 0) || (type != KRB_NAME_SERVICE))
		goto err;

	totlen += len;
	verlen -= len;

	/* sname name & realm[1] */
	if((verlen <= 0) || ((len = krb_decode_contextual_tag(s, 1, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	if((verlen <= 0) || ((len = krb_decode_sequence_tag(s, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	if(((name = der_read_general_string(s, &len)) == NULL))
		goto err;

	totlen += len;
	verlen -= len;
	
	if(((realm = der_read_general_string(s, &len)) == NULL))
	{
		xfree(name);
		goto err;
	}

	totlen += len;

	if(verlen != len)
	{
		xfree(name);
		xfree(realm);
		goto err;
	}

	*str = xzalloc(strlen(name) + strlen(realm) + 2);
	strcpy(*str, name);
	strcat(*str, "/");
	strcat(*str, realm);

	xfree(name);
	xfree(realm);
	return totlen;

	err:
		stream_set_mark(s, bm);
		return 0;
}

int krb_decode_enckey(STREAM* s, KrbENCKey* key)
{
	uint8* bm;
	int totlen, len, verlen;
	totlen = 0;
	stream_get_mark(s, bm);
	
	if((len = krb_decode_sequence_tag(s, &verlen)) == 0)
		goto err;

	totlen += len;
	
	/* enctype[0] */
	if((len = krb_decode_int(s, 0, (uint32*)&(key->enctype))) == 0)
		goto err;

	totlen += len;
	verlen -= len;

	/* keyvalue[1] */
	if((verlen <= 0) || ((len = krb_decode_octet_string(s, 1, (uint8**)&(key->skey.data), &(key->skey.length))) == 0))
		goto err;

	totlen += len;

	if(verlen != len)
		goto err;

	return totlen;

	err:
		stream_set_mark(s, bm);
		return 0;
}

int krb_decode_encrypted_data(STREAM* s, KrbENCData* enc_data)
{
	uint8* bm;
	int totlen, len, verlen;
	totlen = 0;
	stream_get_mark(s, bm);
	
	if((len = krb_decode_sequence_tag(s, &verlen)) == 0)
		goto err;

	totlen += len;
	
	/* enctype[0] */
	if(((len = krb_decode_int(s, 0, (uint32*)&(enc_data->enctype))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* kvno[1]  OPTIONAL */
	if((verlen <= 0))
		goto err;
	len = krb_decode_int(s, 1, (uint32*)&(enc_data->kvno));
	totlen += len;
	verlen -= len;

	/* data[2] */
	if((verlen <= 0) || ((len = krb_decode_octet_string(s, 2, (uint8**)&(enc_data->encblob.data), &(enc_data->encblob.length))) == 0))
		goto err;

	totlen += len;

	if(verlen != len)
		goto err;

	return totlen;

	err:
		stream_set_mark(s, bm);
		return 0;
}

int krb_decode_ticket(STREAM* s, uint8 tag, Ticket* ticket)
{
	uint8* bm;
	int totlen, len, tmp, verlen;
	totlen = 0;
	stream_get_mark(s, bm);

	/* Contextual tag */
	if((len = krb_decode_contextual_tag(s, tag, &verlen)) == 0)
		goto err;

	totlen += len;

	/* Application Tag 1 */
	if(((len = krb_decode_application_tag(s, 1, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -=len;

	if((verlen <= 0) || ((len = krb_decode_sequence_tag(s, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	/* tkt vno[0] */
	if((verlen <= 0) || ((len = krb_decode_int(s, 0, (uint32*)&(ticket->tktvno))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* realm[1] */
	if((verlen <= 0) || ((len = krb_decode_string(s, 1, &(ticket->realm))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* sname[2] */
	if((verlen <= 0) || ((len = krb_decode_sname(s, 2, &(ticket->sname))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* Encrypted Data[3] */
	if((verlen <= 0) || ((len = krb_decode_contextual_tag(s, 3, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	if((verlen <= 0) || ((len = krb_decode_encrypted_data(s, &(ticket->enc_part))) == 0))
		goto err;

	totlen += len;

	if(verlen != len)
		goto err;
	
	return totlen;

	err:
		stream_set_mark(s, bm);
		krb_free_ticket(ticket);
		return 0;
}

int krb_decode_kdc_rep(STREAM* s, KrbKDCREP* kdc_rep, sint32 maxlen)
{
	uint8* bm;
	int totlen, len, tmp, verlen;
	totlen = 0;
	stream_get_mark(s, bm);

	/* sequence tag */
	if(((len = krb_decode_sequence_tag(s, &verlen)) == 0) || (verlen != (maxlen - len)))
		goto err;
	
	totlen += len;

	/* version no[0] */
	if(((len = krb_decode_int(s, 0, (uint32*)&(kdc_rep->pvno))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* msg type[1] */
	if((verlen <= 0) || ((len = krb_decode_int(s, 1, (uint32*)&(kdc_rep->type))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* PA-DATA[2] OPTIONAL */
	if(verlen <= 0)
		goto err;
	len = krb_decode_contextual_tag(s, 2, &tmp);
	totlen += len + tmp;
	verlen -= (len + tmp);
	stream_seek(s, tmp);

	/* crealm[3] */
	if((verlen <= 0) || ((len = krb_decode_string(s, 3, &(kdc_rep->realm))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* cname[4] */
	if((verlen <= 0) || ((len = krb_decode_cname(s, 4, &(kdc_rep->cname))) == 0))
		goto err;

	totlen += len;
	verlen -= len;
	
	/* ticket[5] */
	if((verlen <= 0) || ((len = krb_decode_ticket(s, 5, &(kdc_rep->etgt))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* enc_part[6] */
	if((verlen <= 0) || ((len = krb_decode_contextual_tag(s, 6, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	totlen += len;
	verlen -= len;

	if((verlen <= 0) || ((len = krb_decode_encrypted_data(s, &(kdc_rep->enc_part))) == 0))
		goto err;

	totlen += len;

	if(verlen != len)
		goto err;

	return totlen;

	err:
		krb_free_kdcrep(kdc_rep);
		return 0;
}

int krb_decode_krb_error(STREAM* s, KrbERROR* krb_err, sint32 maxlen)
{
	uint8* bm;
	int totlen, len, verlen;
	int i, tag;
	totlen = tag = 0;
	stream_get_mark(s, bm);

	/* sequence tag */
	if(((len = krb_decode_sequence_tag(s, &verlen)) == 0) || (verlen != (maxlen - len)))
		goto err;
	
	totlen += len;

	/* version no[0] */
	if(((len = krb_decode_int(s, tag++, (uint32*)&(krb_err->pvno))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* msg type[1] */
	if((verlen <= 0) || ((len = krb_decode_int(s, tag++, (uint32*)&(krb_err->type))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* skip tag 2 3 */
	for(i = 2;i < 4;i++)
	{
		if(verlen <= 0)
			goto err;

		len = krb_skip_contextual_tag(s, tag++);
		verlen -= len;
		totlen += len;
	}

	/* stime[4] */
	if((verlen <= 0) || ((len = krb_decode_time(s, tag++, &(krb_err->stime))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* skip tag 5 */
	if(verlen <= 0)
		goto err;
	len = krb_skip_contextual_tag(s, tag++);
	
	totlen += len;
	verlen -= len;

	/* error code[6] */
	if((verlen <= 0) || ((len = krb_decode_int(s, tag++, (uint32*)&(krb_err->errcode))) == 0))
		goto err;

	totlen += len;
	verlen -= len;
 
	/* skip tag 7, 8, 9, 10, 11 */
	for(i = 7;i < 12;i++)
	{
		if(verlen <= 0)
			goto err;
		else if(verlen == 0)
			return totlen;

		len = krb_skip_contextual_tag(s, tag++);
		verlen -= len;
		totlen += len;
	}

	/* edata (tag 12) */
	if((verlen < 0) || ((len = krb_decode_octet_string(s, tag++, (uint8**)&(krb_err->edata.data), &(krb_err->edata.length))) == 0))
		goto err;

	totlen += len;

	if(verlen != len)
		goto err;
	
	return totlen;

	err:
		krb_free_krb_error(krb_err);
		return 0;
}

ENCKDCREPPart* krb_decode_enc_reppart(rdpBlob* msg, uint8 apptag)
{
	int len, tmp, verlen;
	STREAM* s;
	char* timestr;
	ENCKDCREPPart* reppart;

	reppart = xnew(ENCKDCREPPart);
	s = stream_new(0);
	stream_attach(s, ((uint8*) msg->data) + 24, msg->length);
	verlen = msg->length - 24;

	/* application tag */
	if((verlen <= 0) || ((len = krb_decode_application_tag(s, apptag, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	verlen -= len;
	
	/* sequence tag */
	if((verlen <= 0) || ((len = krb_decode_sequence_tag(s, &tmp)) == 0) || (tmp != (verlen - len)))
		goto err;

	verlen -= len;

	/* skey[0] */
	if((verlen <= 0) || ((len = krb_decode_contextual_tag(s, 0, &tmp)) == 0))
		goto err;

	verlen -= len;

	if((verlen <= 0) || ((len = krb_decode_enckey(s, &(reppart->key))) == 0))
		goto err;

	verlen -= len;

	/* skip tag 1 (last req) */
	if(verlen <= 0)
		goto err;
	len = krb_skip_contextual_tag(s, 1);

	verlen -= len;

	/* nonce[2] */

	if((verlen <= 0) || ((len = krb_decode_int(s, 2, (uint32*)&(reppart->nonce))) == 0))
		goto err;
	
	verlen -= len;
	
	/* skip tag 3 (key expiration) */
	if(verlen <= 0)
		goto err;
	len = krb_skip_contextual_tag(s, 3);

	verlen -= len;

	/* TicketFlags[4] */
	if((verlen <= 0) || ((len = krb_decode_flags(s, 4, (uint32*)&(reppart->flags))) == 0))
		goto err;
	
	verlen -= len;

	/* authtime[5] */
	if((verlen <= 0) || ((len = krb_decode_time(s, 5, &timestr)) == 0))
		goto err;
	
	reppart->authtime = get_local_time(timestr);
	verlen -= len;
	xfree(timestr);

	/* skip tag 6 (start time) */
	if(verlen <= 0)
		goto err;
	len = krb_skip_contextual_tag(s, 6);

	verlen -= len;

	/* endtime[7] */
	if((verlen <= 0) || ((len = krb_decode_time(s, 7, &timestr)) == 0))
		goto err;

	reppart->endtime = get_local_time(timestr);
	verlen -= len;
	xfree(timestr);

	/* skip tag 8 (renew till) */
	if(verlen <= 0)
		goto err;
	len = krb_skip_contextual_tag(s, 8);

	verlen -= len;
	
	/* realm[9] */
	if((verlen <= 0) || ((len = krb_decode_string(s, 9, &(reppart->realm))) == 0))
		goto err;

	verlen -= len;

	/* sname[10]  */
	if((verlen <= 0) || ((len = krb_decode_sname(s, 10, &(reppart->sname))) == 0))
		goto err;

	verlen -= len;

	/* skip tag 11, 12 (tag 12 is supported enctypes) */
	if(verlen <= 0)
		goto err;

	len = krb_skip_contextual_tag(s, 11);
	len += krb_skip_contextual_tag(s, 12);

	verlen -= len;

	if(verlen != 0)
		goto err;
	
	stream_detach(s);
	return reppart;
	err:
		krb_free_reppart(reppart);
		xfree(reppart);
		return NULL;
}

int krb_decode_tgtrep(STREAM* s, KrbTGTREP* krb_tgtrep)
{
	uint8* bm;
	int totlen, len, verlen;
	totlen = 0;
	stream_get_mark(s, bm);

	/* sequence tag */
	if(((len = krb_decode_sequence_tag(s, &verlen)) == 0))
		goto err;
	
	totlen += len;

	/* version no[0] */
	if(((len = krb_decode_int(s, 0, (uint32*)&(krb_tgtrep->pvno))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* msg type[1] */
	if((verlen <= 0) || ((len = krb_decode_int(s, 1, (uint32*)&(krb_tgtrep->type))) == 0))
		goto err;

	totlen += len;
	verlen -= len;

	/* ticket[2] */
	if((verlen <= 0) || ((len = krb_decode_ticket(s, 2, &(krb_tgtrep->ticket))) == 0))
		goto err;

	totlen += len;

	if(verlen != len)
		goto err;

	return totlen;

	err:
		stream_set_mark(s, bm);
		return 0;
}

