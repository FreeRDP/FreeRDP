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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/rpc.h>
#include <winpr/crypto.h>

#if !defined(_WIN32) || defined(_UWP)

#include "../log.h"
#define TAG WINPR_TAG("rpc")

RPC_STATUS RpcBindingCopy(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE SourceBinding,
                          WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* DestinationBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingFree(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetOption(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE hBinding,
                               WINPR_ATTR_UNUSED unsigned long option,
                               WINPR_ATTR_UNUSED ULONG_PTR optionValue)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqOption(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE hBinding,
                               WINPR_ATTR_UNUSED unsigned long option,
                               WINPR_ATTR_UNUSED ULONG_PTR* pOptionValue)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingFromStringBindingA(WINPR_ATTR_UNUSED RPC_CSTR StringBinding,
                                        WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingFromStringBindingW(WINPR_ATTR_UNUSED RPC_WSTR StringBinding,
                                        WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcSsGetContextBinding(WINPR_ATTR_UNUSED void* ContextHandle,
                                  WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqObject(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                               WINPR_ATTR_UNUSED UUID* ObjectUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingReset(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetObject(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                               WINPR_ATTR_UNUSED UUID* ObjectUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqDefaultProtectLevel(WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                         WINPR_ATTR_UNUSED unsigned long* AuthnLevel)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingToStringBindingA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                      WINPR_ATTR_UNUSED RPC_CSTR* StringBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingToStringBindingW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                      WINPR_ATTR_UNUSED RPC_WSTR* StringBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingVectorFree(WINPR_ATTR_UNUSED RPC_BINDING_VECTOR** BindingVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringBindingComposeA(WINPR_ATTR_UNUSED RPC_CSTR ObjUuid,
                                    WINPR_ATTR_UNUSED RPC_CSTR Protseq,
                                    WINPR_ATTR_UNUSED RPC_CSTR NetworkAddr,
                                    WINPR_ATTR_UNUSED RPC_CSTR Endpoint,
                                    WINPR_ATTR_UNUSED RPC_CSTR Options,
                                    WINPR_ATTR_UNUSED RPC_CSTR* StringBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringBindingComposeW(WINPR_ATTR_UNUSED RPC_WSTR ObjUuid,
                                    WINPR_ATTR_UNUSED RPC_WSTR Protseq,
                                    WINPR_ATTR_UNUSED RPC_WSTR NetworkAddr,
                                    WINPR_ATTR_UNUSED RPC_WSTR Endpoint,
                                    WINPR_ATTR_UNUSED RPC_WSTR Options,
                                    WINPR_ATTR_UNUSED RPC_WSTR* StringBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringBindingParseA(WINPR_ATTR_UNUSED RPC_CSTR StringBinding,
                                  WINPR_ATTR_UNUSED RPC_CSTR* ObjUuid,
                                  WINPR_ATTR_UNUSED RPC_CSTR* Protseq,
                                  WINPR_ATTR_UNUSED RPC_CSTR* NetworkAddr,
                                  WINPR_ATTR_UNUSED RPC_CSTR* Endpoint,
                                  WINPR_ATTR_UNUSED RPC_CSTR* NetworkOptions)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringBindingParseW(WINPR_ATTR_UNUSED RPC_WSTR StringBinding,
                                  WINPR_ATTR_UNUSED RPC_WSTR* ObjUuid,
                                  WINPR_ATTR_UNUSED RPC_WSTR* Protseq,
                                  WINPR_ATTR_UNUSED RPC_WSTR* NetworkAddr,
                                  WINPR_ATTR_UNUSED RPC_WSTR* Endpoint,
                                  WINPR_ATTR_UNUSED RPC_WSTR* NetworkOptions)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringFreeA(RPC_CSTR* String)
{
	if (String)
		free(*String);

	return RPC_S_OK;
}

RPC_STATUS RpcStringFreeW(RPC_WSTR* String)
{
	if (String)
		free(*String);

	return RPC_S_OK;
}

RPC_STATUS RpcIfInqId(WINPR_ATTR_UNUSED RPC_IF_HANDLE RpcIfHandle,
                      WINPR_ATTR_UNUSED RPC_IF_ID* RpcIfId)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNetworkIsProtseqValidA(WINPR_ATTR_UNUSED RPC_CSTR Protseq)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNetworkIsProtseqValidW(WINPR_ATTR_UNUSED RPC_WSTR Protseq)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqComTimeout(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                WINPR_ATTR_UNUSED unsigned int* Timeout)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtSetComTimeout(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                WINPR_ATTR_UNUSED unsigned int Timeout)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtSetCancelTimeout(WINPR_ATTR_UNUSED long Timeout)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNetworkInqProtseqsA(WINPR_ATTR_UNUSED RPC_PROTSEQ_VECTORA** ProtseqVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNetworkInqProtseqsW(WINPR_ATTR_UNUSED RPC_PROTSEQ_VECTORW** ProtseqVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcObjectInqType(WINPR_ATTR_UNUSED UUID* ObjUuid, WINPR_ATTR_UNUSED UUID* TypeUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcObjectSetInqFn(WINPR_ATTR_UNUSED RPC_OBJECT_INQ_FN* InquiryFn)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcObjectSetType(WINPR_ATTR_UNUSED UUID* ObjUuid, WINPR_ATTR_UNUSED UUID* TypeUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcProtseqVectorFreeA(WINPR_ATTR_UNUSED RPC_PROTSEQ_VECTORA** ProtseqVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcProtseqVectorFreeW(WINPR_ATTR_UNUSED RPC_PROTSEQ_VECTORW** ProtseqVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqBindings(WINPR_ATTR_UNUSED RPC_BINDING_VECTOR** BindingVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqIf(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                          WINPR_ATTR_UNUSED UUID* MgrTypeUuid,
                          WINPR_ATTR_UNUSED RPC_MGR_EPV** MgrEpv)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerListen(WINPR_ATTR_UNUSED unsigned int MinimumCallThreads,
                           WINPR_ATTR_UNUSED unsigned int MaxCalls,
                           WINPR_ATTR_UNUSED unsigned int DontWait)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterIf(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                               WINPR_ATTR_UNUSED UUID* MgrTypeUuid,
                               WINPR_ATTR_UNUSED RPC_MGR_EPV* MgrEpv)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterIfEx(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                 WINPR_ATTR_UNUSED UUID* MgrTypeUuid,
                                 WINPR_ATTR_UNUSED RPC_MGR_EPV* MgrEpv,
                                 WINPR_ATTR_UNUSED unsigned int Flags,
                                 WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                 WINPR_ATTR_UNUSED RPC_IF_CALLBACK_FN* IfCallback)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterIf2(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                WINPR_ATTR_UNUSED UUID* MgrTypeUuid,
                                WINPR_ATTR_UNUSED RPC_MGR_EPV* MgrEpv,
                                WINPR_ATTR_UNUSED unsigned int Flags,
                                WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                WINPR_ATTR_UNUSED unsigned int MaxRpcSize,
                                WINPR_ATTR_UNUSED RPC_IF_CALLBACK_FN* IfCallbackFn)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUnregisterIf(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                 WINPR_ATTR_UNUSED UUID* MgrTypeUuid,
                                 WINPR_ATTR_UNUSED unsigned int WaitForCallsToComplete)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUnregisterIfEx(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                   WINPR_ATTR_UNUSED UUID* MgrTypeUuid,
                                   WINPR_ATTR_UNUSED int RundownContextHandles)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqs(WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                   WINPR_ATTR_UNUSED void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsEx(WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                     WINPR_ATTR_UNUSED void* SecurityDescriptor,
                                     WINPR_ATTR_UNUSED PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsIf(WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                     WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                     WINPR_ATTR_UNUSED void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsIfEx(WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                       WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                       WINPR_ATTR_UNUSED void* SecurityDescriptor,
                                       WINPR_ATTR_UNUSED PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqA(WINPR_ATTR_UNUSED RPC_CSTR Protseq,
                                WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                WINPR_ATTR_UNUSED void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqExA(WINPR_ATTR_UNUSED RPC_CSTR Protseq,
                                  WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                  WINPR_ATTR_UNUSED void* SecurityDescriptor,
                                  WINPR_ATTR_UNUSED PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqW(WINPR_ATTR_UNUSED RPC_WSTR Protseq,
                                WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                WINPR_ATTR_UNUSED void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqExW(WINPR_ATTR_UNUSED RPC_WSTR Protseq,
                                  WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                  WINPR_ATTR_UNUSED void* SecurityDescriptor,
                                  WINPR_ATTR_UNUSED PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpA(WINPR_ATTR_UNUSED RPC_CSTR Protseq,
                                  WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                  WINPR_ATTR_UNUSED RPC_CSTR Endpoint,
                                  WINPR_ATTR_UNUSED void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpExA(WINPR_ATTR_UNUSED RPC_CSTR Protseq,
                                    WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                    WINPR_ATTR_UNUSED RPC_CSTR Endpoint,
                                    WINPR_ATTR_UNUSED void* SecurityDescriptor,
                                    WINPR_ATTR_UNUSED PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpW(WINPR_ATTR_UNUSED RPC_WSTR Protseq,
                                  WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                  WINPR_ATTR_UNUSED RPC_WSTR Endpoint,
                                  WINPR_ATTR_UNUSED void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpExW(WINPR_ATTR_UNUSED RPC_WSTR Protseq,
                                    WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                    WINPR_ATTR_UNUSED RPC_WSTR Endpoint,
                                    WINPR_ATTR_UNUSED void* SecurityDescriptor,
                                    WINPR_ATTR_UNUSED PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfA(WINPR_ATTR_UNUSED RPC_CSTR Protseq,
                                  WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                  WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                  WINPR_ATTR_UNUSED void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfExA(WINPR_ATTR_UNUSED RPC_CSTR Protseq,
                                    WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                    WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                    WINPR_ATTR_UNUSED void* SecurityDescriptor,
                                    WINPR_ATTR_UNUSED PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfW(WINPR_ATTR_UNUSED RPC_WSTR Protseq,
                                  WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                  WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                  WINPR_ATTR_UNUSED void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfExW(WINPR_ATTR_UNUSED RPC_WSTR Protseq,
                                    WINPR_ATTR_UNUSED unsigned int MaxCalls,
                                    WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                    WINPR_ATTR_UNUSED void* SecurityDescriptor,
                                    WINPR_ATTR_UNUSED PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

void RpcServerYield(void)
{
	WLog_ERR(TAG, "Not implemented");
}

RPC_STATUS RpcMgmtStatsVectorFree(WINPR_ATTR_UNUSED RPC_STATS_VECTOR** StatsVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqStats(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                           WINPR_ATTR_UNUSED RPC_STATS_VECTOR** Statistics)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtIsServerListening(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtStopServerListening(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtWaitServerListen(void)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtSetServerStackSize(WINPR_ATTR_UNUSED unsigned long ThreadStackSize)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

void RpcSsDontSerializeContext(void)
{
	WLog_ERR(TAG, "Not implemented");
}

RPC_STATUS RpcMgmtEnableIdleCleanup(void)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqIfIds(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                           WINPR_ATTR_UNUSED RPC_IF_ID_VECTOR** IfIdVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcIfIdVectorFree(WINPR_ATTR_UNUSED RPC_IF_ID_VECTOR** IfIdVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqServerPrincNameA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                      WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                      WINPR_ATTR_UNUSED RPC_CSTR* ServerPrincName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqServerPrincNameW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                      WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                      WINPR_ATTR_UNUSED RPC_WSTR* ServerPrincName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqDefaultPrincNameA(WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                         WINPR_ATTR_UNUSED RPC_CSTR* PrincName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqDefaultPrincNameW(WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                         WINPR_ATTR_UNUSED RPC_WSTR* PrincName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpResolveBinding(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                               WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNsBindingInqEntryNameA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                     WINPR_ATTR_UNUSED unsigned long EntryNameSyntax,
                                     WINPR_ATTR_UNUSED RPC_CSTR* EntryName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNsBindingInqEntryNameW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                     WINPR_ATTR_UNUSED unsigned long EntryNameSyntax,
                                     WINPR_ATTR_UNUSED RPC_WSTR* EntryName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcImpersonateClient(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE BindingHandle)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcRevertToSelfEx(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE BindingHandle)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcRevertToSelf(void)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE ClientBinding,
                                    WINPR_ATTR_UNUSED RPC_AUTHZ_HANDLE* Privs,
                                    WINPR_ATTR_UNUSED RPC_CSTR* ServerPrincName,
                                    WINPR_ATTR_UNUSED unsigned long* AuthnLevel,
                                    WINPR_ATTR_UNUSED unsigned long* AuthnSvc,
                                    WINPR_ATTR_UNUSED unsigned long* AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE ClientBinding,
                                    WINPR_ATTR_UNUSED RPC_AUTHZ_HANDLE* Privs,
                                    WINPR_ATTR_UNUSED RPC_WSTR* ServerPrincName,
                                    WINPR_ATTR_UNUSED unsigned long* AuthnLevel,
                                    WINPR_ATTR_UNUSED unsigned long* AuthnSvc,
                                    WINPR_ATTR_UNUSED unsigned long* AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientExA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE ClientBinding,
                                      WINPR_ATTR_UNUSED RPC_AUTHZ_HANDLE* Privs,
                                      WINPR_ATTR_UNUSED RPC_CSTR* ServerPrincName,
                                      WINPR_ATTR_UNUSED unsigned long* AuthnLevel,
                                      WINPR_ATTR_UNUSED unsigned long* AuthnSvc,
                                      WINPR_ATTR_UNUSED unsigned long* AuthzSvc,
                                      WINPR_ATTR_UNUSED unsigned long Flags)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientExW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE ClientBinding,
                                      WINPR_ATTR_UNUSED RPC_AUTHZ_HANDLE* Privs,
                                      WINPR_ATTR_UNUSED RPC_WSTR* ServerPrincName,
                                      WINPR_ATTR_UNUSED unsigned long* AuthnLevel,
                                      WINPR_ATTR_UNUSED unsigned long* AuthnSvc,
                                      WINPR_ATTR_UNUSED unsigned long* AuthzSvc,
                                      WINPR_ATTR_UNUSED unsigned long Flags)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                  WINPR_ATTR_UNUSED RPC_CSTR* ServerPrincName,
                                  WINPR_ATTR_UNUSED unsigned long* AuthnLevel,
                                  WINPR_ATTR_UNUSED unsigned long* AuthnSvc,
                                  WINPR_ATTR_UNUSED RPC_AUTH_IDENTITY_HANDLE* AuthIdentity,
                                  WINPR_ATTR_UNUSED unsigned long* AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                  WINPR_ATTR_UNUSED RPC_WSTR* ServerPrincName,
                                  WINPR_ATTR_UNUSED unsigned long* AuthnLevel,
                                  WINPR_ATTR_UNUSED unsigned long* AuthnSvc,
                                  WINPR_ATTR_UNUSED RPC_AUTH_IDENTITY_HANDLE* AuthIdentity,
                                  WINPR_ATTR_UNUSED unsigned long* AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                  WINPR_ATTR_UNUSED RPC_CSTR ServerPrincName,
                                  WINPR_ATTR_UNUSED unsigned long AuthnLevel,
                                  WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                  WINPR_ATTR_UNUSED RPC_AUTH_IDENTITY_HANDLE AuthIdentity,
                                  WINPR_ATTR_UNUSED unsigned long AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoExA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                    WINPR_ATTR_UNUSED RPC_CSTR ServerPrincName,
                                    WINPR_ATTR_UNUSED unsigned long AuthnLevel,
                                    WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                    WINPR_ATTR_UNUSED RPC_AUTH_IDENTITY_HANDLE AuthIdentity,
                                    WINPR_ATTR_UNUSED unsigned long AuthzSvc,
                                    WINPR_ATTR_UNUSED RPC_SECURITY_QOS* SecurityQos)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                  WINPR_ATTR_UNUSED RPC_WSTR ServerPrincName,
                                  WINPR_ATTR_UNUSED unsigned long AuthnLevel,
                                  WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                  WINPR_ATTR_UNUSED RPC_AUTH_IDENTITY_HANDLE AuthIdentity,
                                  WINPR_ATTR_UNUSED unsigned long AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoExW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                    WINPR_ATTR_UNUSED RPC_WSTR ServerPrincName,
                                    WINPR_ATTR_UNUSED unsigned long AuthnLevel,
                                    WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                    WINPR_ATTR_UNUSED RPC_AUTH_IDENTITY_HANDLE AuthIdentity,
                                    WINPR_ATTR_UNUSED unsigned long AuthzSvc,
                                    WINPR_ATTR_UNUSED RPC_SECURITY_QOS* SecurityQOS)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoExA(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                    WINPR_ATTR_UNUSED RPC_CSTR* ServerPrincName,
                                    WINPR_ATTR_UNUSED unsigned long* AuthnLevel,
                                    WINPR_ATTR_UNUSED unsigned long* AuthnSvc,
                                    WINPR_ATTR_UNUSED RPC_AUTH_IDENTITY_HANDLE* AuthIdentity,
                                    WINPR_ATTR_UNUSED unsigned long* AuthzSvc,
                                    WINPR_ATTR_UNUSED unsigned long RpcQosVersion,
                                    WINPR_ATTR_UNUSED RPC_SECURITY_QOS* SecurityQOS)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoExW(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                                    WINPR_ATTR_UNUSED RPC_WSTR* ServerPrincName,
                                    WINPR_ATTR_UNUSED unsigned long* AuthnLevel,
                                    WINPR_ATTR_UNUSED unsigned long* AuthnSvc,
                                    WINPR_ATTR_UNUSED RPC_AUTH_IDENTITY_HANDLE* AuthIdentity,
                                    WINPR_ATTR_UNUSED unsigned long* AuthzSvc,
                                    WINPR_ATTR_UNUSED unsigned long RpcQosVersion,
                                    WINPR_ATTR_UNUSED RPC_SECURITY_QOS* SecurityQOS)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterAuthInfoA(WINPR_ATTR_UNUSED RPC_CSTR ServerPrincName,
                                      WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                      WINPR_ATTR_UNUSED RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn,
                                      WINPR_ATTR_UNUSED void* Arg)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterAuthInfoW(WINPR_ATTR_UNUSED RPC_WSTR ServerPrincName,
                                      WINPR_ATTR_UNUSED unsigned long AuthnSvc,
                                      WINPR_ATTR_UNUSED RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn,
                                      WINPR_ATTR_UNUSED void* Arg)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingServerFromClient(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE ClientBinding,
                                      WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* ServerBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

void RpcRaiseException(RPC_STATUS exception)
{
	WLog_ERR(TAG, "RpcRaiseException: 0x%08lx", WINPR_CXX_COMPAT_CAST(unsigned long, exception));
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	exit((int)exception);
}

RPC_STATUS RpcTestCancel(void)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerTestCancel(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE BindingHandle)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcCancelThread(WINPR_ATTR_UNUSED void* Thread)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcCancelThreadEx(WINPR_ATTR_UNUSED void* Thread, WINPR_ATTR_UNUSED long Timeout)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

/**
 * UUID Functions
 */

static UUID UUID_NIL = {
	0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

RPC_STATUS UuidCreate(UUID* Uuid)
{
	winpr_RAND_pseudo(Uuid, 16);
	return RPC_S_OK;
}

RPC_STATUS UuidCreateSequential(UUID* Uuid)
{
	winpr_RAND_pseudo(Uuid, 16);
	return RPC_S_OK;
}

RPC_STATUS UuidToStringA(const UUID* Uuid, RPC_CSTR* StringUuid)
{
	*StringUuid = (RPC_CSTR)malloc(36 + 1);

	if (!(*StringUuid))
		return RPC_S_OUT_OF_MEMORY;

	if (!Uuid)
		Uuid = &UUID_NIL;

	/**
	 * Format is 32 hex digits partitioned in 5 groups:
	 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
	 */
	(void)sprintf_s((char*)*StringUuid, 36 + 1, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	                Uuid->Data1, Uuid->Data2, Uuid->Data3, Uuid->Data4[0], Uuid->Data4[1],
	                Uuid->Data4[2], Uuid->Data4[3], Uuid->Data4[4], Uuid->Data4[5], Uuid->Data4[6],
	                Uuid->Data4[7]);
	return RPC_S_OK;
}

RPC_STATUS UuidToStringW(WINPR_ATTR_UNUSED const UUID* Uuid, WINPR_ATTR_UNUSED RPC_WSTR* StringUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS UuidFromStringA(RPC_CSTR StringUuid, UUID* Uuid)
{
	BYTE bin[36] = { 0 };

	if (!StringUuid)
		return UuidCreateNil(Uuid);

	const size_t slen = 2 * sizeof(UUID) + 4;
	if (strnlen(StringUuid, slen) != slen)
		return RPC_S_INVALID_STRING_UUID;

	if ((StringUuid[8] != '-') || (StringUuid[13] != '-') || (StringUuid[18] != '-') ||
	    (StringUuid[23] != '-'))
	{
		return RPC_S_INVALID_STRING_UUID;
	}

	for (size_t index = 0; index < 36; index++)
	{
		if ((index == 8) || (index == 13) || (index == 18) || (index == 23))
			continue;

		if ((StringUuid[index] >= '0') && (StringUuid[index] <= '9'))
			bin[index] = (StringUuid[index] - '0') & 0xFF;
		else if ((StringUuid[index] >= 'a') && (StringUuid[index] <= 'f'))
			bin[index] = (StringUuid[index] - 'a' + 10) & 0xFF;
		else if ((StringUuid[index] >= 'A') && (StringUuid[index] <= 'F'))
			bin[index] = (StringUuid[index] - 'A' + 10) & 0xFF;
		else
			return RPC_S_INVALID_STRING_UUID;
	}

	Uuid->Data1 = (UINT32)((bin[0] << 28) | (bin[1] << 24) | (bin[2] << 20) | (bin[3] << 16) |
	                       (bin[4] << 12) | (bin[5] << 8) | (bin[6] << 4) | bin[7]);
	Uuid->Data2 = (UINT16)((bin[9] << 12) | (bin[10] << 8) | (bin[11] << 4) | bin[12]);
	Uuid->Data3 = (UINT16)((bin[14] << 12) | (bin[15] << 8) | (bin[16] << 4) | bin[17]);
	Uuid->Data4[0] = (UINT8)((bin[19] << 4) | bin[20]);
	Uuid->Data4[1] = (UINT8)((bin[21] << 4) | bin[22]);
	Uuid->Data4[2] = (UINT8)((bin[24] << 4) | bin[25]);
	Uuid->Data4[3] = (UINT8)((bin[26] << 4) | bin[27]);
	Uuid->Data4[4] = (UINT8)((bin[28] << 4) | bin[29]);
	Uuid->Data4[5] = (UINT8)((bin[30] << 4) | bin[31]);
	Uuid->Data4[6] = (UINT8)((bin[32] << 4) | bin[33]);
	Uuid->Data4[7] = (UINT8)((bin[34] << 4) | bin[35]);
	return RPC_S_OK;
}

RPC_STATUS UuidFromStringW(WINPR_ATTR_UNUSED RPC_WSTR StringUuid, WINPR_ATTR_UNUSED UUID* Uuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

signed int UuidCompare(const UUID* Uuid1, const UUID* Uuid2, RPC_STATUS* Status)
{
	*Status = RPC_S_OK;

	if (!Uuid1)
		Uuid1 = &UUID_NIL;

	if (!Uuid2)
		Uuid2 = &UUID_NIL;

	if (Uuid1->Data1 != Uuid2->Data1)
		return (Uuid1->Data1 < Uuid2->Data1) ? -1 : 1;

	if (Uuid1->Data2 != Uuid2->Data2)
		return (Uuid1->Data2 < Uuid2->Data2) ? -1 : 1;

	if (Uuid1->Data3 != Uuid2->Data3)
		return (Uuid1->Data3 < Uuid2->Data3) ? -1 : 1;

	for (int index = 0; index < 8; index++)
	{
		if (Uuid1->Data4[index] != Uuid2->Data4[index])
			return (Uuid1->Data4[index] < Uuid2->Data4[index]) ? -1 : 1;
	}

	return 0;
}

RPC_STATUS UuidCreateNil(UUID* NilUuid)
{
	CopyMemory((void*)NilUuid, (void*)&UUID_NIL, 16);
	return RPC_S_OK;
}

int UuidEqual(const UUID* Uuid1, const UUID* Uuid2, RPC_STATUS* Status)
{
	return ((UuidCompare(Uuid1, Uuid2, Status) == 0) ? TRUE : FALSE);
}

unsigned short UuidHash(WINPR_ATTR_UNUSED const UUID* Uuid, WINPR_ATTR_UNUSED RPC_STATUS* Status)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

int UuidIsNil(const UUID* Uuid, RPC_STATUS* Status)
{
	return UuidEqual(Uuid, &UUID_NIL, Status);
}

RPC_STATUS RpcEpRegisterNoReplaceA(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                   WINPR_ATTR_UNUSED RPC_BINDING_VECTOR* BindingVector,
                                   WINPR_ATTR_UNUSED UUID_VECTOR* UuidVector,
                                   WINPR_ATTR_UNUSED RPC_CSTR Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpRegisterNoReplaceW(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                                   WINPR_ATTR_UNUSED RPC_BINDING_VECTOR* BindingVector,
                                   WINPR_ATTR_UNUSED UUID_VECTOR* UuidVector,
                                   WINPR_ATTR_UNUSED RPC_WSTR Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpRegisterA(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                          WINPR_ATTR_UNUSED RPC_BINDING_VECTOR* BindingVector,
                          WINPR_ATTR_UNUSED UUID_VECTOR* UuidVector,
                          WINPR_ATTR_UNUSED RPC_CSTR Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpRegisterW(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                          WINPR_ATTR_UNUSED RPC_BINDING_VECTOR* BindingVector,
                          WINPR_ATTR_UNUSED UUID_VECTOR* UuidVector,
                          WINPR_ATTR_UNUSED RPC_WSTR Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpUnregister(WINPR_ATTR_UNUSED RPC_IF_HANDLE IfSpec,
                           WINPR_ATTR_UNUSED RPC_BINDING_VECTOR* BindingVector,
                           WINPR_ATTR_UNUSED UUID_VECTOR* UuidVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS DceErrorInqTextA(WINPR_ATTR_UNUSED RPC_STATUS RpcStatus,
                            WINPR_ATTR_UNUSED RPC_CSTR ErrorText)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS DceErrorInqTextW(WINPR_ATTR_UNUSED RPC_STATUS RpcStatus,
                            WINPR_ATTR_UNUSED RPC_WSTR ErrorText)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqBegin(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE EpBinding,
                                WINPR_ATTR_UNUSED unsigned long InquiryType,
                                WINPR_ATTR_UNUSED RPC_IF_ID* IfId,
                                WINPR_ATTR_UNUSED unsigned long VersOption,
                                WINPR_ATTR_UNUSED UUID* ObjectUuid,
                                WINPR_ATTR_UNUSED RPC_EP_INQ_HANDLE* InquiryContext)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqDone(WINPR_ATTR_UNUSED RPC_EP_INQ_HANDLE* InquiryContext)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqNextA(WINPR_ATTR_UNUSED RPC_EP_INQ_HANDLE InquiryContext,
                                WINPR_ATTR_UNUSED RPC_IF_ID* IfId,
                                WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* Binding,
                                WINPR_ATTR_UNUSED UUID* ObjectUuid,
                                WINPR_ATTR_UNUSED RPC_CSTR* Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqNextW(WINPR_ATTR_UNUSED RPC_EP_INQ_HANDLE InquiryContext,
                                WINPR_ATTR_UNUSED RPC_IF_ID* IfId,
                                WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* Binding,
                                WINPR_ATTR_UNUSED UUID* ObjectUuid,
                                WINPR_ATTR_UNUSED RPC_WSTR* Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpUnregister(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE EpBinding,
                               WINPR_ATTR_UNUSED RPC_IF_ID* IfId,
                               WINPR_ATTR_UNUSED RPC_BINDING_HANDLE Binding,
                               WINPR_ATTR_UNUSED UUID* ObjectUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtSetAuthorizationFn(WINPR_ATTR_UNUSED RPC_MGMT_AUTHORIZATION_FN AuthorizationFn)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqBindingHandle(WINPR_ATTR_UNUSED RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

#endif
