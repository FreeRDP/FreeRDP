/**
 * WinPR: Windows Portable Runtime
 * Microsoft Remote Procedure Call (MSRPC)
 *
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

#ifndef WINPR_RPC_H
#define WINPR_RPC_H

#include <winpr/wtypes.h>

typedef struct _CONTEXT_HANDLE
{
	UINT32 ContextType;
	BYTE ContextUuid[16];
} CONTEXT_HANDLE;

typedef PCONTEXT_HANDLE PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE;
typedef PCONTEXT_HANDLE PTUNNEL_CONTEXT_HANDLE_SERIALIZE;

typedef PCONTEXT_HANDLE PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE;
typedef PCONTEXT_HANDLE PCHANNEL_CONTEXT_HANDLE_SERIALIZE;

#if defined(_WIN32) && !defined(_UWP)

#include <rpc.h>

#else

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/sspi.h>
#include <winpr/spec.h>
#include <winpr/error.h>

#define RPC_S_OK				ERROR_SUCCESS
#define RPC_S_INVALID_ARG			ERROR_INVALID_PARAMETER
#define RPC_S_OUT_OF_MEMORY			ERROR_OUTOFMEMORY
#define RPC_S_OUT_OF_THREADS			ERROR_MAX_THRDS_REACHED
#define RPC_S_INVALID_LEVEL			ERROR_INVALID_PARAMETER
#define RPC_S_BUFFER_TOO_SMALL			ERROR_INSUFFICIENT_BUFFER
#define RPC_S_INVALID_SECURITY_DESC		ERROR_INVALID_SECURITY_DESCR
#define RPC_S_ACCESS_DENIED			ERROR_ACCESS_DENIED
#define RPC_S_SERVER_OUT_OF_MEMORY		ERROR_NOT_ENOUGH_SERVER_MEMORY
#define RPC_S_ASYNC_CALL_PENDING		ERROR_IO_PENDING
#define RPC_S_UNKNOWN_PRINCIPAL			ERROR_NONE_MAPPED
#define RPC_S_TIMEOUT				ERROR_TIMEOUT

#define RPC_X_NO_MEMORY				RPC_S_OUT_OF_MEMORY
#define RPC_X_INVALID_BOUND			RPC_S_INVALID_BOUND
#define RPC_X_INVALID_TAG			RPC_S_INVALID_TAG
#define RPC_X_ENUM_VALUE_TOO_LARGE		RPC_X_ENUM_VALUE_OUT_OF_RANGE
#define RPC_X_SS_CONTEXT_MISMATCH		ERROR_INVALID_HANDLE
#define RPC_X_INVALID_BUFFER			ERROR_INVALID_USER_BUFFER
#define RPC_X_PIPE_APP_MEMORY			ERROR_OUTOFMEMORY
#define RPC_X_INVALID_PIPE_OPERATION		RPC_X_WRONG_PIPE_ORDER

#define RPC_VAR_ENTRY	__cdecl

typedef long RPC_STATUS;

#ifndef _WIN32
typedef CHAR* RPC_CSTR;
typedef WCHAR* RPC_WSTR;
#endif

typedef void* I_RPC_HANDLE;
typedef I_RPC_HANDLE RPC_BINDING_HANDLE;
typedef RPC_BINDING_HANDLE handle_t;

typedef struct _RPC_BINDING_VECTOR
{
	unsigned long Count;
	RPC_BINDING_HANDLE BindingH[1];
} RPC_BINDING_VECTOR;
#define rpc_binding_vector_t RPC_BINDING_VECTOR

typedef struct _UUID_VECTOR
{
	unsigned long Count;
	UUID *Uuid[1];
} UUID_VECTOR;
#define uuid_vector_t UUID_VECTOR

typedef void *RPC_IF_HANDLE;

typedef struct _RPC_IF_ID
{
	UUID Uuid;
	unsigned short VersMajor;
	unsigned short VersMinor;
} RPC_IF_ID;

#define RPC_C_BINDING_INFINITE_TIMEOUT		10
#define RPC_C_BINDING_MIN_TIMEOUT		0
#define RPC_C_BINDING_DEFAULT_TIMEOUT		5
#define RPC_C_BINDING_MAX_TIMEOUT		9

#define RPC_C_CANCEL_INFINITE_TIMEOUT		-1

#define RPC_C_LISTEN_MAX_CALLS_DEFAULT		1234
#define RPC_C_PROTSEQ_MAX_REQS_DEFAULT		10

#define RPC_C_BIND_TO_ALL_NICS			1
#define RPC_C_USE_INTERNET_PORT			0x1
#define RPC_C_USE_INTRANET_PORT			0x2
#define RPC_C_DONT_FAIL				0x4

#define RPC_C_MQ_TEMPORARY			0x0000
#define RPC_C_MQ_PERMANENT			0x0001
#define RPC_C_MQ_CLEAR_ON_OPEN			0x0002
#define RPC_C_MQ_USE_EXISTING_SECURITY		0x0004
#define RPC_C_MQ_AUTHN_LEVEL_NONE		0x0000
#define RPC_C_MQ_AUTHN_LEVEL_PKT_INTEGRITY	0x0008
#define RPC_C_MQ_AUTHN_LEVEL_PKT_PRIVACY	0x0010

#define RPC_C_OPT_MQ_DELIVERY			1
#define RPC_C_OPT_MQ_PRIORITY			2
#define RPC_C_OPT_MQ_JOURNAL			3
#define RPC_C_OPT_MQ_ACKNOWLEDGE		4
#define RPC_C_OPT_MQ_AUTHN_SERVICE		5
#define RPC_C_OPT_MQ_AUTHN_LEVEL		6
#define RPC_C_OPT_MQ_TIME_TO_REACH_QUEUE	7
#define RPC_C_OPT_MQ_TIME_TO_BE_RECEIVED	8
#define RPC_C_OPT_BINDING_NONCAUSAL		9
#define RPC_C_OPT_SECURITY_CALLBACK		10
#define RPC_C_OPT_UNIQUE_BINDING		11
#define RPC_C_OPT_CALL_TIMEOUT			12
#define RPC_C_OPT_DONT_LINGER			13
#define RPC_C_OPT_MAX_OPTIONS			14

#define RPC_C_MQ_EXPRESS			0
#define RPC_C_MQ_RECOVERABLE			1

#define RPC_C_MQ_JOURNAL_NONE			0
#define RPC_C_MQ_JOURNAL_DEADLETTER		1
#define RPC_C_MQ_JOURNAL_ALWAYS			2

#define RPC_C_FULL_CERT_CHAIN			0x0001

typedef struct _RPC_PROTSEQ_VECTORA
{
	unsigned int Count;
	unsigned char *Protseq[1];
} RPC_PROTSEQ_VECTORA;

typedef struct _RPC_PROTSEQ_VECTORW
{
	unsigned int Count;
	unsigned short *Protseq[1];
} RPC_PROTSEQ_VECTORW;

#ifdef UNICODE
#define RPC_PROTSEQ_VECTOR	RPC_PROTSEQ_VECTORW
#else
#define RPC_PROTSEQ_VECTOR	RPC_PROTSEQ_VECTORA
#endif

typedef struct _RPC_POLICY
{
	unsigned int Length;
	unsigned long EndpointFlags;
	unsigned long NICFlags;
} RPC_POLICY, *PRPC_POLICY;

typedef void RPC_OBJECT_INQ_FN(UUID* ObjectUuid, UUID* TypeUuid, RPC_STATUS* pStatus);
typedef RPC_STATUS RPC_IF_CALLBACK_FN(RPC_IF_HANDLE InterfaceUuid, void* Context);
typedef void RPC_SECURITY_CALLBACK_FN(void* Context);

#define RPC_MGR_EPV void

typedef struct
{
	unsigned int Count;
	unsigned long Stats[1];
} RPC_STATS_VECTOR;

#define RPC_C_STATS_CALLS_IN		0
#define RPC_C_STATS_CALLS_OUT		1
#define RPC_C_STATS_PKTS_IN		2
#define RPC_C_STATS_PKTS_OUT		3

typedef struct
{
	unsigned long Count;
	RPC_IF_ID *IfId[1];
} RPC_IF_ID_VECTOR;

#ifndef _WIN32

typedef void *RPC_AUTH_IDENTITY_HANDLE;
typedef void *RPC_AUTHZ_HANDLE;

#define RPC_C_AUTHN_LEVEL_DEFAULT			0
#define RPC_C_AUTHN_LEVEL_NONE				1
#define RPC_C_AUTHN_LEVEL_CONNECT			2
#define RPC_C_AUTHN_LEVEL_CALL				3
#define RPC_C_AUTHN_LEVEL_PKT				4
#define RPC_C_AUTHN_LEVEL_PKT_INTEGRITY			5
#define RPC_C_AUTHN_LEVEL_PKT_PRIVACY			6

#define RPC_C_IMP_LEVEL_DEFAULT				0
#define RPC_C_IMP_LEVEL_ANONYMOUS			1
#define RPC_C_IMP_LEVEL_IDENTIFY			2
#define RPC_C_IMP_LEVEL_IMPERSONATE			3
#define RPC_C_IMP_LEVEL_DELEGATE			4

#define RPC_C_QOS_IDENTITY_STATIC			0
#define RPC_C_QOS_IDENTITY_DYNAMIC			1

#define RPC_C_QOS_CAPABILITIES_DEFAULT			0x0
#define RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH		0x1
#define RPC_C_QOS_CAPABILITIES_MAKE_FULLSIC		0x2
#define RPC_C_QOS_CAPABILITIES_ANY_AUTHORITY		0x4
#define RPC_C_QOS_CAPABILITIES_IGNORE_DELEGATE_FAILURE	0x8
#define RPC_C_QOS_CAPABILITIES_LOCAL_MA_HINT		0x10

#define RPC_C_PROTECT_LEVEL_DEFAULT			(RPC_C_AUTHN_LEVEL_DEFAULT)
#define RPC_C_PROTECT_LEVEL_NONE			(RPC_C_AUTHN_LEVEL_NONE)
#define RPC_C_PROTECT_LEVEL_CONNECT			(RPC_C_AUTHN_LEVEL_CONNECT)
#define RPC_C_PROTECT_LEVEL_CALL			(RPC_C_AUTHN_LEVEL_CALL)
#define RPC_C_PROTECT_LEVEL_PKT				(RPC_C_AUTHN_LEVEL_PKT)
#define RPC_C_PROTECT_LEVEL_PKT_INTEGRITY		(RPC_C_AUTHN_LEVEL_PKT_INTEGRITY)
#define RPC_C_PROTECT_LEVEL_PKT_PRIVACY			(RPC_C_AUTHN_LEVEL_PKT_PRIVACY)

#define RPC_C_AUTHN_NONE				0
#define RPC_C_AUTHN_DCE_PRIVATE				1
#define RPC_C_AUTHN_DCE_PUBLIC				2
#define RPC_C_AUTHN_DEC_PUBLIC				4
#define RPC_C_AUTHN_GSS_NEGOTIATE			9
#define RPC_C_AUTHN_WINNT				10
#define RPC_C_AUTHN_GSS_SCHANNEL			14
#define RPC_C_AUTHN_GSS_KERBEROS			16
#define RPC_C_AUTHN_DPA					17
#define RPC_C_AUTHN_MSN					18
#define RPC_C_AUTHN_DIGEST				21
#define RPC_C_AUTHN_MQ					100
#define RPC_C_AUTHN_DEFAULT				0xFFFFFFFFL

#define RPC_C_NO_CREDENTIALS				((RPC_AUTH_IDENTITY_HANDLE) MAXUINT_PTR)

#define RPC_C_SECURITY_QOS_VERSION			1L
#define RPC_C_SECURITY_QOS_VERSION_1			1L

typedef struct _RPC_SECURITY_QOS
{
	unsigned long Version;
	unsigned long Capabilities;
	unsigned long IdentityTracking;
	unsigned long ImpersonationType;
} RPC_SECURITY_QOS, *PRPC_SECURITY_QOS;

#define RPC_C_SECURITY_QOS_VERSION_2			2L

#define RPC_C_AUTHN_INFO_TYPE_HTTP			1

#define RPC_C_HTTP_AUTHN_TARGET_SERVER			1
#define RPC_C_HTTP_AUTHN_TARGET_PROXY			2

#define RPC_C_HTTP_AUTHN_SCHEME_BASIC			0x00000001
#define RPC_C_HTTP_AUTHN_SCHEME_NTLM			0x00000002
#define RPC_C_HTTP_AUTHN_SCHEME_PASSPORT		0x00000004
#define RPC_C_HTTP_AUTHN_SCHEME_DIGEST			0x00000008
#define RPC_C_HTTP_AUTHN_SCHEME_NEGOTIATE		0x00000010
#define RPC_C_HTTP_AUTHN_SCHEME_CERT			0x00010000

#define RPC_C_HTTP_FLAG_USE_SSL				1
#define RPC_C_HTTP_FLAG_USE_FIRST_AUTH_SCHEME		2
#define RPC_C_HTTP_FLAG_IGNORE_CERT_CN_INVALID		8

typedef struct _RPC_HTTP_TRANSPORT_CREDENTIALS_W
{
	SEC_WINNT_AUTH_IDENTITY_W* TransportCredentials;
	unsigned long Flags;
	unsigned long AuthenticationTarget;
	unsigned long NumberOfAuthnSchemes;
	unsigned long* AuthnSchemes;
	unsigned short* ServerCertificateSubject;
} RPC_HTTP_TRANSPORT_CREDENTIALS_W, *PRPC_HTTP_TRANSPORT_CREDENTIALS_W;

typedef struct _RPC_HTTP_TRANSPORT_CREDENTIALS_A
{
	SEC_WINNT_AUTH_IDENTITY_A* TransportCredentials;
	unsigned long Flags;
	unsigned long AuthenticationTarget;
	unsigned long NumberOfAuthnSchemes;
	unsigned long* AuthnSchemes;
	unsigned char* ServerCertificateSubject;
} RPC_HTTP_TRANSPORT_CREDENTIALS_A, *PRPC_HTTP_TRANSPORT_CREDENTIALS_A;

typedef struct _RPC_SECURITY_QOS_V2_W
{
	unsigned long Version;
	unsigned long Capabilities;
	unsigned long IdentityTracking;
	unsigned long ImpersonationType;
	unsigned long AdditionalSecurityInfoType;
	union {
		RPC_HTTP_TRANSPORT_CREDENTIALS_W* HttpCredentials;
	} u;
} RPC_SECURITY_QOS_V2_W, *PRPC_SECURITY_QOS_V2_W;

typedef struct _RPC_SECURITY_QOS_V2_A
{
	unsigned long Version;
	unsigned long Capabilities;
	unsigned long IdentityTracking;
	unsigned long ImpersonationType;
	unsigned long AdditionalSecurityInfoType;
	union {
		RPC_HTTP_TRANSPORT_CREDENTIALS_A* HttpCredentials;
	} u;
} RPC_SECURITY_QOS_V2_A, *PRPC_SECURITY_QOS_V2_A;

#define RPC_C_SECURITY_QOS_VERSION_3 3L

typedef struct _RPC_SECURITY_QOS_V3_W
{
	unsigned long Version;
	unsigned long Capabilities;
	unsigned long IdentityTracking;
	unsigned long ImpersonationType;
	unsigned long AdditionalSecurityInfoType;
	union {
		RPC_HTTP_TRANSPORT_CREDENTIALS_W* HttpCredentials;
	} u;
	void* Sid;
} RPC_SECURITY_QOS_V3_W, *PRPC_SECURITY_QOS_V3_W;

typedef struct _RPC_SECURITY_QOS_V3_A
{
	unsigned long Version;
	unsigned long Capabilities;
	unsigned long IdentityTracking;
	unsigned long ImpersonationType;
	unsigned long AdditionalSecurityInfoType;
	union {
		RPC_HTTP_TRANSPORT_CREDENTIALS_A* HttpCredentials;
	} u;
	void* Sid;
} RPC_SECURITY_QOS_V3_A, *PRPC_SECURITY_QOS_V3_A;

typedef enum _RPC_HTTP_REDIRECTOR_STAGE
{
	RPCHTTP_RS_REDIRECT = 1,
	RPCHTTP_RS_ACCESS_1,
	RPCHTTP_RS_SESSION,
	RPCHTTP_RS_ACCESS_2,
	RPCHTTP_RS_INTERFACE
} RPC_HTTP_REDIRECTOR_STAGE;

typedef RPC_STATUS (*RPC_NEW_HTTP_PROXY_CHANNEL)(RPC_HTTP_REDIRECTOR_STAGE RedirectorStage,
		unsigned short* ServerName, unsigned short* ServerPort, unsigned short* RemoteUser,
		unsigned short* AuthType, void* ResourceUuid, void* Metadata, void* SessionId,
		void* Interface, void* Reserved, unsigned long Flags, unsigned short** NewServerName,
		unsigned short** NewServerPort);

typedef void (*RPC_HTTP_PROXY_FREE_STRING)(unsigned short* String);

#define RPC_C_AUTHZ_NONE			0
#define RPC_C_AUTHZ_NAME			1
#define RPC_C_AUTHZ_DCE				2
#define RPC_C_AUTHZ_DEFAULT			0xFFFFFFFF

#endif

typedef void (*RPC_AUTH_KEY_RETRIEVAL_FN)(void* Arg, unsigned short* ServerPrincName, unsigned long KeyVer, void** Key, RPC_STATUS* pStatus);

#define DCE_C_ERROR_STRING_LEN			256

typedef I_RPC_HANDLE *RPC_EP_INQ_HANDLE;

#define RPC_C_EP_ALL_ELTS			0
#define RPC_C_EP_MATCH_BY_IF			1
#define RPC_C_EP_MATCH_BY_OBJ			2
#define RPC_C_EP_MATCH_BY_BOTH			3

#define RPC_C_VERS_ALL				1
#define RPC_C_VERS_COMPATIBLE			2
#define RPC_C_VERS_EXACT			3
#define RPC_C_VERS_MAJOR_ONLY			4
#define RPC_C_VERS_UPTO				5

typedef int (*RPC_MGMT_AUTHORIZATION_FN)(RPC_BINDING_HANDLE ClientBinding, unsigned long RequestedMgmtOperation, RPC_STATUS *pStatus);

#define RPC_C_MGMT_INQ_IF_IDS			0
#define RPC_C_MGMT_INQ_PRINC_NAME		1
#define RPC_C_MGMT_INQ_STATS			2
#define RPC_C_MGMT_IS_SERVER_LISTEN		3
#define RPC_C_MGMT_STOP_SERVER_LISTEN		4

#define RPC_C_PARM_MAX_PACKET_LENGTH		1
#define RPC_C_PARM_BUFFER_LENGTH		2

#define RPC_IF_AUTOLISTEN			0x0001
#define RPC_IF_OLE				0x0002
#define RPC_IF_ALLOW_UNKNOWN_AUTHORITY		0x0004
#define RPC_IF_ALLOW_SECURE_ONLY		0x0008
#define RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH	0x0010
#define RPC_IF_ALLOW_LOCAL_ONLY			0x0020
#define RPC_IF_SEC_NO_CACHE			0x0040

typedef struct _RPC_BINDING_HANDLE_OPTIONS_V1
{
  unsigned long Version;
  unsigned long Flags;
  unsigned long ComTimeout;
  unsigned long CallTimeout;
} RPC_BINDING_HANDLE_OPTIONS_V1, RPC_BINDING_HANDLE_OPTIONS;

typedef struct
{
	unsigned long Version;
	unsigned short* ServerPrincName;
	unsigned long AuthnLevel;
	unsigned long AuthnSvc;
	SEC_WINNT_AUTH_IDENTITY* AuthIdentity;
	RPC_SECURITY_QOS* SecurityQos;
} RPC_BINDING_HANDLE_SECURITY_V1, RPC_BINDING_HANDLE_SECURITY;

typedef struct _RPC_BINDING_HANDLE_TEMPLATE
{
	unsigned long Version;
	unsigned long Flags;
	unsigned long ProtocolSequence;
	unsigned short* NetworkAddress;
	unsigned short* StringEndpoint;
	union {
		unsigned short* Reserved;
	} u1;
	UUID ObjectUuid;
} RPC_BINDING_HANDLE_TEMPLATE_V1, RPC_BINDING_HANDLE_TEMPLATE;

#define RPC_CALL_STATUS_IN_PROGRESS		0x01
#define RPC_CALL_STATUS_CANCELLED		0x02
#define RPC_CALL_STATUS_DISCONNECTED		0x03

#include <winpr/ndr.h>
#include <winpr/midl.h>

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API RPC_STATUS RpcBindingCopy(RPC_BINDING_HANDLE SourceBinding, RPC_BINDING_HANDLE* DestinationBinding);
WINPR_API RPC_STATUS RpcBindingFree(RPC_BINDING_HANDLE* Binding);
WINPR_API RPC_STATUS RpcBindingSetOption(RPC_BINDING_HANDLE hBinding, unsigned long option, ULONG_PTR optionValue);
WINPR_API RPC_STATUS RpcBindingInqOption(RPC_BINDING_HANDLE hBinding, unsigned long option, ULONG_PTR *pOptionValue);
WINPR_API RPC_STATUS RpcBindingFromStringBindingA(RPC_CSTR StringBinding, RPC_BINDING_HANDLE* Binding);
WINPR_API RPC_STATUS RpcBindingFromStringBindingW(RPC_WSTR StringBinding, RPC_BINDING_HANDLE* Binding);
WINPR_API RPC_STATUS RpcSsGetContextBinding(void* ContextHandle, RPC_BINDING_HANDLE* Binding);
WINPR_API RPC_STATUS RpcBindingInqObject(RPC_BINDING_HANDLE Binding, UUID* ObjectUuid);
WINPR_API RPC_STATUS RpcBindingReset(RPC_BINDING_HANDLE Binding);
WINPR_API RPC_STATUS RpcBindingSetObject(RPC_BINDING_HANDLE Binding, UUID* ObjectUuid);
WINPR_API RPC_STATUS RpcMgmtInqDefaultProtectLevel(unsigned long AuthnSvc, unsigned long* AuthnLevel);
WINPR_API RPC_STATUS RpcBindingToStringBindingA(RPC_BINDING_HANDLE Binding, RPC_CSTR* StringBinding);
WINPR_API RPC_STATUS RpcBindingToStringBindingW(RPC_BINDING_HANDLE Binding, RPC_WSTR* StringBinding);
WINPR_API RPC_STATUS RpcBindingVectorFree(RPC_BINDING_VECTOR** BindingVector);
WINPR_API RPC_STATUS RpcStringBindingComposeA(RPC_CSTR ObjUuid, RPC_CSTR Protseq, RPC_CSTR NetworkAddr,
		RPC_CSTR Endpoint, RPC_CSTR Options, RPC_CSTR* StringBinding);
WINPR_API RPC_STATUS RpcStringBindingComposeW(RPC_WSTR ObjUuid, RPC_WSTR Protseq, RPC_WSTR NetworkAddr,
		RPC_WSTR Endpoint, RPC_WSTR Options, RPC_WSTR* StringBinding);
WINPR_API RPC_STATUS RpcStringBindingParseA(RPC_CSTR StringBinding, RPC_CSTR* ObjUuid, RPC_CSTR* Protseq,
		RPC_CSTR* NetworkAddr, RPC_CSTR *Endpoint, RPC_CSTR* NetworkOptions);
WINPR_API RPC_STATUS RpcStringBindingParseW(RPC_WSTR StringBinding, RPC_WSTR* ObjUuid, RPC_WSTR* Protseq,
		RPC_WSTR* NetworkAddr, RPC_WSTR *Endpoint, RPC_WSTR* NetworkOptions);
WINPR_API RPC_STATUS RpcStringFreeA(RPC_CSTR* String);
WINPR_API RPC_STATUS RpcStringFreeW(RPC_WSTR* String);
WINPR_API RPC_STATUS RpcIfInqId(RPC_IF_HANDLE RpcIfHandle, RPC_IF_ID* RpcIfId);
WINPR_API RPC_STATUS RpcNetworkIsProtseqValidA(RPC_CSTR Protseq);
WINPR_API RPC_STATUS RpcNetworkIsProtseqValidW(RPC_WSTR Protseq);
WINPR_API RPC_STATUS RpcMgmtInqComTimeout(RPC_BINDING_HANDLE Binding, unsigned int* Timeout);
WINPR_API RPC_STATUS RpcMgmtSetComTimeout(RPC_BINDING_HANDLE Binding, unsigned int Timeout);
WINPR_API RPC_STATUS RpcMgmtSetCancelTimeout(long Timeout);
WINPR_API RPC_STATUS RpcNetworkInqProtseqsA(RPC_PROTSEQ_VECTORA** ProtseqVector);
WINPR_API RPC_STATUS RpcNetworkInqProtseqsW(RPC_PROTSEQ_VECTORW** ProtseqVector);
WINPR_API RPC_STATUS RpcObjectInqType(UUID* ObjUuid, UUID* TypeUuid);
WINPR_API RPC_STATUS RpcObjectSetInqFn(RPC_OBJECT_INQ_FN* InquiryFn);
WINPR_API RPC_STATUS RpcObjectSetType(UUID* ObjUuid, UUID* TypeUuid);
WINPR_API RPC_STATUS RpcProtseqVectorFreeA(RPC_PROTSEQ_VECTORA** ProtseqVector);
WINPR_API RPC_STATUS RpcProtseqVectorFreeW(RPC_PROTSEQ_VECTORW** ProtseqVector);
WINPR_API RPC_STATUS RpcServerInqBindings (RPC_BINDING_VECTOR** BindingVector);
WINPR_API RPC_STATUS RpcServerInqIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV** MgrEpv);
WINPR_API RPC_STATUS RpcServerListen(unsigned int MinimumCallThreads, unsigned int MaxCalls, unsigned int DontWait);
WINPR_API RPC_STATUS RpcServerRegisterIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv);
WINPR_API RPC_STATUS RpcServerRegisterIfEx(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv,
		unsigned int Flags, unsigned int MaxCalls, RPC_IF_CALLBACK_FN* IfCallback);
WINPR_API RPC_STATUS RpcServerRegisterIf2(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv,
		unsigned int Flags, unsigned int MaxCalls, unsigned int MaxRpcSize, RPC_IF_CALLBACK_FN* IfCallbackFn);
WINPR_API RPC_STATUS RpcServerUnregisterIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, unsigned int WaitForCallsToComplete);
WINPR_API RPC_STATUS RpcServerUnregisterIfEx(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, int RundownContextHandles);
WINPR_API RPC_STATUS RpcServerUseAllProtseqs(unsigned int MaxCalls, void* SecurityDescriptor);
WINPR_API RPC_STATUS RpcServerUseAllProtseqsEx(unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy);
WINPR_API RPC_STATUS RpcServerUseAllProtseqsIf(unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor);
WINPR_API RPC_STATUS RpcServerUseAllProtseqsIfEx(unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy);
WINPR_API RPC_STATUS RpcServerUseProtseqA(RPC_CSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor);
WINPR_API RPC_STATUS RpcServerUseProtseqExA(RPC_CSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy);
WINPR_API RPC_STATUS RpcServerUseProtseqW(RPC_WSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor);
WINPR_API RPC_STATUS RpcServerUseProtseqExW(RPC_WSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy);
WINPR_API RPC_STATUS RpcServerUseProtseqEpA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_CSTR Endpoint, void* SecurityDescriptor);
WINPR_API RPC_STATUS RpcServerUseProtseqEpExA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_CSTR Endpoint, void* SecurityDescriptor, PRPC_POLICY Policy);
WINPR_API RPC_STATUS RpcServerUseProtseqEpW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_WSTR Endpoint, void* SecurityDescriptor);
WINPR_API RPC_STATUS RpcServerUseProtseqEpExW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_WSTR Endpoint, void* SecurityDescriptor, PRPC_POLICY Policy);
WINPR_API RPC_STATUS RpcServerUseProtseqIfA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor);
WINPR_API RPC_STATUS RpcServerUseProtseqIfExA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy);
WINPR_API RPC_STATUS RpcServerUseProtseqIfW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor);
WINPR_API RPC_STATUS RpcServerUseProtseqIfExW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy);
WINPR_API void RpcServerYield(void);
WINPR_API RPC_STATUS RpcMgmtStatsVectorFree(RPC_STATS_VECTOR** StatsVector);
WINPR_API RPC_STATUS RpcMgmtInqStats(RPC_BINDING_HANDLE Binding, RPC_STATS_VECTOR** Statistics);
WINPR_API RPC_STATUS RpcMgmtIsServerListening(RPC_BINDING_HANDLE Binding);
WINPR_API RPC_STATUS RpcMgmtStopServerListening(RPC_BINDING_HANDLE Binding);
WINPR_API RPC_STATUS RpcMgmtWaitServerListen(void);
WINPR_API RPC_STATUS RpcMgmtSetServerStackSize(unsigned long ThreadStackSize);
WINPR_API void RpcSsDontSerializeContext(void);
WINPR_API RPC_STATUS RpcMgmtEnableIdleCleanup(void);
WINPR_API RPC_STATUS RpcMgmtInqIfIds(RPC_BINDING_HANDLE Binding, RPC_IF_ID_VECTOR** IfIdVector);
WINPR_API RPC_STATUS RpcIfIdVectorFree(RPC_IF_ID_VECTOR** IfIdVector);
WINPR_API RPC_STATUS RpcMgmtInqServerPrincNameA(RPC_BINDING_HANDLE Binding, unsigned long AuthnSvc, RPC_CSTR* ServerPrincName);
WINPR_API RPC_STATUS RpcMgmtInqServerPrincNameW(RPC_BINDING_HANDLE Binding, unsigned long AuthnSvc, RPC_WSTR* ServerPrincName);
WINPR_API RPC_STATUS RpcServerInqDefaultPrincNameA(unsigned long AuthnSvc, RPC_CSTR* PrincName);
WINPR_API RPC_STATUS RpcServerInqDefaultPrincNameW(unsigned long AuthnSvc, RPC_WSTR* PrincName);
WINPR_API RPC_STATUS RpcEpResolveBinding(RPC_BINDING_HANDLE Binding, RPC_IF_HANDLE IfSpec);
WINPR_API RPC_STATUS RpcNsBindingInqEntryNameA(RPC_BINDING_HANDLE Binding, unsigned long EntryNameSyntax, RPC_CSTR* EntryName);
WINPR_API RPC_STATUS RpcNsBindingInqEntryNameW(RPC_BINDING_HANDLE Binding, unsigned long EntryNameSyntax, RPC_WSTR* EntryName);

WINPR_API RPC_STATUS RpcImpersonateClient(RPC_BINDING_HANDLE BindingHandle);
WINPR_API RPC_STATUS RpcRevertToSelfEx(RPC_BINDING_HANDLE BindingHandle);
WINPR_API RPC_STATUS RpcRevertToSelf(void);
WINPR_API RPC_STATUS RpcBindingInqAuthClientA(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
		RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc);
WINPR_API RPC_STATUS RpcBindingInqAuthClientW(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
		RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc);
WINPR_API RPC_STATUS RpcBindingInqAuthClientExA(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
		RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc, unsigned long Flags);
WINPR_API RPC_STATUS RpcBindingInqAuthClientExW(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
		RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc, unsigned long Flags);
WINPR_API RPC_STATUS RpcBindingInqAuthInfoA(RPC_BINDING_HANDLE Binding, RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel,
		unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc);
WINPR_API RPC_STATUS RpcBindingInqAuthInfoW(RPC_BINDING_HANDLE Binding, RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel,
		unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc);
WINPR_API RPC_STATUS RpcBindingSetAuthInfoA(RPC_BINDING_HANDLE Binding, RPC_CSTR ServerPrincName, unsigned long AuthnLevel,
		unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc);
WINPR_API RPC_STATUS RpcBindingSetAuthInfoExA(RPC_BINDING_HANDLE Binding, RPC_CSTR ServerPrincName, unsigned long AuthnLevel,
		unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc, RPC_SECURITY_QOS* SecurityQos);
WINPR_API RPC_STATUS RpcBindingSetAuthInfoW(RPC_BINDING_HANDLE Binding, RPC_WSTR ServerPrincName, unsigned long AuthnLevel,
		unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc);
WINPR_API RPC_STATUS RpcBindingSetAuthInfoExW(RPC_BINDING_HANDLE Binding, RPC_WSTR ServerPrincName, unsigned long AuthnLevel,
		unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc, RPC_SECURITY_QOS* SecurityQOS);
WINPR_API RPC_STATUS RpcBindingInqAuthInfoExA(RPC_BINDING_HANDLE Binding, RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel,
		unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc,
		unsigned long RpcQosVersion, RPC_SECURITY_QOS* SecurityQOS);
WINPR_API RPC_STATUS RpcBindingInqAuthInfoExW(RPC_BINDING_HANDLE Binding, RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel,
		unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc,
		unsigned long RpcQosVersion, RPC_SECURITY_QOS* SecurityQOS);

WINPR_API RPC_STATUS RpcServerRegisterAuthInfoA(RPC_CSTR ServerPrincName, unsigned long AuthnSvc, RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn, void* Arg);
WINPR_API RPC_STATUS RpcServerRegisterAuthInfoW(RPC_WSTR ServerPrincName, unsigned long AuthnSvc, RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn, void* Arg);

WINPR_API RPC_STATUS RpcBindingServerFromClient(RPC_BINDING_HANDLE ClientBinding, RPC_BINDING_HANDLE* ServerBinding);
WINPR_API DECLSPEC_NORETURN void RpcRaiseException(RPC_STATUS exception);
WINPR_API RPC_STATUS RpcTestCancel(void);
WINPR_API RPC_STATUS RpcServerTestCancel(RPC_BINDING_HANDLE BindingHandle);
WINPR_API RPC_STATUS RpcCancelThread(void* Thread);
WINPR_API RPC_STATUS RpcCancelThreadEx(void* Thread, long Timeout);

WINPR_API RPC_STATUS UuidCreate(UUID* Uuid);
WINPR_API RPC_STATUS UuidCreateSequential(UUID* Uuid);
WINPR_API RPC_STATUS UuidToStringA(UUID* Uuid, RPC_CSTR* StringUuid);
WINPR_API RPC_STATUS UuidFromStringA(RPC_CSTR StringUuid, UUID* Uuid);
WINPR_API RPC_STATUS UuidToStringW(UUID* Uuid, RPC_WSTR* StringUuid);
WINPR_API RPC_STATUS UuidFromStringW(RPC_WSTR StringUuid, UUID* Uuid);
WINPR_API signed int UuidCompare(UUID* Uuid1, UUID* Uuid2, RPC_STATUS* Status);
WINPR_API RPC_STATUS UuidCreateNil(UUID* NilUuid);
WINPR_API int UuidEqual(UUID* Uuid1, UUID* Uuid2, RPC_STATUS* Status);
WINPR_API unsigned short UuidHash(UUID* Uuid, RPC_STATUS* Status);
WINPR_API int UuidIsNil(UUID* Uuid, RPC_STATUS* Status);

WINPR_API RPC_STATUS RpcEpRegisterNoReplaceA(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_CSTR Annotation);
WINPR_API RPC_STATUS RpcEpRegisterNoReplaceW(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_WSTR Annotation);
WINPR_API RPC_STATUS RpcEpRegisterA(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_CSTR Annotation);
WINPR_API RPC_STATUS RpcEpRegisterW(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_WSTR Annotation);
WINPR_API RPC_STATUS RpcEpUnregister(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector);

WINPR_API RPC_STATUS DceErrorInqTextA(RPC_STATUS RpcStatus, RPC_CSTR ErrorText);
WINPR_API RPC_STATUS DceErrorInqTextW(RPC_STATUS RpcStatus, RPC_WSTR ErrorText);

WINPR_API RPC_STATUS RpcMgmtEpEltInqBegin(RPC_BINDING_HANDLE EpBinding, unsigned long InquiryType, RPC_IF_ID* IfId,
		unsigned long VersOption, UUID* ObjectUuid, RPC_EP_INQ_HANDLE* InquiryContext);
WINPR_API RPC_STATUS RpcMgmtEpEltInqDone(RPC_EP_INQ_HANDLE* InquiryContext);
WINPR_API RPC_STATUS RpcMgmtEpEltInqNextA(RPC_EP_INQ_HANDLE InquiryContext, RPC_IF_ID* IfId,
		RPC_BINDING_HANDLE* Binding, UUID* ObjectUuid, RPC_CSTR* Annotation);
WINPR_API RPC_STATUS RpcMgmtEpEltInqNextW(RPC_EP_INQ_HANDLE InquiryContext, RPC_IF_ID* IfId,
		RPC_BINDING_HANDLE* Binding, UUID* ObjectUuid, RPC_WSTR* Annotation);
WINPR_API RPC_STATUS RpcMgmtEpUnregister(RPC_BINDING_HANDLE EpBinding, RPC_IF_ID* IfId,
		RPC_BINDING_HANDLE Binding, UUID* ObjectUuid);
WINPR_API RPC_STATUS RpcMgmtSetAuthorizationFn(RPC_MGMT_AUTHORIZATION_FN AuthorizationFn);

WINPR_API RPC_STATUS RpcServerInqBindingHandle(RPC_BINDING_HANDLE* Binding);

#ifdef __cplusplus
}
#endif

#endif

#endif /* WINPR_RPC_H */
