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
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_SSPI_KERBEROS_PRIVATE_H
#define FREERDP_SSPI_KERBEROS_PRIVATE_H

#ifndef _WIN32
#include <netdb.h>
#include <resolv.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#include <sys/types.h>

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/blob.h>
#include <freerdp/sspi/sspi.h>

#define MSKRB_OID               "1.2.840.48018.1.2.2"
#define STDKRB_OID              "1.2.840.113554.1.2.2"

#define SERVICE					"_kerberos."
#define KRB_VERSION				5
#define KRB_SERVER				"krbtgt/"
#define APP_SERVER				"TERMSRV/"

#define KRB_NAME_PRINCIPAL 		1
#define KRB_NAME_SERVICE		2

/* KRB TAGS */
#define KRB_TAG_ASREQ			10
#define KRB_TAG_ASREP			11
#define KRB_TAG_TGSREQ			12
#define KRB_TAG_TGSREP			13
#define KRB_TAG_APREQ			14
#define KRB_TAG_APREP			15
#define KRB_TAG_ERROR			30
#define KRB_TAG_U2UTGTREQ       16
#define KRB_TAG_U2UTGTREP       16

#define NAME_TYPE_PRINCIPAL		1
#define NAME_TYPE_SERVICE		2

/* KRB ERROR */
#define KDC_ERR_PREAUTH_FAILED  24
#define KDC_ERR_PREAUTH_REQ		25
#define KRB_AP_ERR_SKEW			37
#define KDC_ERR_C_PRINCIPAL_UNKNOWN 6

#define PA_ENCTYPE_INFO			11
#define PA_ENCTYPE_INFO2		19

/* ENCRYPTION TYPE */
#define ETYPE_DES_CBC_CRC		1
#define ETYPE_DES_CBC_MD5		3
#define ETYPE_AES128_CTS_HMAC	17
#define ETYPE_AES256_CTS_HMAC	18
#define ETYPE_RC4_HMAC			23

/* CHECKSUM TYPE */
#define KRB_CKSUM_HMAC_MD5      -138

/* AD TYPE */
#define AD_IF_RELEVANT          1

#define GETUINT16( _s, _p) do{ \
	*(_p) = (uint8)(_s)[0] << 8 | (uint8)(_s)[1]; \
	(_s) += 2; } while (0)
#define _GET_BYTE_LENGTH(_n) (((_n) > 0xFF) ? 3 :(((_n) > 0x7F) ? 2 : 1))

enum _KRBCTX_STATE
{
	KRB_STATE_INITIAL,
	KRB_FAILOVER_NTLM,
	KRB_ASREQ_OK,
	KRB_ASREP_OK,
	KRB_ASREP_ERR,
	KRB_TGSREQ_OK,
	KRB_TGSREP_OK,
	KRB_TGSREP_ERR,
	KRB_U2UTGTREQ_OK,
	KRB_U2UTGTREP_OK,
	KRB_APREQ_OK,
	KRB_APREP_OK,
	KRB_PACKET_ERROR,
	KRB_STATE_FINAL
};
typedef enum _KRBCTX_STATE KRBCTX_STATE;

struct _kdc_srv_entry
{
	struct _kdc_srv_entry* next;
	uint16 priority;
	uint16 weight;
	uint16 port;
	char* kdchost;
};
typedef struct _kdc_srv_entry KDCENTRY;

struct _PA_DATA
{
	int type;
	rdpBlob value;
};
typedef struct _PA_DATA PAData;

struct _AUTH_DATA
{
	int ad_type;
	uint8* ad_data;
};
typedef struct _AUTH_DATA AuthData;

struct _Krb_ENCData
{
	sint32 enctype;
	int kvno;
	rdpBlob encblob;
};
typedef struct _Krb_ENCData KrbENCData;

struct _Krb_ENCKey
{
	sint32 enctype;
	rdpBlob skey;
};
typedef struct _Krb_ENCKey KrbENCKey;

struct _ENC_KDC_REPPART
{
	KrbENCKey key;
	int nonce;
	uint32 flags;
	uint32 authtime;
	uint32 endtime;
	char *realm;
	char *sname;
};
typedef struct _ENC_KDC_REPPART ENCKDCREPPart;

struct _Krb_Ticket
{
	int tktvno;
	char* realm;
	char* sname;
	KrbENCData enc_part;
};
typedef struct _Krb_Ticket Ticket;

struct _Krb_Authenticator
{
	int avno;
	char* crealm;
	char* cname;
	int cksumtype;
	rdpBlob* cksum;
	uint32 cusec;
	char* ctime;
	uint32 seqno;
	AuthData auth_data;
};
typedef struct _Krb_Authenticator Authenticator;

struct _KDC_REQ_BODY
{
	uint32 kdc_options;
	char* cname;
	char* realm;
	char* sname;
	char* from;
	char* till;
	char* rtime;
	uint32 nonce;
};
typedef struct _KDC_REQ_BODY KDCReqBody;

struct _KRB_KDCREP
{
	int pvno;
	int type;
	PAData** padata;
	char* realm;
	char* cname;
	Ticket etgt;
	KrbENCData enc_part;
};
typedef struct _KRB_KDCREP KrbKDCREP;

struct _KRB_AS_REQ
{
	int pvno;
	int type;
    boolean pa_pac_request;
	PAData** padata;
	KDCReqBody req_body;
};
typedef struct _KRB_AS_REQ KrbASREQ;

struct _KRB_TGS_REQ
{
	int pvno;
	int type;
    boolean pa_pac_request;
	PAData** padata;
	KDCReqBody req_body;
};
typedef struct _KRB_TGS_REQ KrbTGSREQ;

struct _KRB_APREQ
{
	int pvno;
	int type;
	uint32 ap_options;
	Ticket* ticket;
	KrbENCData enc_auth;
};
typedef struct _KRB_APREQ KrbAPREQ;

struct _KRB_ERROR
{
	int pvno;
	int type;
	int errcode;
	char* stime;
	uint32 susec;
	char* realm;
	char* sname;
	rdpBlob edata;
};
typedef struct _KRB_ERROR KrbERROR;

struct _KRB_ASREP
{
	KrbKDCREP kdc_rep;
};
typedef struct _KRB_ASREP KrbASREP;

struct _KRB_TGSREP
{
	KrbKDCREP kdc_rep;
};
typedef struct _KRB_TGSREP KrbTGSREP;

struct _KRB_TGTREQ
{
	int pvno;
	int type;
	char* sname;
	char* realm;
};
typedef struct _KRB_TGTREQ KrbTGTREQ;

struct _KRB_TGTREP
{
	int pvno;
	int type;
	Ticket ticket;
};
typedef struct _KRB_TGTREP KrbTGTREP;

struct _KRB_CONTEXT
{
	UNICONV* uniconv;
	rdpSettings* settings;
	int ksockfd;
	uint16 krbport;
	char* krbhost;
	char* cname;
	char* realm;
	char* sname;
	char* hostname;
	SEC_AUTH_IDENTITY identity;
	rdpBlob passwd;
	sint32 enctype;
	sint32 clockskew;
	uint32 ctime;
	uint32 nonce;
	Ticket asticket;
	KrbENCKey* askey;
	Ticket tgsticket;
	KrbENCKey* tgskey;
	KRBCTX_STATE state;
	CTXT_HANDLE context;
};
typedef struct _KRB_CONTEXT KRB_CONTEXT;

CTXT_HANDLE* krbctx_client_init(rdpSettings* settings, SEC_AUTH_IDENTITY* identity);

boolean tcp_is_ipaddr(const char* hostname);
char* get_utc_time(time_t t);
time_t get_local_time(char* str);
uint32 get_clock_skew(char* str);
char* get_dns_queryname(char *host, char* protocol);
int krb_get_realm(rdpSettings* settings);
KDCENTRY* krb_locate_kdc(rdpSettings* settings);

int krb_tcp_connect(KRB_CONTEXT* krb_ctx, KDCENTRY* entry);
int krb_tcp_send(KRB_CONTEXT* krb_ctx, uint8* data, uint32 length);
int krb_tcp_recv(KRB_CONTEXT* krb_ctx, uint8* data, uint32 length);

void krb_asreq_send(KRB_CONTEXT* krb_ctx, uint8 errcode);
int krb_asrep_recv(KRB_CONTEXT* krb_ctx);

void krb_tgsreq_send(KRB_CONTEXT* krb_ctx, uint8 errcode);
int krb_tgsrep_recv(KRB_CONTEXT* krb_ctx);

int krb_verify_kdcrep(KRB_CONTEXT* krb_ctx, KrbKDCREP* kdc_rep, int msgtype);
void krb_save_ticket(KRB_CONTEXT* krb_ctx, KrbKDCREP* kdc_rep);

KrbASREQ* krb_asreq_new(KRB_CONTEXT* krb_ctx, uint8 errcode);
KrbTGSREQ* krb_tgsreq_new(KRB_CONTEXT* krb_Rss, uint8 errcode);
KrbAPREQ* krb_apreq_new(KRB_CONTEXT* krb_ctx, Ticket* ticket, Authenticator* krb_auth);

void krb_ContextFree(KRB_CONTEXT* krb_ctx);
void krb_free_ticket(Ticket* ticket);
void krb_free_padata(PAData** padata);
void krb_free_req_body(KDCReqBody* req_body);
void krb_free_kdcrep(KrbKDCREP* kdc_rep);
void krb_free_reppart(ENCKDCREPPart* reppart);
void krb_free_asreq(KrbASREQ* krb_asreq);
void krb_free_asrep(KrbASREP* krb_asrep);
void krb_free_tgsreq(KrbTGSREQ* krb_tgsreq);
void krb_free_tgsrep(KrbTGSREP* krb_tgsrep);
void krb_free_krb_error(KrbERROR* krb_err);

#endif /* FREERDP_SSPI_KERBEROS_PRIVATE_H */
