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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <winpr/rpc.h>

#ifndef _WIN32

RPC_STATUS RpcBindingCopy(RPC_BINDING_HANDLE SourceBinding, RPC_BINDING_HANDLE* DestinationBinding)
{
	return 0;
}

RPC_STATUS RpcBindingFree(RPC_BINDING_HANDLE* Binding)
{
	return 0;
}

RPC_STATUS RpcBindingSetOption(RPC_BINDING_HANDLE hBinding, unsigned long option, ULONG_PTR optionValue)
{
	return 0;
}

RPC_STATUS RpcBindingInqOption(RPC_BINDING_HANDLE hBinding, unsigned long option, ULONG_PTR *pOptionValue)
{
	return 0;
}

RPC_STATUS RpcBindingFromStringBindingA(RPC_CSTR StringBinding, RPC_BINDING_HANDLE* Binding)
{
	return 0;
}

RPC_STATUS RpcBindingFromStringBindingW(RPC_WSTR StringBinding, RPC_BINDING_HANDLE* Binding)
{
	return 0;
}

RPC_STATUS RpcSsGetContextBinding(void* ContextHandle, RPC_BINDING_HANDLE* Binding)
{
	return 0;
}

RPC_STATUS RpcBindingInqObject(RPC_BINDING_HANDLE Binding, UUID* ObjectUuid)
{
	return 0;
}

RPC_STATUS RpcBindingReset(RPC_BINDING_HANDLE Binding)
{
	return 0;
}

RPC_STATUS RpcBindingSetObject(RPC_BINDING_HANDLE Binding, UUID* ObjectUuid)
{
	return 0;
}

RPC_STATUS RpcMgmtInqDefaultProtectLevel(unsigned long AuthnSvc, unsigned long* AuthnLevel)
{
	return 0;
}

RPC_STATUS RpcBindingToStringBindingA(RPC_BINDING_HANDLE Binding, RPC_CSTR* StringBinding)
{
	return 0;
}

RPC_STATUS RpcBindingToStringBindingW(RPC_BINDING_HANDLE Binding, RPC_WSTR* StringBinding)
{
	return 0;
}

RPC_STATUS RpcBindingVectorFree(RPC_BINDING_VECTOR** BindingVector)
{
	return 0;
}

RPC_STATUS RpcStringBindingComposeA(RPC_CSTR ObjUuid, RPC_CSTR Protseq, RPC_CSTR NetworkAddr,
		RPC_CSTR Endpoint, RPC_CSTR Options, RPC_CSTR* StringBinding)
{
	return 0;
}

RPC_STATUS RpcStringBindingComposeW(RPC_WSTR ObjUuid, RPC_WSTR Protseq, RPC_WSTR NetworkAddr,
		RPC_WSTR Endpoint, RPC_WSTR Options, RPC_WSTR* StringBinding)
{
	return 0;
}

RPC_STATUS RpcStringBindingParseA(RPC_CSTR StringBinding, RPC_CSTR* ObjUuid, RPC_CSTR* Protseq,
		RPC_CSTR* NetworkAddr, RPC_CSTR *Endpoint, RPC_CSTR* NetworkOptions)
{
	return 0;
}

RPC_STATUS RpcStringBindingParseW(RPC_WSTR StringBinding, RPC_WSTR* ObjUuid, RPC_WSTR* Protseq,
		RPC_WSTR* NetworkAddr, RPC_WSTR *Endpoint, RPC_WSTR* NetworkOptions)
{
	return 0;
}

RPC_STATUS RpcStringFreeA(RPC_CSTR* String)
{
	return 0;
}

RPC_STATUS RpcStringFreeW(RPC_WSTR* String)
{
	return 0;
}

RPC_STATUS RpcIfInqId(RPC_IF_HANDLE RpcIfHandle, RPC_IF_ID* RpcIfId)
{
	return 0;
}

RPC_STATUS RpcNetworkIsProtseqValidA(RPC_CSTR Protseq)
{
	return 0;
}

RPC_STATUS RpcNetworkIsProtseqValidW(RPC_WSTR Protseq)
{
	return 0;
}

RPC_STATUS RpcMgmtInqComTimeout(RPC_BINDING_HANDLE Binding, unsigned int* Timeout)
{
	return 0;
}

RPC_STATUS RpcMgmtSetComTimeout(RPC_BINDING_HANDLE Binding, unsigned int Timeout)
{
	return 0;
}

RPC_STATUS RpcMgmtSetCancelTimeout(long Timeout)
{
	return 0;
}

RPC_STATUS RpcNetworkInqProtseqsA(RPC_PROTSEQ_VECTORA** ProtseqVector)
{
	return 0;
}

RPC_STATUS RpcNetworkInqProtseqsW(RPC_PROTSEQ_VECTORW** ProtseqVector)
{
	return 0;
}

RPC_STATUS RpcObjectInqType(UUID* ObjUuid, UUID* TypeUuid)
{
	return 0;
}

RPC_STATUS RpcObjectSetInqFn(RPC_OBJECT_INQ_FN* InquiryFn)
{
	return 0;
}

RPC_STATUS RpcObjectSetType(UUID* ObjUuid, UUID* TypeUuid)
{
	return 0;
}

RPC_STATUS RpcProtseqVectorFreeA(RPC_PROTSEQ_VECTORA** ProtseqVector)
{
	return 0;
}

RPC_STATUS RpcProtseqVectorFreeW(RPC_PROTSEQ_VECTORW** ProtseqVector)
{
	return 0;
}

RPC_STATUS RpcServerInqBindings (RPC_BINDING_VECTOR** BindingVector)
{
	return 0;
}

RPC_STATUS RpcServerInqIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV** MgrEpv)
{
	return 0;
}

RPC_STATUS RpcServerListen(unsigned int MinimumCallThreads, unsigned int MaxCalls, unsigned int DontWait)
{
	return 0;
}

RPC_STATUS RpcServerRegisterIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv)
{
	return 0;
}

RPC_STATUS RpcServerRegisterIfEx(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv,
		unsigned int Flags, unsigned int MaxCalls, RPC_IF_CALLBACK_FN* IfCallback)
{
	return 0;
}

RPC_STATUS RpcServerRegisterIf2(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv,
		unsigned int Flags, unsigned int MaxCalls, unsigned int MaxRpcSize, RPC_IF_CALLBACK_FN* IfCallbackFn)
{
	return 0;
}

RPC_STATUS RpcServerUnregisterIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, unsigned int WaitForCallsToComplete)
{
	return 0;
}

RPC_STATUS RpcServerUnregisterIfEx(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, int RundownContextHandles)
{
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqs(unsigned int MaxCalls, void* SecurityDescriptor)
{
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsEx(unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsIf(unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor)
{
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsIfEx(unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqA(RPC_CSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqExA(RPC_CSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqW(RPC_WSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqExW(RPC_WSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_CSTR Endpoint, void* SecurityDescriptor)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpExA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_CSTR Endpoint, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_WSTR Endpoint, void* SecurityDescriptor)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpExW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_WSTR Endpoint, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfExA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor)
{
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfExW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	return 0;
}

void RpcServerYield()
{

}

RPC_STATUS RpcMgmtStatsVectorFree(RPC_STATS_VECTOR** StatsVector)
{
	return 0;
}

RPC_STATUS RpcMgmtInqStats(RPC_BINDING_HANDLE Binding, RPC_STATS_VECTOR** Statistics)
{
	return 0;
}

RPC_STATUS RpcMgmtIsServerListening(RPC_BINDING_HANDLE Binding)
{
	return 0;
}

RPC_STATUS RpcMgmtStopServerListening(RPC_BINDING_HANDLE Binding)
{
	return 0;
}

RPC_STATUS RpcMgmtWaitServerListen(void)
{
	return 0;
}

RPC_STATUS RpcMgmtSetServerStackSize(unsigned long ThreadStackSize)
{
	return 0;
}

void RpcSsDontSerializeContext(void)
{

}

RPC_STATUS RpcMgmtEnableIdleCleanup(void)
{
	return 0;
}

RPC_STATUS RpcMgmtInqIfIds(RPC_BINDING_HANDLE Binding, RPC_IF_ID_VECTOR** IfIdVector)
{
	return 0;
}

RPC_STATUS RpcIfIdVectorFree(RPC_IF_ID_VECTOR** IfIdVector)
{
	return 0;
}

RPC_STATUS RpcMgmtInqServerPrincNameA(RPC_BINDING_HANDLE Binding, unsigned long AuthnSvc, RPC_CSTR* ServerPrincName)
{
	return 0;
}

RPC_STATUS RpcMgmtInqServerPrincNameW(RPC_BINDING_HANDLE Binding, unsigned long AuthnSvc, RPC_WSTR* ServerPrincName)
{
	return 0;
}

RPC_STATUS RpcServerInqDefaultPrincNameA(unsigned long AuthnSvc, RPC_CSTR* PrincName)
{
	return 0;
}

RPC_STATUS RpcServerInqDefaultPrincNameW(unsigned long AuthnSvc, RPC_WSTR* PrincName)
{
	return 0;
}

RPC_STATUS RpcEpResolveBinding(RPC_BINDING_HANDLE Binding, RPC_IF_HANDLE IfSpec)
{
	return 0;
}

RPC_STATUS RpcNsBindingInqEntryNameA(RPC_BINDING_HANDLE Binding, unsigned long EntryNameSyntax, RPC_CSTR* EntryName)
{
	return 0;
}

RPC_STATUS RpcNsBindingInqEntryNameW(RPC_BINDING_HANDLE Binding, unsigned long EntryNameSyntax, RPC_WSTR* EntryName)
{
	return 0;
}


RPC_STATUS RpcImpersonateClient(RPC_BINDING_HANDLE BindingHandle)
{
	return 0;
}

RPC_STATUS RpcRevertToSelfEx(RPC_BINDING_HANDLE BindingHandle)
{
	return 0;
}

RPC_STATUS RpcRevertToSelf()
{
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientA(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
		RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc)
{
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientW(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
		RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc)
{
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientExA(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
		RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc, unsigned long Flags)
{
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientExW(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
		RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc, unsigned long Flags)
{
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoA(RPC_BINDING_HANDLE Binding, RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel,
		unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc)
{
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoW(RPC_BINDING_HANDLE Binding, RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel,
		unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc)
{
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoA(RPC_BINDING_HANDLE Binding, RPC_CSTR ServerPrincName, unsigned long AuthnLevel,
		unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc)
{
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoExA(RPC_BINDING_HANDLE Binding, RPC_CSTR ServerPrincName, unsigned long AuthnLevel,
		unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc, RPC_SECURITY_QOS* SecurityQos)
{
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoW(RPC_BINDING_HANDLE Binding, RPC_WSTR ServerPrincName, unsigned long AuthnLevel,
		unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc)
{
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoExW(RPC_BINDING_HANDLE Binding, RPC_WSTR ServerPrincName, unsigned long AuthnLevel,
		unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc, RPC_SECURITY_QOS* SecurityQOS)
{
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoExA(RPC_BINDING_HANDLE Binding, RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel,
		unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc,
		unsigned long RpcQosVersion, RPC_SECURITY_QOS* SecurityQOS)
{
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoExW(RPC_BINDING_HANDLE Binding, RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel,
		unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc,
		unsigned long RpcQosVersion, RPC_SECURITY_QOS* SecurityQOS)
{
	return 0;
}


RPC_STATUS RpcServerRegisterAuthInfoA(RPC_CSTR ServerPrincName, unsigned long AuthnSvc, RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn, void* Arg)
{
	return 0;
}

RPC_STATUS RpcServerRegisterAuthInfoW(RPC_WSTR ServerPrincName, unsigned long AuthnSvc, RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn, void* Arg)
{
	return 0;
}


RPC_STATUS RpcBindingServerFromClient(RPC_BINDING_HANDLE ClientBinding, RPC_BINDING_HANDLE* ServerBinding)
{
	return 0;
}

DECLSPEC_NORETURN void RpcRaiseException(RPC_STATUS exception)
{
	printf("RpcRaiseException: 0x%08luX\n", exception);
}

RPC_STATUS RpcTestCancel()
{
	return 0;
}

RPC_STATUS RpcServerTestCancel(RPC_BINDING_HANDLE BindingHandle)
{
	return 0;
}

RPC_STATUS RpcCancelThread(void* Thread)
{
	return 0;
}

RPC_STATUS RpcCancelThreadEx(void* Thread, long Timeout)
{
	return 0;
}


RPC_STATUS UuidCreate(UUID* Uuid)
{
	return 0;
}

RPC_STATUS UuidCreateSequential(UUID* Uuid)
{
	return 0;
}

RPC_STATUS UuidToStringA(UUID* Uuid, RPC_CSTR* StringUuid)
{
	return 0;
}

RPC_STATUS UuidFromStringA(RPC_CSTR StringUuid, UUID* Uuid)
{
	return 0;
}

RPC_STATUS UuidToStringW(UUID* Uuid, RPC_WSTR* StringUuid)
{
	return 0;
}

RPC_STATUS UuidFromStringW(RPC_WSTR StringUuid, UUID* Uuid)
{
	return 0;
}

signed int UuidCompare(UUID* Uuid1, UUID* Uuid2, RPC_STATUS* Status)
{
	return 0;
}

RPC_STATUS UuidCreateNil(UUID* NilUuid)
{
	return 0;
}

int UuidEqual(UUID* Uuid1, UUID* Uuid2, RPC_STATUS* Status)
{
	return 0;
}

unsigned short UuidHash(UUID* Uuid, RPC_STATUS* Status)
{
	return 0;
}

int UuidIsNil(UUID* Uuid, RPC_STATUS* Status)
{
	return 0;
}


RPC_STATUS RpcEpRegisterNoReplaceA(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_CSTR Annotation)
{
	return 0;
}

RPC_STATUS RpcEpRegisterNoReplaceW(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_WSTR Annotation)
{
	return 0;
}

RPC_STATUS RpcEpRegisterA(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_CSTR Annotation)
{
	return 0;
}

RPC_STATUS RpcEpRegisterW(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_WSTR Annotation)
{
	return 0;
}

RPC_STATUS RpcEpUnregister(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector)
{
	return 0;
}


RPC_STATUS DceErrorInqTextA(RPC_STATUS RpcStatus, RPC_CSTR ErrorText)
{
	return 0;
}

RPC_STATUS DceErrorInqTextW(RPC_STATUS RpcStatus, RPC_WSTR ErrorText)
{
	return 0;
}


RPC_STATUS RpcMgmtEpEltInqBegin(RPC_BINDING_HANDLE EpBinding, unsigned long InquiryType, RPC_IF_ID* IfId,
		unsigned long VersOption, UUID* ObjectUuid, RPC_EP_INQ_HANDLE* InquiryContext)
{
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqDone(RPC_EP_INQ_HANDLE* InquiryContext)
{
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqNextA(RPC_EP_INQ_HANDLE InquiryContext, RPC_IF_ID* IfId,
		RPC_BINDING_HANDLE* Binding, UUID* ObjectUuid, RPC_CSTR* Annotation)
{
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqNextW(RPC_EP_INQ_HANDLE InquiryContext, RPC_IF_ID* IfId,
		RPC_BINDING_HANDLE* Binding, UUID* ObjectUuid, RPC_WSTR* Annotation)
{
	return 0;
}

RPC_STATUS RpcMgmtEpUnregister(RPC_BINDING_HANDLE EpBinding, RPC_IF_ID* IfId,
		RPC_BINDING_HANDLE Binding, UUID* ObjectUuid)
{
	return 0;
}

RPC_STATUS RpcMgmtSetAuthorizationFn(RPC_MGMT_AUTHORIZATION_FN AuthorizationFn)
{
	return 0;
}


RPC_STATUS RpcServerInqBindingHandle(RPC_BINDING_HANDLE* Binding)
{
	return 0;
}

#endif
