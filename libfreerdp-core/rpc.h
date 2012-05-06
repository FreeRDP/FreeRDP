/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RPC over HTTP
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
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

#ifndef FREERDP_CORE_RPC_H
#define FREERDP_CORE_RPC_H

typedef struct rdp_rpc rdpRpc;
typedef struct rdp_ntlm_http rdpNtlmHttp;

#include "tcp.h"
#include "rts.h"
#include "http.h"
#include "transport.h"

#include <time.h>
#include <winpr/sspi.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/hexdump.h>

struct _rpc_pdu_header
{
	uint8 rpc_vers;
	uint8 rpc_vers_minor;
	uint8 ptype;
	uint8 pfc_flags;
	uint8 packed_drep[4];
	uint16 frag_length;
	uint16 auth_length;
	uint32 call_id;
};
typedef struct _rpc_pdu_header RPC_PDU_HEADER;

typedef   uint16   p_context_id_t;

typedef struct   {
	uuid		if_uuid;
	uint32		if_version;
} p_syntax_id_t;

typedef struct {
	p_context_id_t	p_cont_id;
	uint8			n_transfer_syn;     /* number of items */
	uint8			reserved;           /* alignment pad, m.b.z. */
	p_syntax_id_t	abstract_syntax;    /* transfer syntax list */
	p_syntax_id_t*	transfer_syntaxes; /*size_is(n_transfer_syn)*/
} p_cont_elem_t;

typedef struct {
	uint8			n_context_elem;      /* number of items */
	uint8			reserved;            /* alignment pad, m.b.z. */
	uint16			reserved2;           /* alignment pad, m.b.z. */
	p_cont_elem_t*	p_cont_elem;          /*size_is(n_cont_elem)*/
} p_cont_list_t;

typedef enum {
	acceptance, user_rejection, provider_rejection
} p_cont_def_result_t;

typedef enum {
	reason_not_specified,
	abstract_syntax_not_supported,
	proposed_transfer_syntaxes_not_supported,
	local_limit_exceeded
} p_provider_reason_t;


typedef struct {
	p_cont_def_result_t   result;
	p_provider_reason_t   reason;  /* only relevant if result !=
										 * acceptance */
	p_syntax_id_t         transfer_syntax;/* tr syntax selected
											 * 0 if result not
											 * accepted */
} p_result_t;

/* Same order and number of elements as in bind request */

typedef struct {
	uint8		n_results;      /* count */
	uint8		reserved;       /* alignment pad, m.b.z. */
	uint16		reserved2;       /* alignment pad, m.b.z. */
	p_result_t*	p_results; /*size_is(n_results)*/
} p_result_list_t;

typedef struct {
	uint8   major;
	uint8   minor;
} version_t;
typedef   version_t   p_rt_version_t;

typedef struct {
	uint8			n_protocols;   /* count */
	p_rt_version_t*	p_protocols;   /* size_is(n_protocols) */
} p_rt_versions_supported_t;

typedef struct {
	uint16	length;
	char*	port_spec; /* port string spec; size_is(length) */
} port_any_t;

#define REASON_NOT_SPECIFIED            0
#define TEMPORARY_CONGESTION            1
#define LOCAL_LIMIT_EXCEEDED            2
#define CALLED_PADDR_UNKNOWN            3 /* not used */
#define PROTOCOL_VERSION_NOT_SUPPORTED  4
#define DEFAULT_CONTEXT_NOT_SUPPORTED   5 /* not used */
#define USER_DATA_NOT_READABLE          6 /* not used */
#define NO_PSAP_AVAILABLE               7 /* not used */

typedef  uint16 rpcrt_reason_code_t;/* 0..65535 */

typedef  struct {
	uint8	rpc_vers;
	uint8	rpc_vers_minor;
	uint8	reserved[2];/* must be zero */
	uint8	packed_drep[4];
	uint32	reject_status;
	uint8	reserved2[4];
} rpcrt_optional_data_t;

typedef struct {
	rpcrt_reason_code_t		reason_code; /* 0..65535 */
	rpcrt_optional_data_t	rpc_info; /* may be RPC specific */
} rpcconn_reject_optional_data_t;

typedef struct {
	rpcrt_reason_code_t		reason_code; /* 0..65535 */
	rpcrt_optional_data_t	rpc_info; /* may be RPC-specific */
} rpcconn_disc_optional_data_t;

typedef struct{
	/* restore 4 byte alignment */

	uint8*	auth_pad; /* align(4); size_is(auth_pad_length) */
	uint8	auth_type;       /* :01  which authent service */
	uint8	auth_level;      /* :01  which level within service */
	uint8	auth_pad_length; /* :01 */
	uint8	auth_reserved;   /* :01 reserved, m.b.z. */
	uint32	auth_context_id; /* :04 */
	uint8*	auth_value; /* credentials; size_is(auth_length) */
} auth_verifier_co_t;

/* Connection-oriented PDU Definitions */

typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;          /* 00:01 RPC version */
	uint8  rpc_vers_minor ;       /* 01:01 minor version */
	uint8  PTYPE; /* 02:01 alter context PDU */
	uint8  pfc_flags;             /* 03:01 flags */
	uint8  packed_drep[4];        /* 04:04 NDR data rep format label*/
	uint16 frag_length;           /* 08:02 total length of fragment */
	uint16 auth_length;           /* 10:02 length of auth_value */
	uint32  call_id;              /* 12:04 call identifier */

	/* end common fields */

	uint16 max_xmit_frag;         /* ignored */
	uint16 max_recv_frag;         /* ignored */
	uint32 assoc_group_id;        /* ignored */

	/* presentation context list */

	p_cont_list_t  p_context_elem; /* variable size */

	/* optional authentication verifier */
	/* following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier;

} rpcconn_alter_context_hdr_t;

typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8	rpc_vers;       /* 00:01 RPC version */
	uint8	rpc_vers_minor;      /* 01:01 minor version */
	uint8	PTYPE; /* 02:01 alter
										 	 	 context response PDU */
	uint8	pfc_flags;          /* 03:01 flags */
	uint8	packed_drep[4];     /* 04:04 NDR data rep format label*/
	uint16	frag_length;        /* 08:02 total length of fragment */
	uint16	auth_length;        /* 10:02 length of auth_value */
	uint32	call_id;           /* 12:04 call identifier */

	/* end common fields */

	uint16		max_xmit_frag;      /* ignored */
	uint16		max_recv_frag;      /* ignored */
	uint32		assoc_group_id;     /* ignored */
	port_any_t	sec_addr;        /* ignored */

	/* restore 4-octet alignment */

	uint8*		pad2; /* size_is(align(4)) */

	/* presentation context result list, including hints */

	p_result_list_t		p_result_list;    /* variable size */

	/* optional authentication verifier */
	/* following fields present iff auth_length != 0 */

	auth_verifier_co_t	auth_verifier; /* xx:yy */
} rpcconn_alter_context_response_hdr_t;

/*  bind header */
typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;        /* 00:01 RPC version */
	uint8  rpc_vers_minor;      /* 01:01 minor version */
	uint8  PTYPE;        /* 02:01 bind PDU */
	uint8  pfc_flags;           /* 03:01 flags */
	uint8    packed_drep[4];      /* 04:04 NDR data rep format label*/
	uint16 frag_length;         /* 08:02 total length of fragment */
	uint16 auth_length;         /* 10:02 length of auth_value */
	uint32  call_id;            /* 12:04 call identifier */

	/* end common fields */

	uint16 max_xmit_frag;     /* 16:02 max transmit frag size, bytes */
	uint16 max_recv_frag;     /* 18:02 max receive  frag size, bytes */
	uint32 assoc_group_id;    /* 20:04 incarnation of client-server
														* assoc group */
	/* presentation context list */

	p_cont_list_t  p_context_elem; /* variable size */

	/* optional authentication verifier */
	/* following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier;
} rpcconn_bind_hdr_t;

typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;       /* 00:01 RPC version */
	uint8  rpc_vers_minor ;    /* 01:01 minor version */
	uint8  PTYPE;   /* 02:01 bind ack PDU */
	uint8  pfc_flags;          /* 03:01 flags */
	uint8    packed_drep[4];     /* 04:04 NDR data rep format label*/
	uint16 frag_length;        /* 08:02 total length of fragment */
	uint16 auth_length;        /* 10:02 length of auth_value */
	uint32  call_id;           /* 12:04 call identifier */

	/* end common fields */

	uint16 max_xmit_frag;      /* 16:02 max transmit frag size */
	uint16 max_recv_frag;      /* 18:02 max receive  frag size */
	uint32 assoc_group_id;     /* 20:04 returned assoc_group_id */
	port_any_t sec_addr;        /* 24:yy optional secondary address
								 * for process incarnation; local port
								 * part of address only */
	/* restore 4-octet alignment */

	uint8* pad2; /* size_is(align(4)) */

	/* presentation context result list, including hints */

	p_result_list_t     p_result_list;    /* variable size */

	/* optional authentication verifier */
	/* following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier; /* xx:yy */
} rpcconn_bind_ack_hdr_t;

typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;       /* 00:01 RPC version */
	uint8  rpc_vers_minor ;    /* 01:01 minor version */
	uint8  PTYPE;   /* 02:01 bind ack PDU */
	uint8  pfc_flags;          /* 03:01 flags */
	uint8    packed_drep[4];     /* 04:04 NDR data rep format label*/
	uint16 frag_length;        /* 08:02 total length of fragment */
	uint16 auth_length;        /* 10:02 length of auth_value */
	uint32  call_id;           /* 12:04 call identifier */

	/* end common fields */

	uint16 max_xmit_frag;      /* 16:02 max transmit frag size */
	uint16 max_recv_frag;      /* 18:02 max receive  frag size */

	/* optional authentication verifier */
	/* following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier; /* xx:yy */
} rpcconn_rpc_auth_3_hdr_t;

typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;       /* 00:01 RPC version */
	uint8  rpc_vers_minor ;    /* 01:01 minor version */
	uint8  PTYPE;   /* 02:01 bind nak PDU */
	uint8  pfc_flags;          /* 03:01 flags */
	uint8    packed_drep[4];     /* 04:04 NDR data rep format label*/
	uint16 frag_length;        /* 08:02 total length of fragment */
	uint16 auth_length;        /* 10:02 length of auth_value */
	uint32  call_id;           /* 12:04 call identifier */

	/* end common fields */

	/*p_reject_reason_t*/uint16 provider_reject_reason; /* 16:02 presentation (TODO search definition of p_reject_reason_t)
												  context reject */

	p_rt_versions_supported_t versions; /* 18:yy array of protocol
										 * versions supported */
} rpcconn_bind_nak_hdr_t;


typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;        /* 00:01 RPC version */
	uint8  rpc_vers_minor;       /* 01:01 minor version */
	uint8  PTYPE;    /* 02:01 CO cancel PDU */
	uint8  pfc_flags;            /* 03:01 flags */
	uint8    packed_drep[4];       /* 04:04 NDR data rep format label*/
	uint16 frag_length;          /* 08:02 total length of fragment */
	uint16 auth_length;          /* 10:02 length of auth_value */
	uint32  call_id;             /* 12:04 call identifier */

	/* end common fields */

	/* optional authentication verifier
	 * following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier; /* xx:yy */

} rpcconn_cancel_hdr_t;

typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;        /* 00:01 RPC version */
	uint8  rpc_vers_minor;      /* 01:01 minor version */
	uint8  PTYPE;       /* 02:01 fault PDU */
	uint8  pfc_flags;           /* 03:01 flags */
	uint8   packed_drep[4];      /* 04:04 NDR data rep format label*/
	uint16 frag_length;         /* 08:02 total length of fragment */
	uint16 auth_length;         /* 10:02 length of auth_value */
	uint32  call_id;            /* 12:04 call identifier */

	/* end common fields */

	/* needed for request, response, fault */

	uint32  alloc_hint;      /* 16:04 allocation hint */
	p_context_id_t p_cont_id; /* 20:02 pres context, i.e. data rep */

	/* needed for response or fault */

	uint8  cancel_count;      /* 22:01 received cancel count */
	uint8  reserved;         /* 23:01 reserved, m.b.z. */

	/* fault code  */

	uint32  status;           /* 24:04 run-time fault code or zero */

	/* always pad to next 8-octet boundary */

	uint8  reserved2[4];     /* 28:04 reserved padding, m.b.z. */

	/* stub data here, 8-octet aligned
		   .
		   .
		   .                          */
	uint8* stub_data;

	/* optional authentication verifier */
	/* following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier; /* xx:yy */
} rpcconn_fault_hdr_t;


typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;        /* 00:01 RPC version */
	uint8  rpc_vers_minor;      /* 01:01 minor version */
	uint8  PTYPE;    /* 02:01 orphaned PDU */
	uint8  pfc_flags;           /* 03:01 flags */
	uint8  packed_drep[4];      /* 04:04 NDR data rep format label*/
	uint16 frag_length;         /* 08:02 total length of fragment */
	uint16 auth_length;         /* 10:02 length of auth_value */
	uint32  call_id;            /* 12:04 call identifier */

	/* end common fields */

	/* optional authentication verifier
	 * following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier; /* xx:yy */
} rpcconn_orphaned_hdr_t;


typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;       /* 00:01 RPC version */
	uint8  rpc_vers_minor;     /* 01:01 minor version */
	uint8  PTYPE;   /* 02:01 request PDU */
	uint8  pfc_flags;          /* 03:01 flags */
	uint8  packed_drep[4];     /* 04:04 NDR data rep format label*/
	uint16 frag_length;        /* 08:02 total length of fragment */
	uint16 auth_length;        /* 10:02 length of auth_value */
	uint32  call_id;           /* 12:04 call identifier */

	/* end common fields */

	/* needed on request, response, fault */

	uint32  alloc_hint;        /* 16:04 allocation hint */
	p_context_id_t p_cont_id;  /* 20:02 pres context, i.e. data rep */
	uint16 opnum;              /* 22:02 operation #
								 * within the interface */

	/* optional field for request, only present if the PFC_OBJECT_UUID
	 * field is non-zero */

	uuid  object;              /* 24:16 object UID */

	/* stub data, 8-octet aligned
			   .
			   .
			   .                 */
	uint8* stub_data;

	/* optional authentication verifier */
	/* following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier; /* xx:yy */
} rpcconn_request_hdr_t;

typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;        /* 00:01 RPC version */
	uint8  rpc_vers_minor;      /* 01:01 minor version */
	uint8  PTYPE;    /* 02:01 response PDU */
	uint8  pfc_flags;           /* 03:01 flags */
	uint8  packed_drep[4];      /* 04:04 NDR data rep format label*/
	uint16 frag_length;         /* 08:02 total length of fragment */
	uint16 auth_length;         /* 10:02 length of auth_value */
	uint32 call_id;             /* 12:04 call identifier */

	/* end common fields */

	/* needed for request, response, fault */

	uint32  alloc_hint;         /* 16:04 allocation hint */
	p_context_id_t p_cont_id;    /* 20:02 pres context, i.e.
							  * data rep */

	/* needed for response or fault */

	uint8  cancel_count;        /* 22:01 cancel count */
	uint8  reserved;            /* 23:01 reserved, m.b.z. */

	/* stub data here, 8-octet aligned
	   .
	   .
	   .                          */
	uint8* stub_data;

	/* optional authentication verifier */
	/* following fields present iff auth_length != 0 */

	auth_verifier_co_t   auth_verifier; /* xx:yy */
} rpcconn_response_hdr_t;


typedef struct {
	/* start 8-octet aligned */

	/* common fields */
	uint8  rpc_vers;        /* 00:01 RPC version */
	uint8  rpc_vers_minor;      /* 01:01 minor version */
	uint8  PTYPE;   /* 02:01 shutdown PDU */
	uint8  pfc_flags;           /* 03:01 flags */
	uint8  packed_drep[4];      /* 04:04 NDR data rep format label*/
	uint16 frag_length;         /* 08:02 total length of fragment */
	uint16 auth_length;         /* 10:02 */
	uint32  call_id;            /* 12:04 call identifier */

	/* end common fields */
} rpcconn_shutdown_hdr_t;

struct rdp_ntlm
{
	UNICONV* uniconv;
	CtxtHandle context;
	uint32 cbMaxToken;
	uint32 fContextReq;
	uint32 pfContextAttr;
	TimeStamp expiration;
	PSecBuffer pBuffer;
	SecBuffer inputBuffer;
	SecBuffer outputBuffer;
	boolean haveContext;
	boolean haveInputBuffer;
	SecBufferDesc inputBufferDesc;
	SecBufferDesc outputBufferDesc;
	CredHandle credentials;
	boolean confidentiality;
	SecPkgInfo* pPackageInfo;
	SecurityFunctionTable* table;
	SEC_WINNT_AUTH_IDENTITY identity;
	SecPkgContext_Sizes ContextSizes;
};
typedef struct rdp_ntlm rdpNtlm;

enum _TSG_CHANNEL
{
	TSG_CHANNEL_IN,
	TSG_CHANNEL_OUT
};
typedef enum _TSG_CHANNEL TSG_CHANNEL;

struct rdp_ntlm_http
{
	rdpNtlm* ntlm;
	HttpContext* context;
};

/* Ping Originator */

struct rpc_ping_originator
{
	uint32 ConnectionTimeout;
	uint32 LastPacketSentTimestamp;
	uint32 KeepAliveInterval;
};
typedef struct rpc_ping_originator RpcPingOriginator;

/* Client In Channel */

struct rpc_in_channel
{
	/* Sending Channel */

	uint32 PlugState;
	void* SendQueue;
	uint32 BytesSent;
	uint32 SenderAvailableWindow;
	uint32 PeerReceiveWindow;

	/* Ping Originator */

	RpcPingOriginator PingOriginator;
};
typedef struct rpc_in_channel RpcInChannel;

/* Client Out Channel */

struct rpc_out_channel
{
	/* Receiving Channel */

	uint32 ReceiveWindow;
	uint32 ReceiveWindowSize;
	uint32 ReceiverAvailableWindow;
	uint32 BytesReceived;
	uint32 AvailableWindowAdvertised;
};
typedef struct rpc_out_channel RpcOutChannel;

/* Client Virtual Connection */

enum _VIRTUAL_CONNECTION_STATE
{
	VIRTUAL_CONNECTION_STATE_INITIAL,
	VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT,
	VIRTUAL_CONNECTION_STATE_WAIT_A3W,
	VIRTUAL_CONNECTION_STATE_WAIT_C2,
	VIRTUAL_CONNECTION_STATE_OPENED,
	VIRTUAL_CONNECTION_STATE_FINAL
};
typedef enum _VIRTUAL_CONNECTION_STATE VIRTUAL_CONNECTION_STATE;

struct rpc_virtual_connection
{
	uint8 Cookie[16]; /* Virtual Connection Cookie */
	VIRTUAL_CONNECTION_STATE State; /* Virtual Connection State */
	RpcInChannel* DefaultInChannel; /* Default IN Channel */
	RpcInChannel* NonDefaultInChannel; /* Non-Default IN Channel */
	uint8 DefaultInChannelCookie[16]; /* Default IN Channel Cookie */
	uint8 NonDefaultInChannelCookie[16]; /* Non-Default Default IN Channel Cookie */
	RpcOutChannel* DefaultOutChannel; /* Default OUT Channel */
	RpcOutChannel* NonDefaultOutChannel; /* Non-Default OUT Channel */
	uint8 DefaultOutChannelCookie[16]; /* Default OUT Channel Cookie */
	uint8 NonDefaultOutChannelCookie[16]; /* Non-Default Default OUT Channel Cookie */
	uint8 AssociationGroupId[16]; /* AssociationGroupId */
};
typedef struct rpc_virtual_connection RpcVirtualConnection;

struct rdp_rpc
{
	rdpTls* tls_in;
	rdpTls* tls_out;

	rdpNtlm* ntlm;
	int send_seq_num;

	rdpNtlmHttp* ntlm_http_in;
	rdpNtlmHttp* ntlm_http_out;

	UNICONV* uniconv;
	rdpSettings* settings;
	rdpTransport* transport;

	uint8* write_buffer;
	uint32 write_buffer_len;
	uint8* read_buffer;
	uint32 read_buffer_len;

	uint32 call_id;
	uint32 pipe_call_id;

	uint32 ReceiveWindow;

	RpcVirtualConnection* VirtualConnection;
};

boolean ntlm_authenticate(rdpNtlm* ntlm);

boolean ntlm_client_init(rdpNtlm* ntlm, boolean confidentiality, char* user, char* domain, char* password);
void ntlm_client_uninit(rdpNtlm* ntlm);

rdpNtlm* ntlm_new();
void ntlm_free(rdpNtlm* ntlm);

boolean rpc_connect(rdpRpc* rpc);

boolean rpc_ntlm_http_out_connect(rdpRpc* rpc);
boolean rpc_ntlm_http_in_connect(rdpRpc* rpc);

void rpc_pdu_header_read(STREAM* s, RPC_PDU_HEADER* header);

int rpc_out_write(rdpRpc* rpc, uint8* data, int length);
int rpc_in_write(rdpRpc* rpc, uint8* data, int length);

int rpc_out_read(rdpRpc* rpc, uint8* data, int length);

int rpc_tsg_write(rdpRpc* rpc, uint8* data, int length, uint16 opnum);
int rpc_read(rdpRpc* rpc, uint8* data, int length);

rdpRpc* rpc_new(rdpTransport* transport);
void rpc_free(rdpRpc* rpc);

#ifdef WITH_DEBUG_TSG
#define WITH_DEBUG_RPC
#endif

#ifdef WITH_DEBUG_RPC
#define DEBUG_RPC(fmt, ...) DEBUG_CLASS(RPC, fmt, ## __VA_ARGS__)
#else
#define DEBUG_RPC(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_CORE_RPC_H */
