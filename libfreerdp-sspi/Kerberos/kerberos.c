/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Kerberos Auth Protocol
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#include <netdb.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

#include "kerberos.h"
#include "kerberos_crypto.h"
#include "kerberos_encode.h"
#include "kerberos_decode.h"

#include <freerdp/sspi/sspi.h>

#include <freerdp/utils/tcp.h>
#include <freerdp/utils/time.h>
#include <freerdp/utils/blob.h>
#include <freerdp/utils/print.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/unicode.h>

#include "../sspi.h"

char* KRB_PACKAGE_NAME = "Kerberos";

boolean tcp_is_ipaddr(const char* hostname)
{
	char* s;
	uint8 dotcnt, digcnt;
	char val;
	s = (char*)hostname;
	dotcnt = digcnt = 0;
	while((s - hostname) < 15 && (*s != '\0'))
	{
		val = *s;
		if(!isdigit(val) && (val != '.'))
			return false;
		if(val == '.')
			if((digcnt == 0) && (dotcnt == 0))
				return false;
			else
			{
				if(dotcnt >= 3)
					return false;
				dotcnt++;
				digcnt = 0;
			}
		else
			if(digcnt >= 3)
				return false;
			else
				digcnt++;
		s++;
	}
	if((*s != '\0') || (dotcnt < 3) || (digcnt == 0))
		return false;
	return true;
}

char* get_utc_time(time_t t)
{
	char* str;
	struct tm* utc;

	if (t == 0)
		t = time(NULL);

	utc = gmtime(&t);
	str = (char*) xzalloc(16);
	strftime(str, 16, "%Y%m%d%H%M%SZ", utc);

	return str;
}

time_t get_local_time(char* str)
{
	return freerdp_get_unix_time_from_generalized_time(str);
}

time_t get_clock_skew(char* str)
{
	time_t ctime;
	ctime = time(NULL);
	return (get_local_time(str) - ctime);
}

char* get_dns_queryname(char *host, char* protocol)
{
	char* qname;
	uint32 buflen;
	buflen = 0;
	
	if(protocol)
		buflen = strlen(protocol);
	
	buflen += strlen(host)+strlen(SERVICE)+1;
	qname = (char*)xzalloc(buflen);
	strcat(qname, SERVICE);

	if(protocol)
		strcat(qname, protocol);
	
	strcat(qname, host);
	
	return qname;
}

int get_krb_realm(rdpSettings* settings)
{
#ifdef WITH_RESOLV
	char* s;
	char* queryname;
	int reslen;
	unsigned char response[2048];
	char ns_realm[2048];
	ns_msg handle;
	ns_rr rr;
	s = settings->hostname;
	if (settings->kerberos_realm)
		return 0;
	do
	{
		queryname = get_dns_queryname(s, NULL);

		if ((reslen = res_query(queryname, ns_c_in, ns_t_txt, response, sizeof(response))))
		{
			ns_initparse(response, reslen, &handle);
			ns_parserr(&handle, ns_s_an, 0, &rr);
			if (ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), ns_rr_rdata(rr), (char*)ns_realm, sizeof(ns_realm)) < 0)
				goto end;
			else
				settings->kerberos_realm = xstrdup(ns_realm);
			xfree(queryname);
			return 0;
		}

		end:
			xfree(queryname);
			for (;*s != '.' && *s != '\0';s++);
			if (*s != '\0')
				s++;
	}
	while (*s != '\0');
	s = settings->hostname;
	for (;*s != '.';s++);
	s++;
	settings->kerberos_realm = xstrdup(s);
#endif

	return 0;
}

KDCENTRY* krb_locate_kdc(rdpSettings* settings)
{
#ifdef WITH_RESOLV
	char *qname;
	int reslen, i;
	unsigned char response[4096];
	char kdc_host[4096];
	unsigned char* temp;
	KDCENTRY* head;
	KDCENTRY* kdcentry;
	KDCENTRY* srvans;
	ns_msg handle;
	ns_rr rr;
	head = kdcentry =  NULL;
	if (get_krb_realm(settings))
		return NULL;
	if (settings->kerberos_kdc)
	{
		srvans = xnew(KDCENTRY);
		srvans->kdchost = xstrdup(settings->kerberos_kdc);
		srvans->port = 88;
		return srvans;
	}
	qname = get_dns_queryname(settings->kerberos_realm, "_tcp.");

	if ((reslen = res_query(qname, ns_c_in, ns_t_srv, response, sizeof(response))) < 0)
		return NULL;

	ns_initparse(response, reslen, &handle);

	for (i = 0;i < ns_msg_count(handle, ns_s_an);i++)
	{
		ns_parserr(&handle, ns_s_an, i, &rr);
		srvans = xnew(KDCENTRY);
		temp = (unsigned char*)ns_rr_rdata(rr);
		GETUINT16(temp, &(srvans->priority));
		GETUINT16(temp, &(srvans->weight));
		GETUINT16(temp, &(srvans->port));
		if (ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), temp, (char*)kdc_host, sizeof(kdc_host)) < 0)
			srvans->kdchost = xstrdup((const char*)temp);
		else
			srvans->kdchost = xstrdup(kdc_host);
		if (head == NULL || head->priority > srvans->priority)
		{
			srvans->next = head;
			head = srvans;
		}
		else
		{
			for (kdcentry = head;kdcentry->next != NULL && kdcentry->next->priority < srvans->priority;kdcentry = kdcentry->next);
			srvans->next = kdcentry->next;
			kdcentry->next = srvans;
		}
	}

	return head;
#else
	return NULL;
#endif
}

int krb_tcp_connect(KRB_CONTEXT* krb_ctx, KDCENTRY* entry)
{
	int sockfd;

	sockfd = freerdp_tcp_connect(entry->kdchost, entry->port);

	if (sockfd == -1)
		return -1;

	krb_ctx->krbhost = xstrdup(entry->kdchost);
	krb_ctx->krbport = entry->port;
	krb_ctx->ksockfd = sockfd;

	return 0;
}

int krb_tcp_recv(KRB_CONTEXT* krb_ctx, uint8* data, uint32 length)
{
	return freerdp_tcp_read(krb_ctx->ksockfd, data, length);
}

int krb_tcp_send(KRB_CONTEXT* krb_ctx, uint8* data, uint32 length)
{
	return freerdp_tcp_write(krb_ctx->ksockfd, data, length);
}

KRB_CONTEXT* kerberos_ContextNew()
{
	KRB_CONTEXT* context;

	context = xnew(KRB_CONTEXT);

	if (context != NULL)
	{
		context->enctype = ETYPE_RC4_HMAC; //choose enc type
		context->state = KRB_STATE_INITIAL;
		context->uniconv = freerdp_uniconv_new();
	}

	return context;
}

void kerberos_ContextFree(KRB_CONTEXT* krb_ctx)
{
	if (krb_ctx != NULL)
	{
		xfree(krb_ctx->krbhost);
		xfree(krb_ctx->cname);
		xfree(krb_ctx->realm);
		freerdp_blob_free(&(krb_ctx->passwd));

		if(krb_ctx->askey != NULL)
		{
			freerdp_blob_free(&(krb_ctx->askey->skey));
			xfree(krb_ctx->askey);
		}
		
		if(krb_ctx->tgskey != NULL)
		{
			freerdp_blob_free(&(krb_ctx->tgskey->skey));
			xfree(krb_ctx->tgskey);
		}

		krb_free_ticket(&(krb_ctx->asticket));
		krb_free_ticket(&(krb_ctx->tgsticket));
		krb_ctx->state = KRB_STATE_FINAL;
	}
}

SECURITY_STATUS SEC_ENTRY kerberos_AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, TimeStamp* ptsExpiry)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, TimeStamp* ptsExpiry)
{
	CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY* identity;

	if (fCredentialUse == SECPKG_CRED_OUTBOUND)
	{
		credentials = sspi_CredentialsNew();
		identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;

		memcpy(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) KRB_PACKAGE_NAME);

		return SEC_E_OK;
	}

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_FreeCredentialsHandle(PCredHandle phCredential)
{
	CREDENTIALS* credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_QueryCredentialsAttributesW(PCredHandle phCredential, uint32 ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_QueryCredentialsAttributesA(PCredHandle phCredential, uint32 ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		CREDENTIALS* credentials;
		//SecPkgCredentials_Names* credential_names = (SecPkgCredentials_Names*) pBuffer;

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		//if (credentials->identity.Flags == SEC_WINNT_AUTH_IDENTITY_ANSI)
		//	credential_names->sUserName = xstrdup((char*) credentials->identity.User);

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

void krb_SetContextIdentity(KRB_CONTEXT* context, SEC_WINNT_AUTH_IDENTITY* identity)
{
	size_t size;
	/* TEMPORARY workaround for utf8 TODO: UTF16 to utf8 */
	identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
	context->identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	if (identity->Flags == SEC_WINNT_AUTH_IDENTITY_ANSI)
	{
		context->identity.User = (uint16*) freerdp_uniconv_out(context->uniconv, (char*) identity->User, &size);
		context->identity.UserLength = (uint32) size;

		if (identity->DomainLength > 0)
		{
			context->identity.Domain = (uint16*) freerdp_uniconv_out(context->uniconv, (char*) identity->Domain, &size);
			context->identity.DomainLength = (uint32) size;
		}
		else
		{
			context->identity.Domain = (uint16*) NULL;
			context->identity.DomainLength = 0;
		}

		context->identity.Password = (uint16*) freerdp_uniconv_out(context->uniconv, (char*) identity->Password, &size);
		context->identity.PasswordLength = (uint32) size;
	}
	else
	{
		context->identity.User = (uint16*) xzalloc(identity->UserLength + 1);
		memcpy(context->identity.User, identity->User, identity->UserLength);

		if (identity->DomainLength > 0)
		{
			context->identity.Domain = (uint16*) xmalloc(identity->DomainLength);
			memcpy(context->identity.Domain, identity->Domain, identity->DomainLength);
		}
		else
		{
			context->identity.Domain = (uint16*) NULL;
			context->identity.DomainLength = 0;
		}

		context->identity.Password = (uint16*) xzalloc(identity->PasswordLength + 1);
		memcpy(context->identity.Password, identity->Password, identity->PasswordLength);
		identity->Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
	}
}

SECURITY_STATUS SEC_ENTRY kerberos_InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_WCHAR* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		PSecBufferDesc pInput, uint32 Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, uint32* pfContextAttr, PTimeStamp ptsExpiry)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_CHAR* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		PSecBufferDesc pInput, uint32 Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, uint32* pfContextAttr, PTimeStamp ptsExpiry)
{
	KRB_CONTEXT* krb_ctx;
	//SECURITY_STATUS status;
	//CREDENTIALS* credentials;
	//PSecBuffer input_SecBuffer;
	//PSecBuffer output_SecBuffer;
	int errcode;
	errcode = 0;

	krb_ctx = (KRB_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!krb_ctx)
		return SEC_E_INVALID_HANDLE;
	else
	{
		while(true)
		{
			switch(krb_ctx->state)
			{
				case KRB_PACKET_ERROR:
					kerberos_ContextFree(krb_ctx);
					return SEC_E_INVALID_HANDLE;
					break;
				case KRB_STATE_INITIAL:
				case KRB_ASREP_ERR:
					krb_asreq_send(krb_ctx, errcode);
					break;
				case KRB_ASREQ_OK:
					errcode = krb_asrep_recv(krb_ctx);
					break;
				case KRB_ASREP_OK:
					krb_tgsreq_send(krb_ctx, 0);
					break;
				case KRB_TGSREQ_OK:
					krb_tgsrep_recv(krb_ctx);
					break;
				case KRB_TGSREP_OK:
					return SEC_I_COMPLETE_AND_CONTINUE;
					break;
				default:
					printf("not implemented\n");
					return -1;
			}
		}
	}
}

PCtxtHandle krbctx_client_init(rdpSettings* settings, SEC_WINNT_AUTH_IDENTITY* identity)
{
	SECURITY_STATUS status;
	KDCENTRY* kdclist;
	KDCENTRY* entry;
	KRB_CONTEXT* krb_ctx;
	uint32 fContextReq;
	uint32 pfContextAttr;
	TimeStamp expiration;

	if (tcp_is_ipaddr(settings->hostname))
		return NULL;

	kdclist = krb_locate_kdc(settings);

	/* start the state machine with initialized to zero */
	krb_ctx = kerberos_ContextNew();

	for (entry = kdclist;entry != NULL; entry = entry->next)
	{
		if(!krb_tcp_connect(krb_ctx, entry))
			break;
	}

	if (entry == NULL)
	{
		xfree(krb_ctx);
		return NULL;
	}

	krb_SetContextIdentity(krb_ctx, identity);
	krb_ctx->realm = xstrtoup(settings->kerberos_realm);
	krb_ctx->cname = xstrdup((char*)krb_ctx->identity.User);
	krb_ctx->settings = settings;
	krb_ctx->passwd.data = freerdp_uniconv_out(krb_ctx->uniconv, (char*) krb_ctx->identity.Password, (size_t*) &(krb_ctx->passwd.length));

	fContextReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT |
			ISC_REQ_CONFIDENTIALITY | ISC_REQ_DELEGATE;

	sspi_SecureHandleSetLowerPointer(&krb_ctx->context, krb_ctx);
	sspi_SecureHandleSetUpperPointer(&krb_ctx->context, (void*) KRB_PACKAGE_NAME);

	status = kerberos_InitializeSecurityContextA(NULL, &krb_ctx->context, NULL,
		fContextReq, 0, SECURITY_NATIVE_DREP, NULL, 0, &krb_ctx->context, NULL, &pfContextAttr, &expiration);

	if(status == SEC_E_INVALID_HANDLE)
	{
		printf("failed to init kerberos\n");
		return NULL;
	}
	else if(status == SEC_I_COMPLETE_AND_CONTINUE)
	{
		printf("successfully obtained ticket for TERMSRV\n");
		return &krb_ctx->context;
	}
	else if(status == -1)
	{
		printf("deadend \n ");
		return NULL;
	}
	else
	{
		printf("shit got wrong in state machine\n");
		return NULL;
	}
}

void krb_asreq_send(KRB_CONTEXT* krb_ctx, uint8 errcode)
{
	KrbASREQ* krb_asreq;
	PAData** pa_data;
	KrbENCData enc_data;
	KrbENCKey* enckey;
	STREAM* s;
	STREAM* paenc;
	rdpBlob msg;
	rdpBlob* encmsg;
	uint32 curlen, totlen, pai;
	uint8 *bm;
	bm = NULL;
	totlen = pai = 0;
	krb_asreq = krb_asreq_new(krb_ctx, errcode);
	pa_data = krb_asreq->padata;
	enckey = NULL;
	s = stream_new(2048);
	paenc = stream_new(100);
	
	//Begin write asn1 data reversely into stream
	stream_seek(s, 1024);
	stream_seek(paenc, 99);
	
	/* KDC-REQ-BODY (TAG 4) */
	totlen += krb_encode_req_body(s, &(krb_asreq->req_body), krb_asreq->type);
	totlen += krb_encode_contextual_tag(s, 4, totlen);

	/* padata = PA-ENC-TIMESTAMP */
	if(errcode != 0)
	{
		freerdp_blob_alloc(&msg, 21);
		memcpy(msg.data, "\x30\x13\xa0\x11\x18\x0f", 6); // PA-ENC-TS-ENC without optional pausec
		memcpy(((uint8*) msg.data) + 6, krb_asreq->req_body.from, 15);
		enckey = string2key(&(krb_ctx->passwd), krb_ctx->enctype);
		encmsg = crypto_kdcmsg_encrypt(&msg, enckey, 1); //RFC4757 section 3 for msgtype (T=1)
		enc_data.enctype = enckey->enctype;
		enc_data.kvno = -1;
		enc_data.encblob.data = encmsg->data;
		enc_data.encblob.length = encmsg->length;
		curlen = krb_encode_encrypted_data(paenc, &enc_data);
		freerdp_blob_free(&msg);
		msg.data = paenc->p;
		msg.length = curlen;

		/* padata = PA-ENC-TIMESTAMP */
		(*(pa_data + pai))->type = 2;
		(*(pa_data + pai))->value = msg;
		pai++;
		freerdp_blob_free(encmsg);
		xfree(encmsg);
	}
	
	freerdp_blob_alloc(&msg, 7);
	memcpy(msg.data, "\x30\x05\xa0\03\x01\x01", 6); // asn1 data
	*((uint8*)msg.data + 6) = krb_asreq->pa_pac_request;

	/* padata = PA-PAC-REQUEST */
	(*(pa_data + pai))->type = 128;
	(*(pa_data + pai))->value = msg;

	*(pa_data + ++pai) = NULL;

	/* padata (TAG 3) */
	curlen = krb_encode_padata(s, pa_data);
	totlen += curlen + krb_encode_contextual_tag(s, 3, curlen);

	/* MSGTYPE = AS-REQ (TAG 2) */
	totlen += krb_encode_uint8(s, 2, krb_asreq->type);

	/* KERBEROS PROTOCOL VERSION NO (TAG 1)*/
	totlen += krb_encode_uint8(s, 1, krb_asreq->pvno);

	totlen += krb_encode_sequence_tag(s, totlen);
	totlen += krb_encode_application_tag(s, krb_asreq->type, totlen);
	totlen += krb_encode_recordmark(s, totlen);

	/* READY SEND */
	krb_tcp_send(krb_ctx, s->p, totlen);

	/* save stuff */
	krb_ctx->askey = enckey;
	krb_ctx->nonce = krb_asreq->req_body.nonce;
	xfree(krb_ctx->sname);
	krb_ctx->sname = xstrdup(krb_asreq->req_body.sname);
	krb_ctx->ctime = get_local_time(krb_asreq->req_body.from);
	krb_ctx->state = KRB_ASREQ_OK;

	/* clean up */
	freerdp_blob_free(&msg);
	stream_free(s);
	stream_free(paenc);
	krb_free_asreq(krb_asreq);
	xfree(krb_asreq);
}

int krb_asrep_recv(KRB_CONTEXT* krb_ctx)
{
	int totlen, tmp, len;
	int errcode;
	STREAM* s;
	KrbERROR* krb_err;
	KrbASREP* krb_asrep;
	KrbKDCREP* kdc_rep;
	errcode = -1;
	s = stream_new(2048);
	krb_tcp_recv(krb_ctx, s->data, s->size);
	
	stream_read_uint32_be(s, totlen);
	if(totlen >= 2044)  // MALFORMED PACKET
		goto finish;
	
	if(((len = krb_decode_application_tag(s, KRB_TAG_ASREP, &tmp)) == 0) || (tmp != (totlen - len)))  //NOT AN AS-REP
	{
		if(((len = krb_decode_application_tag(s, KRB_TAG_ERROR, &tmp)) == 0) || (tmp != (totlen - len)))  // NOT AN KRB-ERROR
		{
			krb_ctx->state = KRB_PACKET_ERROR;
			goto finish;
		}
		else
		{
			totlen -= len;
			krb_err = xnew(KrbERROR);

			if((totlen <= 0) || ((len = krb_decode_krb_error(s, krb_err, totlen)) == 0))
			{
				krb_ctx->state = KRB_PACKET_ERROR;
				xfree(krb_err);
				goto finish;
			}

			/* ERROR CODE */
			errcode = krb_err->errcode; 
			if(errcode == KRB_AP_ERR_SKEW)
			{
				krb_ctx->state = KRB_ASREP_ERR;
				goto errclean;
			}
			else if(errcode == KDC_ERR_PREAUTH_REQ)
			{
				/* should parse PA-ENC-TYPE-INFO2 */
				krb_ctx->state = KRB_ASREP_ERR;
				goto errclean;
			}
			else if(errcode == KDC_ERR_C_PRINCIPAL_UNKNOWN)
			{
				printf("KDC_ERR_C_PRINCIPAL_UNKNOWN\n");
				krb_ctx->state = KRB_ASREP_ERR;
				goto errclean;
			}
			else
			{
				krb_ctx->state = KRB_PACKET_ERROR;
				goto errclean;
			}
			errclean:
				krb_free_krb_error(krb_err);
				xfree(krb_err);
				goto finish;
		}
	}
	else	/* AS-REP process */
	{
		totlen -= len;

		krb_asrep = xnew(KrbASREP);
		if(krb_decode_kdc_rep(s, &(krb_asrep->kdc_rep), totlen) == 0)
		{
			krb_ctx->state = KRB_PACKET_ERROR;
			goto finish;
		}
		kdc_rep = &(krb_asrep->kdc_rep);

		if(krb_verify_kdcrep(krb_ctx, kdc_rep, KRB_TAG_ASREP) == 0)
			krb_ctx->state = KRB_ASREP_OK;
		else
			krb_ctx->state = KRB_PACKET_ERROR;

		/* clean up */
		krb_free_asrep(krb_asrep);
		xfree(krb_asrep);
		goto finish;
	}
	finish:
		stream_free(s);
		return errcode;
}

void krb_tgsreq_send(KRB_CONTEXT* krb_ctx, uint8 errcode)
{
	KrbTGSREQ* krb_tgsreq;
	KrbAPREQ* krb_apreq;
	PAData** pa_data;
	Authenticator* krb_auth;
	STREAM* s;
	STREAM* sapreq;
	rdpBlob msg;
	uint32 curlen, totlen, tmp;
	uint8 *bm;
	bm = NULL;
	totlen = tmp = 0;
	krb_tgsreq = krb_tgsreq_new(krb_ctx, errcode);
	krb_auth = xnew(Authenticator);
	pa_data = krb_tgsreq->padata;
	s = stream_new(4096);
	sapreq = stream_new(2048);
	
	//Begin write asn1 data reversely into stream
	stream_seek(s, 4095);
	stream_seek(sapreq, 2047);
	
	/* KDC-REQ-BODY (TAG 4) */
	totlen += krb_encode_req_body(s, &(krb_tgsreq->req_body), krb_tgsreq->type);
	stream_get_mark(s, bm);
	tmp = totlen;
	totlen += krb_encode_contextual_tag(s, 4, totlen);

	msg.data = bm;
	msg.length = tmp;

	/* Authenticator */
	krb_auth->avno = KRB_VERSION;
	krb_auth->cname = krb_ctx->cname;
	krb_auth->crealm = krb_ctx->realm;
	krb_auth->cksumtype = get_cksum_type(krb_ctx->enctype);
	krb_auth->cksum = crypto_kdcmsg_cksum(&msg, krb_ctx->askey, 6); //RFC4757 section 3 for msgtype (T=6)
	krb_auth->ctime = krb_tgsreq->req_body.from;
	krb_auth->cusec = 0;
	crypto_nonce((uint8*)&(krb_auth->seqno), 4);

	/* PA-TGS-REQ */
	krb_apreq = krb_apreq_new(krb_ctx, &(krb_ctx->asticket), krb_auth);
	curlen = krb_encode_apreq(sapreq, krb_apreq);
	msg.data = sapreq->p;
	msg.length = curlen;
	(*pa_data)->type = 1;
	(*pa_data)->value = msg;

	/* PA-DATA (TAG 3) */
	curlen = krb_encode_padata(s, pa_data);
	totlen += curlen + krb_encode_contextual_tag(s, 3, curlen);

	/* MSGTYPE (TAG 2) */
	totlen += krb_encode_uint8(s, 2, krb_tgsreq->type);

	/* VERSION NO (TAG 1) */
	totlen += krb_encode_uint8(s, 1, krb_tgsreq->pvno);

	totlen += krb_encode_sequence_tag(s, totlen);
	totlen += krb_encode_application_tag(s, krb_tgsreq->type, totlen);
	totlen += krb_encode_recordmark(s, totlen);

	/* READY SEND */
	krb_tcp_send(krb_ctx, s->p, totlen);
	
	/* save stuff */
	krb_ctx->nonce = krb_tgsreq->req_body.nonce;
	xfree(krb_ctx->sname);
	krb_ctx->sname = xstrdup(krb_tgsreq->req_body.sname);
	krb_ctx->ctime = get_local_time(krb_tgsreq->req_body.from);
	krb_ctx->state = KRB_TGSREQ_OK;

	/* clean up */
	freerdp_blob_free(krb_auth->cksum);
	xfree(krb_auth->cksum);
	xfree(krb_auth);
	krb_free_tgsreq(krb_tgsreq);
	xfree(krb_tgsreq);
	stream_free(sapreq);
	stream_free(s);
}

int krb_tgsrep_recv(KRB_CONTEXT* krb_ctx)
{
	int totlen, tmp, len;
	int errcode;
	STREAM* s;
	KrbTGSREP* krb_tgsrep;
	KrbKDCREP* kdc_rep;
	errcode = -1;
	s = stream_new(2048);
	krb_tcp_recv(krb_ctx, s->data, s->size);
	
	stream_read_uint32_be(s, totlen);
	if(totlen >= 2044)  // MALFORMED PACKET
		goto finish;
	
	if(((len = krb_decode_application_tag(s, KRB_TAG_TGSREP, &tmp)) == 0) || (tmp != (totlen - len)))  //NOT AN TGS-REP
	{
		krb_ctx->state = KRB_PACKET_ERROR;
		goto finish;
	}
	else	/* TGS-REP process */
	{
		totlen -= len;

		krb_tgsrep = xnew(KrbTGSREP);
		krb_ctx->tgskey = xnew(KrbENCKey);
		if(krb_decode_kdc_rep(s, &(krb_tgsrep->kdc_rep), totlen) == 0)
		{
			krb_ctx->state = KRB_PACKET_ERROR;
			goto finish;
		}
		kdc_rep = &(krb_tgsrep->kdc_rep);
	
		if(krb_verify_kdcrep(krb_ctx, kdc_rep, KRB_TAG_TGSREP) == 0)
			krb_ctx->state = KRB_TGSREP_OK;
		else
			krb_ctx->state = KRB_PACKET_ERROR;

		/* clean up */
		krb_free_tgsrep(krb_tgsrep);
		xfree(krb_tgsrep);
		goto finish;
	}
	finish:
		stream_free(s);
		return errcode;
}

int krb_verify_kdcrep(KRB_CONTEXT* krb_ctx, KrbKDCREP* kdc_rep, int msgtype)
{
	ENCKDCREPPart* reppart;
	KrbENCKey* key;
	rdpBlob* decmsg;
	uint8 tag;
	tag = 0;
	key = NULL;

	/* Verify everything */
	if((kdc_rep->pvno != KRB_VERSION) || (kdc_rep->type != msgtype) || strcasecmp(kdc_rep->cname, krb_ctx->cname) || strcasecmp(kdc_rep->realm, krb_ctx->realm))
	{
		krb_ctx->state = KRB_PACKET_ERROR;
		return -1;
	}
	/* decrypt enc-part */
	decmsg = xnew(rdpBlob);
	if(krb_ctx->askey->enctype != kdc_rep->enc_part.enctype && msgtype == KRB_TAG_ASREP)
	{
		freerdp_blob_free(&(krb_ctx->askey->skey));
		xfree(krb_ctx->askey);
		krb_ctx->askey = string2key(&(krb_ctx->passwd), kdc_rep->enc_part.enctype);
	}
	krb_ctx->askey->enctype = kdc_rep->enc_part.enctype;
	decmsg = crypto_kdcmsg_decrypt(&(kdc_rep->enc_part.encblob), krb_ctx->askey, 8); //RFC4757 section 3 for msgtype (T=8)

	if(msgtype == KRB_TAG_ASREP)
	{
		key = krb_ctx->askey;
		tag = 25;
	}
	else if (msgtype == KRB_TAG_TGSREP)
	{
		key = krb_ctx->tgskey;
		tag = 26;
	}

	/* KDC-REP-PART decode */
	if(decmsg == NULL || ((reppart = krb_decode_enc_reppart(decmsg, tag)) == NULL))
	{
		krb_ctx->state = KRB_PACKET_ERROR;
		return -1;
	}
	freerdp_blob_free(decmsg);
	xfree(decmsg);

	/* Verify KDC-REP-PART */
	if(reppart->nonce != krb_ctx->nonce || strcasecmp(reppart->realm, krb_ctx->realm) || strcasecmp(reppart->sname, krb_ctx->sname))
	{
		krb_free_reppart(reppart);
		xfree(reppart);
		krb_ctx->state = KRB_PACKET_ERROR;
		return -1;
	}

	/* save stuff */
	krb_ctx->clockskew = reppart->authtime - krb_ctx->ctime; //Used to synchronize clocks 
	krb_save_ticket(krb_ctx, kdc_rep);
	freerdp_blob_copy(&(key->skey), &(reppart->key.skey));
	key->enctype = reppart->key.enctype;
	krb_free_reppart(reppart);
	xfree(reppart);
	return 0;
}

void krb_save_ticket(KRB_CONTEXT* krb_ctx, KrbKDCREP* kdc_rep)
{
	Ticket* src;
	Ticket* dst;

	dst = NULL;
	src = &(kdc_rep->etgt);

	if (kdc_rep->type == KRB_TAG_ASREP)
		dst = &(krb_ctx->asticket);
	else if (kdc_rep->type == KRB_TAG_TGSREP)
		dst = &(krb_ctx->tgsticket);

	dst->tktvno = src->tktvno;
	dst->realm = xstrdup(src->realm);
	dst->sname = xstrdup(src->sname);
	dst->enc_part.enctype = src->enc_part.enctype;
	dst->enc_part.kvno = src->enc_part.kvno;

	freerdp_blob_copy(&(dst->enc_part.encblob), &(src->enc_part.encblob));
}

void krb_reqbody_init(KRB_CONTEXT* krb_ctx, KDCReqBody* req_body, uint8 reqtype)
{
	time_t t;

	req_body->cname = xstrdup(krb_ctx->cname);
	req_body->realm = xstrdup(krb_ctx->realm);
	
	if (reqtype == KRB_TAG_ASREQ)
	{
		req_body->kdc_options = 0x40000000 | 0x00800000 | 0x00010000 | 0x00000010;  /* forwardable , renewable, canonicalize, renewable OK */
		req_body->sname = xzalloc((strlen(req_body->realm) + 8) * sizeof(char));
		strcpy(req_body->sname, KRB_SERVER);
		strcat(req_body->sname, req_body->realm);
	}
	else if (reqtype == KRB_TAG_TGSREQ)
	{
		req_body->kdc_options = 0x40000000 | 0x00800000 | 0x00010000;  /* forwardable , renewable, canonicalize */
		req_body->sname = xzalloc((strlen(krb_ctx->settings->hostname) + 10) * sizeof(char));
		strcpy(req_body->sname, APP_SERVER);
		strcat(req_body->sname, krb_ctx->settings->hostname);
	}

	t = time(NULL);
	t += krb_ctx->clockskew; /* fix clockskew */

	req_body->from = get_utc_time((time_t)(t));
	req_body->till = get_utc_time((time_t)(t + 473040000));
	req_body->rtime = get_utc_time((time_t)(t + 473040000));
	
	crypto_nonce((uint8*) &(req_body->nonce), 4);
}

KrbASREQ* krb_asreq_new(KRB_CONTEXT* krb_ctx, uint8 errcode)
{
	KrbASREQ* krb_asreq;
	PAData** lpa_data;
	uint8 pacntmax, i;
	pacntmax = 2;

	krb_asreq = xnew(KrbASREQ);
	krb_asreq->pvno = KRB_VERSION;
	krb_asreq->type = KRB_TAG_ASREQ;
	krb_asreq->pa_pac_request = 0xff; //true
	krb_asreq->padata = (PAData**)xzalloc((pacntmax + 1) * sizeof(PAData*));
	lpa_data = krb_asreq->padata;

	for (i = 0; i < pacntmax; i++)
		*(lpa_data + i) = (PAData*)xzalloc(sizeof(PAData));
	
	krb_reqbody_init(krb_ctx, &(krb_asreq->req_body), krb_asreq->type);
	
	return krb_asreq;
}

KrbAPREQ* krb_apreq_new(KRB_CONTEXT* krb_ctx, Ticket* ticket, Authenticator* krb_auth)
{
	KrbAPREQ* krb_apreq;
	STREAM* as;
	rdpBlob msg;
	rdpBlob* encmsg;
	uint32 len;

	as = stream_new(1024);
	stream_seek(as, 1023);

	krb_apreq = xnew(KrbAPREQ);
	krb_apreq->pvno = KRB_VERSION;
	krb_apreq->type = KRB_TAG_APREQ;
	krb_apreq->ap_options = 0x00000000 | 0x00000000 | 0x00000000; /* Reserved (bit 31), Use session Key (bit 30), Mututal Required (bit 29) */
	krb_apreq->ticket = ticket;

	if (krb_auth != NULL)
	{
		len = krb_encode_authenticator(as, krb_auth);
		msg.data = as->p;
		msg.length = len;
		encmsg = crypto_kdcmsg_encrypt(&msg, krb_ctx->askey, 7); /* RFC4757 section 3 for msgtype (T=7) */
		krb_apreq->enc_auth.enctype = krb_ctx->askey->enctype;
		krb_apreq->enc_auth.kvno = -1;
		krb_apreq->enc_auth.encblob.data = encmsg->data;
		krb_apreq->enc_auth.encblob.length =  encmsg->length;
		xfree(encmsg);
	}
	
	stream_free(as);
	
	return krb_apreq;
}

KrbTGSREQ* krb_tgsreq_new(KRB_CONTEXT* krb_ctx, uint8 errcode)
{
	KrbTGSREQ* krb_tgsreq;
	uint8 pacntmax;
	pacntmax = 1;

	krb_tgsreq = xnew(KrbTGSREQ);
	krb_tgsreq->pvno = KRB_VERSION;
	krb_tgsreq->type = KRB_TAG_TGSREQ;
	krb_tgsreq->pa_pac_request = 0xFF; /* true */

	krb_tgsreq->padata = (PAData**) xzalloc((pacntmax + 1) * sizeof(PAData*));
	*(krb_tgsreq->padata) = xnew(PAData);
	*(krb_tgsreq->padata + 1) = NULL;
	
	krb_reqbody_init(krb_ctx, &(krb_tgsreq->req_body), krb_tgsreq->type);
	
	return krb_tgsreq;
}

void krb_free_ticket(Ticket* ticket)
{
	if (ticket != NULL)
	{
		xfree(ticket->realm);
		xfree(ticket->sname);
		freerdp_blob_free(&(ticket->enc_part.encblob));
		ticket->enc_part.encblob.data = NULL;
	}
}

void krb_free_padata(PAData** padata)
{
	PAData** lpa_data;
	lpa_data = padata;

	if(lpa_data == NULL)
		return;
	while(*lpa_data != NULL)
	{
		xfree(*lpa_data);
		lpa_data++;
	}
}

void krb_free_kdcrep(KrbKDCREP* kdc_rep)
{
	if(kdc_rep != NULL)
	{
		krb_free_padata(kdc_rep->padata);
		xfree(kdc_rep->cname);
		xfree(kdc_rep->realm);
		krb_free_ticket(&(kdc_rep->etgt));
		freerdp_blob_free(&(kdc_rep->enc_part.encblob));
		kdc_rep->enc_part.encblob.data = NULL;
	}
}

void krb_free_reppart(ENCKDCREPPart* reppart)
{
	if(reppart != NULL)
	{
		freerdp_blob_free(&(reppart->key.skey));
		xfree(reppart->sname);
		xfree(reppart->realm);
	}
}

void krb_free_req_body(KDCReqBody* req_body)
{
	if(req_body != NULL)
	{
		xfree(req_body->sname);
		xfree(req_body->realm);
		xfree(req_body->cname);
		xfree(req_body->from);
		xfree(req_body->till);
		xfree(req_body->rtime);
	}
}

void krb_free_asreq(KrbASREQ* krb_asreq)
{
	if(krb_asreq != NULL)
	{
		krb_free_padata(krb_asreq->padata);
		krb_free_req_body(&(krb_asreq->req_body));
	}
}

void krb_free_asrep(KrbASREP* krb_asrep)
{
	if(krb_asrep != NULL)
	{
		krb_free_kdcrep(&(krb_asrep->kdc_rep));
	}
}

void krb_free_tgsreq(KrbTGSREQ* krb_tgsreq)
{
	if(krb_tgsreq != NULL)
	{
		krb_free_padata(krb_tgsreq->padata);
		krb_free_req_body(&(krb_tgsreq->req_body));
	}
}

void krb_free_tgsrep(KrbTGSREP* krb_tgsrep)
{
	if(krb_tgsrep != NULL)
	{
		krb_free_kdcrep(&(krb_tgsrep->kdc_rep));
	}
}

void krb_free_krb_error(KrbERROR* krb_err)
{
	if(krb_err != NULL)
	{
		xfree(krb_err->stime);
		freerdp_blob_free(&(krb_err->edata));
	}
}

SECURITY_STATUS SEC_ENTRY kerberos_QueryContextAttributesW(PCtxtHandle phContext, uint32 ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_QueryContextAttributesA(PCtxtHandle phContext, uint32 ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (ulAttribute == SECPKG_ATTR_SIZES)
	{
		SecPkgContext_Sizes* ContextSizes = (SecPkgContext_Sizes*) pBuffer;

		ContextSizes->cbMaxToken = 2010;
		ContextSizes->cbMaxSignature = 16;
		ContextSizes->cbBlockSize = 0;
		ContextSizes->cbSecurityTrailer = 16;

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY kerberos_EncryptMessage(PCtxtHandle phContext, uint32 fQOP, PSecBufferDesc pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_MakeSignature(PCtxtHandle phContext, uint32 fQOP, PSecBufferDesc pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY kerberos_VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
}

const SecPkgInfoA KERBEROS_SecPkgInfoA =
{
	0x000F3BBF, /* fCapabilities */
	1, /* wVersion */
	0x0010, /* wRPCID */
	0x00002EE0, /* cbMaxToken */
	"Kerberos", /* Name */
	"Microsoft Kerberos V1.0" /* Comment */
};

const SecPkgInfoW KERBEROS_SecPkgInfoW =
{
	0x000F3BBF, /* fCapabilities */
	1, /* wVersion */
	0x0010, /* wRPCID */
	0x00002EE0, /* cbMaxToken */
	L"Kerberos", /* Name */
	L"Microsoft Kerberos V1.0" /* Comment */
};

const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	kerberos_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	kerberos_AcquireCredentialsHandleA, /* AcquireCredentialsHandle */
	kerberos_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	kerberos_InitializeSecurityContextA, /* InitializeSecurityContext */
	NULL, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	NULL, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	kerberos_QueryContextAttributesA, /* QueryContextAttributes */
	NULL, /* ImpersonateSecurityContext */
	NULL, /* RevertSecurityContext */
	kerberos_MakeSignature, /* MakeSignature */
	kerberos_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	kerberos_EncryptMessage, /* EncryptMessage */
	kerberos_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};

const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	kerberos_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	kerberos_AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	kerberos_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	kerberos_InitializeSecurityContextW, /* InitializeSecurityContext */
	NULL, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	NULL, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	kerberos_QueryContextAttributesW, /* QueryContextAttributes */
	NULL, /* ImpersonateSecurityContext */
	NULL, /* RevertSecurityContext */
	kerberos_MakeSignature, /* MakeSignature */
	kerberos_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	kerberos_EncryptMessage, /* EncryptMessage */
	kerberos_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};
