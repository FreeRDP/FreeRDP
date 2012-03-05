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

#include <freerdp/crypto/der.h>

#include "kerberos.h"

#ifndef FREERDP_SSPI_KERBEROS_DECODE_H
#define FREERDP_SSPI_KERBEROS_DECODE_H

int krb_decode_application_tag(STREAM* s, uint8 tag, int *length);
int krb_decode_sequence_tag(STREAM* s, int *length);
int krb_skip_contextual_tag(STREAM* s, uint8 tag);
int krb_decode_contextual_tag(STREAM* s, uint8 tag, int *length);
int krb_decode_integer(STREAM* s, uint32* value);
int krb_decode_time(STREAM* s, uint8 tag, char** str);
int krb_decode_int(STREAM* s, uint8 tag, uint32* result);
int krb_decode_flags(STREAM* s, uint8 tag, uint32* flags);
int krb_decode_string(STREAM* s, uint8 tag, char** str);
int krb_decode_octet_string(STREAM* s, uint8 tag, uint8** data, int* length);
int krb_decode_cname(STREAM* s, uint8 tag, char** str);
int krb_decode_sname(STREAM* s, uint8 tag, char** str);
int krb_decode_enckey(STREAM* s, KrbENCKey* key);
int krb_decode_encrypted_data(STREAM* s, KrbENCData* enc_data);
int krb_decode_ticket(STREAM* s, uint8 tag, Ticket* ticket);
int krb_decode_kdc_rep(STREAM* s, KrbKDCREP* kdc_rep, sint32 maxlen);
int krb_decode_krb_error(STREAM* s, KrbERROR* krb_err, sint32 maxlen);
ENCKDCREPPart* krb_decode_enc_reppart(rdpBlob* msg, uint8 apptag);
int krb_decode_tgtrep(STREAM* s, KrbTGTREP* krb_tgtrep);

#endif /* FREERDP_SSPI_KERBEROS_DECODE_H */
