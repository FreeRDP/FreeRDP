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

#ifndef FREERDP_CORE_RPCH_H
#define FREERDP_CORE_RPCH_H

typedef struct rdp_rpch rdpRpch;
typedef struct rdp_rpch_http rdpRpchHTTP;

#include "tcp.h"

#include <time.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/sspi/sspi.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/hexdump.h>

#define PTYPE_REQUEST			0x00
#define PTYPE_PING			0x01
#define PTYPE_RESPONSE			0x02
#define PTYPE_FAULT			0x03
#define PTYPE_WORKING			0x04
#define PTYPE_NOCALL			0x05
#define PTYPE_REJECT			0x06
#define PTYPE_ACK			0x07
#define PTYPE_CL_CANCEL			0x08
#define PTYPE_FACK			0x09
#define PTYPE_CANCEL_ACK		0x0a
#define PTYPE_BIND			0x0b
#define PTYPE_BIND_ACK			0x0c
#define PTYPE_BIND_NAK			0x0d
#define PTYPE_ALTER_CONTEXT		0x0e
#define PTYPE_ALTER_CONTEXT_RESP	0x0f
#define PTYPE_RPC_AUTH_3		0x10
#define PTYPE_SHUTDOWN			0x11
#define PTYPE_CO_CANCEL			0x12
#define PTYPE_ORPHANED			0x13
#define PTYPE_RTS			0x14

#define PFC_FIRST_FRAG           0x01/* First fragment */
#define PFC_LAST_FRAG            0x02/* Last fragment */
#define PFC_PENDING_CANCEL       0x04/* Cancel was pending at sender */
#define PFC_RESERVED_1           0x08
#define PFC_CONC_MPX             0x10/* supports concurrent multiplexing
                                        * of a single connection. */
#define PFC_DID_NOT_EXECUTE      0x20/* only meaningful on `fault' packet;
                                        * if true, guaranteed call did not
                                        * execute. */
#define PFC_MAYBE                0x40/* `maybe' call semantics requested */
#define PFC_OBJECT_UUID          0x80/* if true, a non-nil object UUID
                                        * was specified in the handle, and
                                        * is present in the optional object
                                        * field. If false, the object field
                                        * is omitted. */

#define RTS_FLAG_NONE			0x0000	/*No special flags.*/
#define RTS_FLAG_PING			0x0001	/*Proves that the sender is still active; can also be used to flush the pipeline by the other party.*/
#define RTS_FLAG_OTHER_CMD		0x0002	/*Indicates that the PDU contains a command that cannot be defined by the other flags in this table.*/
#define RTS_FLAG_RECYCLE_CHANNEL 0x0004	/*Indicates that the PDU is associated with recycling a channel.*/
#define RTS_FLAG_IN_CHANNEL		0x0008	/*Indicates that the PDU is associated with IN channel communications.*/
#define RTS_FLAG_OUT_CHANNEL	0x0010	/*Indicates that the PDU is associated with OUT channel communications.*/
#define RTS_FLAG_EOF			0x0020	/*Indicates that this is the last PDU on an IN channel or OUT channel. Not all channels, however, use this to indicate the last PDU.*/
#define RTS_FLAG_ECHO			0x0040	/*Signifies that this PDU is an echo request or response.*/

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

enum _RPCH_HTTP_STATE
{
	RPCH_HTTP_DISCONNECTED = 0,
	RPCH_HTTP_SENDING = 1,
	RPCH_HTTP_RECIEVING = 2
};
typedef enum _RPCH_HTTP_STATE RPCH_HTTP_STATE;

struct rdp_rpch_http
{
	RPCH_HTTP_STATE state;
	int contentLength;
	int remContentLength;
	//struct _NTLMSSP* ntht;
};

struct rdp_rpch
{
	rdpSettings* settings;
	rdpTcp* tcp_in;
	rdpTcp* tcp_out;
	rdpTls* tls_in;
	rdpTls* tls_out;
	//struct _NTLMSSP* ntlmssp;

	rdpRpchHTTP* http_in;
	rdpRpchHTTP* http_out;

	uint8* write_buffer;
	uint32 write_buffer_len;
	uint8* read_buffer;
	uint32 read_buffer_len;

	uint32 BytesReceived;
	uint32 AwailableWindow;
	uint32 BytesSent;
	uint32 RecAwailableWindow;
	uint8* virtualConnectionCookie;
	uint8* OUTChannelCookie;
	uint8* INChannelCookie;
	uint32 call_id;
	uint32 pipe_call_id;
};

boolean rpch_attach(rdpRpch* rpch, rdpTcp* tcp_in, rdpTcp* tcp_out, rdpTls* tls_in, rdpTls* tls_out);
boolean rpch_connect(rdpRpch* rpch);

int rpch_write(rdpRpch* rpch, uint8* data, int length, uint16 opnum);
int rpch_read(rdpRpch* rpch, uint8* data, int length);

rdpRpch* rpch_new(rdpSettings* settings);

#ifdef WITH_DEBUG_TSG
#define WITH_DEBUG_RPCH
#endif

#ifdef WITH_DEBUG_RPCH
#define DEBUG_RPCH(fmt, ...) DEBUG_CLASS(RPCH, fmt, ## __VA_ARGS__)
#else
#define DEBUG_RPCH(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_CORE_RPCH_H */
