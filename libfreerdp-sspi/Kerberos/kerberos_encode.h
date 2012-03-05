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

#include <freerdp/crypto/der.h>

#include "kerberos.h"

#ifndef FREERDP_SSPI_KERBEROS_ENCODE_H
#define FREERDP_SSPI_KERBEROS_ENCODE_H

int krb_encode_sequence_tag(STREAM* s, uint32 len);
int krb_encode_contextual_tag(STREAM* s, uint8 tag, uint32 len);
int krb_encode_application_tag(STREAM* s, uint8 tag, uint32 len);
int krb_encode_recordmark(STREAM* s, uint32 len);
int krb_encode_cname(STREAM* s, uint8 tag, char* cname);
int krb_encode_sname(STREAM* s, uint8 tag, char* sname);
int krb_encode_uint8(STREAM* s, uint8 tag, uint8 val);
int krb_encode_integer(STREAM* s, uint8 tag, int val);
int krb_encode_options(STREAM* s, uint8 tag, uint32 options);
int krb_encode_string(STREAM* s, uint8 tag, char* str);
int krb_encode_time(STREAM* s, uint8 tag, char* strtime);
int krb_encode_octet_string(STREAM* s, uint8* string, uint32 len);
int krb_encode_padata(STREAM* s, PAData** pa_data);
int krb_encode_encrypted_data(STREAM* s, KrbENCData* enc_data);
int krb_encode_checksum(STREAM* s, rdpBlob* cksum, int cktype);
int krb_encode_authenticator(STREAM* s, Authenticator *krb_auth);
int krb_encode_ticket(STREAM* s, uint8 tag, Ticket* ticket);
int krb_encode_req_body(STREAM* s, KDCReqBody* req_body, int msgtype);
int krb_encode_apreq(STREAM* s, KrbAPREQ* krb_apreq);
int krb_encode_tgtreq(STREAM* s, KrbTGTREQ* krb_tgtreq);

#endif /* FREERDP_SSPI_KERBEROS_ENCODE_H */
