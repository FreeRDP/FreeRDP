/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC over HTTP
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_GATEWAY_RPC_H
#define FREERDP_LIB_CORE_GATEWAY_RPC_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>
#include <winpr/collections.h>
#include <winpr/interlocked.h>

#include <freerdp/log.h>
#include <freerdp/utils/ringbuffer.h>

typedef struct rdp_rpc rdpRpc;

#pragma pack(push, 1)

#define DEFINE_RPC_COMMON_FIELDS() \
	BYTE rpc_vers; \
	BYTE rpc_vers_minor; \
	BYTE ptype; \
	BYTE pfc_flags; \
	BYTE packed_drep[4]; \
	UINT16 frag_length; \
	UINT16 auth_length; \
	UINT32 call_id

#define RPC_COMMON_FIELDS_LENGTH	16

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT16 Flags;
	UINT16 NumberOfCommands;
} rpcconn_rts_hdr_t;

#define RTS_PDU_HEADER_LENGTH		20

#define RPC_PDU_FLAG_STUB		0x00000001

typedef struct _RPC_PDU
{
	wStream* s;
	UINT32 Type;
	UINT32 Flags;
	UINT32 CallId;
} RPC_PDU, *PRPC_PDU;

#pragma pack(pop)

#include "../tcp.h"
#include "../transport.h"

#include "rts.h"
#include "http.h"
#include "ntlm.h"

#include <time.h>

#include <winpr/sspi.h>
#include <winpr/interlocked.h>

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/api.h>

#include <winpr/print.h>

/**
 * CAE Specification
 * DCE 1.1: Remote Procedure Call
 * Document Number: C706
 * http://pubs.opengroup.org/onlinepubs/9629399/
 */

#define PTYPE_REQUEST				0x00
#define PTYPE_PING				0x01
#define PTYPE_RESPONSE				0x02
#define PTYPE_FAULT				0x03
#define PTYPE_WORKING				0x04
#define PTYPE_NOCALL				0x05
#define PTYPE_REJECT				0x06
#define PTYPE_ACK				0x07
#define PTYPE_CL_CANCEL				0x08
#define PTYPE_FACK				0x09
#define PTYPE_CANCEL_ACK			0x0A
#define PTYPE_BIND				0x0B
#define PTYPE_BIND_ACK				0x0C
#define PTYPE_BIND_NAK				0x0D
#define PTYPE_ALTER_CONTEXT			0x0E
#define PTYPE_ALTER_CONTEXT_RESP		0x0F
#define PTYPE_RPC_AUTH_3			0x10
#define PTYPE_SHUTDOWN				0x11
#define PTYPE_CO_CANCEL				0x12
#define PTYPE_ORPHANED				0x13
#define PTYPE_RTS				0x14

#define PFC_FIRST_FRAG				0x01
#define PFC_LAST_FRAG				0x02
#define PFC_PENDING_CANCEL			0x04
#define PFC_SUPPORT_HEADER_SIGN			0x04
#define PFC_RESERVED_1				0x08
#define PFC_CONC_MPX				0x10
#define PFC_DID_NOT_EXECUTE			0x20
#define PFC_MAYBE				0x40
#define PFC_OBJECT_UUID				0x80

/* Minimum fragment sizes */
#define RPC_CO_MUST_RECV_FRAG_SIZE		1432
#define RPC_CL_MUST_RECV_FRAG_SIZE		1464

/**
 * The PDU maximum header length is enough
 * to contain either the RPC common fields
 * or all fields up to the stub data in PDUs
 * that use it (request, response, fault)
 */
#define RPC_PDU_HEADER_MAX_LENGTH   32

#pragma pack(push, 1)

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();
} rpcconn_common_hdr_t;

typedef UINT16 p_context_id_t;
typedef UINT16 p_reject_reason_t;

typedef struct
{
	UINT32 time_low;
	UINT16 time_mid;
	UINT16 time_hi_and_version;
	BYTE clock_seq_hi_and_reserved;
	BYTE clock_seq_low;
	BYTE node[6];
} p_uuid_t;

#define ndr_c_int_big_endian		0
#define ndr_c_int_little_endian		1
#define ndr_c_float_ieee		0
#define ndr_c_float_vax			1
#define ndr_c_float_cray		2
#define ndr_c_float_ibm			3
#define ndr_c_char_ascii		0
#define ndr_c_char_ebcdic		1

typedef struct
{
	BYTE int_rep;
	BYTE char_rep;
	BYTE float_rep;
	BYTE reserved;
} ndr_format_t, *ndr_format_p_t;

typedef struct ndr_context_handle
{
	UINT32 context_handle_attributes;
	p_uuid_t context_handle_uuid;
} ndr_context_handle;

typedef struct
{
	p_uuid_t if_uuid;
	UINT32 if_version;
} p_syntax_id_t;

typedef struct
{
	p_context_id_t p_cont_id;
	BYTE n_transfer_syn; /* number of items */
	BYTE reserved; /* alignment pad, m.b.z. */
	p_syntax_id_t abstract_syntax; /* transfer syntax list */
	p_syntax_id_t* transfer_syntaxes; /* size_is(n_transfer_syn) */
} p_cont_elem_t;

typedef struct
{
	BYTE n_context_elem; /* number of items */
	BYTE reserved; /* alignment pad, m.b.z. */
	UINT16 reserved2; /* alignment pad, m.b.z. */
	p_cont_elem_t* p_cont_elem; /* size_is(n_cont_elem) */
} p_cont_list_t;

typedef enum
{
	acceptance,
	user_rejection,
	provider_rejection
} p_cont_def_result_t;

typedef enum
{
	reason_not_specified,
	abstract_syntax_not_supported,
	proposed_transfer_syntaxes_not_supported,
	local_limit_exceeded
} p_provider_reason_t;

typedef struct
{
	p_cont_def_result_t result;
	p_provider_reason_t reason;
	p_syntax_id_t transfer_syntax;
} p_result_t;

/* Same order and number of elements as in bind request */

typedef struct
{
	BYTE n_results; /* count */
	BYTE reserved; /* alignment pad, m.b.z. */
	UINT16 reserved2; /* alignment pad, m.b.z. */
	p_result_t* p_results; /* size_is(n_results) */
} p_result_list_t;

typedef struct
{
	BYTE major;
	BYTE minor;
} version_t;
typedef version_t p_rt_version_t;

typedef struct
{
	BYTE n_protocols; /* count */
	p_rt_version_t*	p_protocols; /* size_is(n_protocols) */
} p_rt_versions_supported_t;

typedef struct
{
	UINT16 length;
	char* port_spec; /* port string spec; size_is(length) */
} port_any_t;

#define REASON_NOT_SPECIFIED			0
#define TEMPORARY_CONGESTION			1
#define LOCAL_LIMIT_EXCEEDED			2
#define CALLED_PADDR_UNKNOWN			3
#define PROTOCOL_VERSION_NOT_SUPPORTED		4
#define DEFAULT_CONTEXT_NOT_SUPPORTED		5
#define USER_DATA_NOT_READABLE			6
#define NO_PSAP_AVAILABLE			7

typedef UINT16 rpcrt_reason_code_t;

typedef struct
{
	BYTE rpc_vers;
	BYTE rpc_vers_minor;
	BYTE reserved[2];
	BYTE packed_drep[4];
	UINT32 reject_status;
	BYTE reserved2[4];
} rpcrt_optional_data_t;

typedef struct
{
	rpcrt_reason_code_t reason_code;
	rpcrt_optional_data_t rpc_info;
} rpcconn_reject_optional_data_t;

typedef struct
{
	rpcrt_reason_code_t reason_code;
	rpcrt_optional_data_t rpc_info;
} rpcconn_disc_optional_data_t;

typedef struct
{
	BYTE signature[8];
} rpc_sec_verification_trailer;

struct auth_verifier_co_s
{
	/* restore 4-byte alignment */

	BYTE auth_type;
	BYTE auth_level;
	BYTE auth_pad_length;
	BYTE auth_reserved;
	UINT32 auth_context_id;

	BYTE* auth_value;
};

typedef struct auth_verifier_co_s rpc_sec_trailer;
typedef struct auth_verifier_co_s auth_verifier_co_t;

/* Connection-oriented PDU Definitions */

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT16 max_xmit_frag;
	UINT16 max_recv_frag;
	UINT32 assoc_group_id;

	p_cont_list_t p_context_elem;

	auth_verifier_co_t auth_verifier;

} rpcconn_alter_context_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT16 max_xmit_frag;
	UINT16 max_recv_frag;
	UINT32 assoc_group_id;
	port_any_t sec_addr;

	/* restore 4-octet alignment */

	p_result_list_t	p_result_list;

	auth_verifier_co_t auth_verifier;
} rpcconn_alter_context_response_hdr_t;

/*  bind header */
typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT16 max_xmit_frag;
	UINT16 max_recv_frag;
	UINT32 assoc_group_id;

	p_cont_list_t p_context_elem;

	auth_verifier_co_t auth_verifier;
} rpcconn_bind_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT16 max_xmit_frag;
	UINT16 max_recv_frag;
	UINT32 assoc_group_id;

	port_any_t sec_addr;

	/* restore 4-octet alignment */

	p_result_list_t p_result_list;

	auth_verifier_co_t auth_verifier;
} rpcconn_bind_ack_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT16 max_xmit_frag;
	UINT16 max_recv_frag;

	auth_verifier_co_t auth_verifier;
} rpcconn_rpc_auth_3_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	p_reject_reason_t provider_reject_reason;

	p_rt_versions_supported_t versions;
} rpcconn_bind_nak_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	auth_verifier_co_t auth_verifier;

} rpcconn_cancel_hdr_t;

/* fault codes */

struct _RPC_FAULT_CODE
{
	UINT32 code;
	char* name;
};
typedef struct _RPC_FAULT_CODE RPC_FAULT_CODE;

#define DEFINE_RPC_FAULT_CODE(_code)	{ _code , #_code },

#define nca_s_comm_failure			0x1C010001
#define nca_s_op_rng_error			0x1C010002
#define nca_s_unk_if				0x1C010003
#define nca_s_wrong_boot_time			0x1C010006
#define nca_s_you_crashed			0x1C010009
#define nca_s_proto_error			0x1C01000B
#define nca_s_out_args_too_big			0x1C010013
#define nca_s_server_too_busy			0x1C010014
#define nca_s_fault_string_too_long		0x1C010015
#define nca_s_unsupported_type			0x1C010017
#define nca_s_fault_int_div_by_zero		0x1C000001
#define nca_s_fault_addr_error			0x1C000002
#define nca_s_fault_fp_div_zero			0x1C000003
#define nca_s_fault_fp_underflow		0x1C000004
#define nca_s_fault_fp_overflow			0x1C000005
#define nca_s_fault_invalid_tag			0x1C000006
#define nca_s_fault_invalid_bound		0x1C000007
#define nca_s_rpc_version_mismatch		0x1C000008
#define nca_s_unspec_reject			0x1C000009
#define nca_s_bad_actid				0x1C00000A
#define nca_s_who_are_you_failed		0x1C00000B
#define nca_s_manager_not_entered		0x1C00000C
#define nca_s_fault_cancel			0x1C00000D
#define nca_s_fault_ill_inst			0x1C00000E
#define nca_s_fault_fp_error			0x1C00000F
#define nca_s_fault_int_overflow		0x1C000010
#define nca_s_fault_unspec			0x1C000012
#define nca_s_fault_remote_comm_failure		0x1C000013
#define nca_s_fault_pipe_empty			0x1C000014
#define nca_s_fault_pipe_closed			0x1C000015
#define nca_s_fault_pipe_order			0x1C000016
#define nca_s_fault_pipe_discipline		0x1C000017
#define nca_s_fault_pipe_comm_error		0x1C000018
#define nca_s_fault_pipe_memory			0x1C000019
#define nca_s_fault_context_mismatch		0x1C00001A
#define nca_s_fault_remote_no_memory		0x1C00001B
#define nca_s_invalid_pres_context_id		0x1C00001C
#define nca_s_unsupported_authn_level		0x1C00001D
#define nca_s_invalid_checksum			0x1C00001F
#define nca_s_invalid_crc			0x1C000020
#define nca_s_fault_user_defined		0x1C000021
#define nca_s_fault_tx_open_failed		0x1C000022
#define nca_s_fault_codeset_conv_error		0x1C000023
#define nca_s_fault_object_not_found		0x1C000024
#define nca_s_fault_no_client_stub		0x1C000025

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT32 alloc_hint;
	p_context_id_t p_cont_id;

	BYTE cancel_count;
	BYTE reserved;

	UINT32 status;

	/* align(8) */

	BYTE* stub_data;

	auth_verifier_co_t auth_verifier;
} rpcconn_fault_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	auth_verifier_co_t auth_verifier;
} rpcconn_orphaned_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT32 alloc_hint;

	p_context_id_t p_cont_id;
	UINT16 opnum;

	/* optional field for request, only present if the PFC_OBJECT_UUID field is non-zero */
	p_uuid_t object;

	/* align(8) */

	BYTE* stub_data;

	auth_verifier_co_t auth_verifier;
} rpcconn_request_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();

	UINT32 alloc_hint;
	p_context_id_t p_cont_id;

	BYTE cancel_count;
	BYTE reserved;

	/* align(8) */

	BYTE* stub_data;

	auth_verifier_co_t auth_verifier;
} rpcconn_response_hdr_t;

typedef struct
{
	DEFINE_RPC_COMMON_FIELDS();
} rpcconn_shutdown_hdr_t;

typedef union
{
	rpcconn_common_hdr_t common;
	rpcconn_alter_context_hdr_t alter_context;
	rpcconn_alter_context_response_hdr_t alter_context_response;
	rpcconn_bind_hdr_t bind;
	rpcconn_bind_ack_hdr_t bind_ack;
	rpcconn_rpc_auth_3_hdr_t rpc_auth_3;
	rpcconn_bind_nak_hdr_t bind_nak;
	rpcconn_cancel_hdr_t cancel;
	rpcconn_fault_hdr_t fault;
	rpcconn_orphaned_hdr_t orphaned;
	rpcconn_request_hdr_t request;
	rpcconn_response_hdr_t response;
	rpcconn_shutdown_hdr_t shutdown;
	rpcconn_rts_hdr_t rts;
} rpcconn_hdr_t;

#pragma pack(pop)

struct _RPC_SECURITY_PROVIDER_INFO
{
	UINT32 Id;
	LONG EvenLegs;
	LONG NumLegs;
};
typedef struct _RPC_SECURITY_PROVIDER_INFO RPC_SECURITY_PROVIDER_INFO;

enum _RPC_CLIENT_STATE
{
	RPC_CLIENT_STATE_INITIAL,
	RPC_CLIENT_STATE_ESTABLISHED,
	RPC_CLIENT_STATE_WAIT_SECURE_BIND_ACK,
	RPC_CLIENT_STATE_WAIT_UNSECURE_BIND_ACK,
	RPC_CLIENT_STATE_WAIT_SECURE_ALTER_CONTEXT_RESPONSE,
	RPC_CLIENT_STATE_CONTEXT_NEGOTIATED,
	RPC_CLIENT_STATE_WAIT_RESPONSE,
	RPC_CLIENT_STATE_FINAL
};
typedef enum _RPC_CLIENT_STATE RPC_CLIENT_STATE;

enum _RPC_CLIENT_CALL_STATE
{
	RPC_CLIENT_CALL_STATE_INITIAL,
	RPC_CLIENT_CALL_STATE_SEND_PDUS,
	RPC_CLIENT_CALL_STATE_DISPATCHED,
	RPC_CLIENT_CALL_STATE_RECEIVE_PDU,
	RPC_CLIENT_CALL_STATE_COMPLETE,
	RPC_CLIENT_CALL_STATE_FAULT,
	RPC_CLIENT_CALL_STATE_FINAL
};
typedef enum _RPC_CLIENT_CALL_STATE RPC_CLIENT_CALL_STATE;

struct rpc_client_call
{
	UINT32 CallId;
	UINT32 OpNum;
	RPC_CLIENT_CALL_STATE State;
};
typedef struct rpc_client_call RpcClientCall;

#define RPC_CHANNEL_COMMON() \
	rdpRpc* rpc; \
	BIO* bio; \
	rdpTls* tls; \
	rdpNtlm* ntlm; \
	HttpContext* http; \
	BYTE Cookie[16]

struct rpc_channel
{
	RPC_CHANNEL_COMMON();
};
typedef struct rpc_channel RpcChannel;

/* Ping Originator */

struct rpc_ping_originator
{
	UINT32 ConnectionTimeout;
	UINT32 LastPacketSentTimestamp;
	UINT32 KeepAliveInterval;
};
typedef struct rpc_ping_originator RpcPingOriginator;

/* Client In Channel */

enum _CLIENT_IN_CHANNEL_STATE
{
	CLIENT_IN_CHANNEL_STATE_INITIAL,
	CLIENT_IN_CHANNEL_STATE_CONNECTED,
	CLIENT_IN_CHANNEL_STATE_SECURITY,
	CLIENT_IN_CHANNEL_STATE_NEGOTIATED,
	CLIENT_IN_CHANNEL_STATE_OPENED,
	CLIENT_IN_CHANNEL_STATE_OPENED_A4W,
	CLIENT_IN_CHANNEL_STATE_FINAL
};
typedef enum _CLIENT_IN_CHANNEL_STATE CLIENT_IN_CHANNEL_STATE;

struct rpc_in_channel
{
	/* Sending Channel */

	RPC_CHANNEL_COMMON();

	CLIENT_IN_CHANNEL_STATE State;

	UINT32 PlugState;
	void* SendQueue;
	UINT32 BytesSent;
	UINT32 SenderAvailableWindow;
	UINT32 PeerReceiveWindow;

	/* Ping Originator */

	RpcPingOriginator PingOriginator;
};
typedef struct rpc_in_channel RpcInChannel;

/* Client Out Channel */

enum _CLIENT_OUT_CHANNEL_STATE
{
	CLIENT_OUT_CHANNEL_STATE_INITIAL,
	CLIENT_OUT_CHANNEL_STATE_CONNECTED,
	CLIENT_OUT_CHANNEL_STATE_SECURITY,
	CLIENT_OUT_CHANNEL_STATE_NEGOTIATED,
	CLIENT_OUT_CHANNEL_STATE_OPENED,
	CLIENT_OUT_CHANNEL_STATE_OPENED_A6W,
	CLIENT_OUT_CHANNEL_STATE_OPENED_A10W,
	CLIENT_OUT_CHANNEL_STATE_OPENED_B3W,
	CLIENT_OUT_CHANNEL_STATE_RECYCLED,
	CLIENT_OUT_CHANNEL_STATE_FINAL
};
typedef enum _CLIENT_OUT_CHANNEL_STATE CLIENT_OUT_CHANNEL_STATE;

struct rpc_out_channel
{
	/* Receiving Channel */

	RPC_CHANNEL_COMMON();

	CLIENT_OUT_CHANNEL_STATE State;

	UINT32 ReceiveWindow;
	UINT32 ReceiveWindowSize;
	UINT32 ReceiverAvailableWindow;
	UINT32 BytesReceived;
	UINT32 AvailableWindowAdvertised;
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
	BYTE Cookie[16];
	BYTE AssociationGroupId[16];
	VIRTUAL_CONNECTION_STATE State;
	RpcInChannel* DefaultInChannel;
	RpcInChannel* NonDefaultInChannel;
	RpcOutChannel* DefaultOutChannel;
	RpcOutChannel* NonDefaultOutChannel;
};
typedef struct rpc_virtual_connection RpcVirtualConnection;

/* Virtual Connection Cookie Table */

#define RPC_UUID_FORMAT_STRING 	"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define RPC_UUID_FORMAT_ARGUMENTS(_rpc_uuid) \
	_rpc_uuid[0], _rpc_uuid[1], _rpc_uuid[2], _rpc_uuid[3], _rpc_uuid[4], _rpc_uuid[5], _rpc_uuid[6], _rpc_uuid[7], \
	_rpc_uuid[8], _rpc_uuid[9], _rpc_uuid[10], _rpc_uuid[11], _rpc_uuid[12], _rpc_uuid[13], _rpc_uuid[14], _rpc_uuid[15]

struct rpc_virtual_connection_cookie_entry
{
	BYTE Cookie[16];
	UINT32 ReferenceCount;
	RpcVirtualConnection* Reference;
};
typedef struct rpc_virtual_connection_cookie_entry
	RpcVirtualConnectionCookieEntry;

struct rpc_client
{
	RPC_PDU* pdu;
	HANDLE PipeEvent;
	RingBuffer ReceivePipe;
	wStream* ReceiveFragment;
	CRITICAL_SECTION PipeLock;
	wArrayList* ClientCallList;
};
typedef struct rpc_client RpcClient;

struct rdp_rpc
{
	RPC_CLIENT_STATE State;

	UINT32 result;

	rdpNtlm* ntlm;
	int SendSeqNum;

	RpcClient* client;

	rdpContext* context;
	rdpSettings* settings;
	rdpTransport* transport;

	UINT32 CallId;
	UINT32 PipeCallId;

	UINT32 StubCallId;
	UINT32 StubFragCount;

	BYTE rpc_vers;
	BYTE rpc_vers_minor;
	BYTE packed_drep[4];

	UINT16 max_xmit_frag;
	UINT16 max_recv_frag;

	UINT32 ReceiveWindow;
	UINT32 ChannelLifetime;
	UINT32 KeepAliveInterval;
	UINT32 CurrentKeepAliveTime;
	UINT32 CurrentKeepAliveInterval;

	RpcVirtualConnection* VirtualConnection;
};

FREERDP_LOCAL void rpc_pdu_header_print(rpcconn_hdr_t* header);
FREERDP_LOCAL void rpc_pdu_header_init(rdpRpc* rpc, rpcconn_hdr_t* header);

FREERDP_LOCAL UINT32 rpc_offset_align(UINT32* offset, UINT32 alignment);
FREERDP_LOCAL UINT32 rpc_offset_pad(UINT32* offset, UINT32 pad);

FREERDP_LOCAL BOOL rpc_get_stub_data_info(rdpRpc* rpc, BYTE* header,
        UINT32* offset, UINT32* length);

FREERDP_LOCAL int rpc_in_channel_write(RpcInChannel* inChannel,
                                       const BYTE* data, int length);

FREERDP_LOCAL int rpc_out_channel_read(RpcOutChannel* outChannel, BYTE* data,
                                       int length);
FREERDP_LOCAL int rpc_out_channel_write(RpcOutChannel* outChannel,
                                        const BYTE* data, int length);

FREERDP_LOCAL RpcOutChannel* rpc_out_channel_new(rdpRpc* rpc);
FREERDP_LOCAL int rpc_out_channel_replacement_connect(RpcOutChannel* outChannel,
        int timeout);
FREERDP_LOCAL void rpc_out_channel_free(RpcOutChannel* outChannel);

FREERDP_LOCAL int rpc_in_channel_transition_to_state(RpcInChannel* inChannel,
        CLIENT_IN_CHANNEL_STATE state);
FREERDP_LOCAL int rpc_out_channel_transition_to_state(RpcOutChannel* outChannel,
        CLIENT_OUT_CHANNEL_STATE state);

FREERDP_LOCAL int rpc_virtual_connection_transition_to_state(rdpRpc* rpc,
        RpcVirtualConnection* connection, VIRTUAL_CONNECTION_STATE state);

FREERDP_LOCAL BOOL rpc_connect(rdpRpc* rpc, int timeout);

FREERDP_LOCAL rdpRpc* rpc_new(rdpTransport* transport);
FREERDP_LOCAL void rpc_free(rdpRpc* rpc);

#endif /* FREERDP_LIB_CORE_GATEWAY_RPC_H */
