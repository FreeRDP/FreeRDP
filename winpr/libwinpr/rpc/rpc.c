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

#include <winpr/crt.h>
#include <winpr/rpc.h>
#include <winpr/crypto.h>

#if !defined(_WIN32) || defined(_UWP)

#include "../log.h"
#define TAG WINPR_TAG("rpc")

RPC_STATUS RpcBindingCopy(RPC_BINDING_HANDLE SourceBinding, RPC_BINDING_HANDLE* DestinationBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingFree(RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetOption(RPC_BINDING_HANDLE hBinding, unsigned long option, ULONG_PTR optionValue)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqOption(RPC_BINDING_HANDLE hBinding, unsigned long option, ULONG_PTR* pOptionValue)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingFromStringBindingA(RPC_CSTR StringBinding, RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingFromStringBindingW(RPC_WSTR StringBinding, RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcSsGetContextBinding(void* ContextHandle, RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqObject(RPC_BINDING_HANDLE Binding, UUID* ObjectUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingReset(RPC_BINDING_HANDLE Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetObject(RPC_BINDING_HANDLE Binding, UUID* ObjectUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqDefaultProtectLevel(unsigned long AuthnSvc, unsigned long* AuthnLevel)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingToStringBindingA(RPC_BINDING_HANDLE Binding, RPC_CSTR* StringBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingToStringBindingW(RPC_BINDING_HANDLE Binding, RPC_WSTR* StringBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingVectorFree(RPC_BINDING_VECTOR** BindingVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringBindingComposeA(RPC_CSTR ObjUuid, RPC_CSTR Protseq, RPC_CSTR NetworkAddr,
									RPC_CSTR Endpoint, RPC_CSTR Options, RPC_CSTR* StringBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringBindingComposeW(RPC_WSTR ObjUuid, RPC_WSTR Protseq, RPC_WSTR NetworkAddr,
									RPC_WSTR Endpoint, RPC_WSTR Options, RPC_WSTR* StringBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringBindingParseA(RPC_CSTR StringBinding, RPC_CSTR* ObjUuid, RPC_CSTR* Protseq,
								  RPC_CSTR* NetworkAddr, RPC_CSTR* Endpoint, RPC_CSTR* NetworkOptions)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcStringBindingParseW(RPC_WSTR StringBinding, RPC_WSTR* ObjUuid, RPC_WSTR* Protseq,
								  RPC_WSTR* NetworkAddr, RPC_WSTR* Endpoint, RPC_WSTR* NetworkOptions)
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

RPC_STATUS RpcIfInqId(RPC_IF_HANDLE RpcIfHandle, RPC_IF_ID* RpcIfId)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNetworkIsProtseqValidA(RPC_CSTR Protseq)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNetworkIsProtseqValidW(RPC_WSTR Protseq)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqComTimeout(RPC_BINDING_HANDLE Binding, unsigned int* Timeout)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtSetComTimeout(RPC_BINDING_HANDLE Binding, unsigned int Timeout)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtSetCancelTimeout(long Timeout)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNetworkInqProtseqsA(RPC_PROTSEQ_VECTORA** ProtseqVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNetworkInqProtseqsW(RPC_PROTSEQ_VECTORW** ProtseqVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcObjectInqType(UUID* ObjUuid, UUID* TypeUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcObjectSetInqFn(RPC_OBJECT_INQ_FN* InquiryFn)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcObjectSetType(UUID* ObjUuid, UUID* TypeUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcProtseqVectorFreeA(RPC_PROTSEQ_VECTORA** ProtseqVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcProtseqVectorFreeW(RPC_PROTSEQ_VECTORW** ProtseqVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqBindings(RPC_BINDING_VECTOR** BindingVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV** MgrEpv)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerListen(unsigned int MinimumCallThreads, unsigned int MaxCalls, unsigned int DontWait)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterIfEx(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv,
								 unsigned int Flags, unsigned int MaxCalls, RPC_IF_CALLBACK_FN* IfCallback)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterIf2(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv,
								unsigned int Flags, unsigned int MaxCalls, unsigned int MaxRpcSize, RPC_IF_CALLBACK_FN* IfCallbackFn)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUnregisterIf(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, unsigned int WaitForCallsToComplete)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUnregisterIfEx(RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, int RundownContextHandles)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqs(unsigned int MaxCalls, void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsEx(unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsIf(unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseAllProtseqsIfEx(unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqA(RPC_CSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqExA(RPC_CSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqW(RPC_WSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqExW(RPC_WSTR Protseq, unsigned int MaxCalls, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_CSTR Endpoint, void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpExA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_CSTR Endpoint, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_WSTR Endpoint, void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqEpExW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_WSTR Endpoint, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfExA(RPC_CSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerUseProtseqIfExW(RPC_WSTR Protseq, unsigned int MaxCalls, RPC_IF_HANDLE IfSpec, void* SecurityDescriptor, PRPC_POLICY Policy)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

void RpcServerYield()
{
	WLog_ERR(TAG, "Not implemented");
}

RPC_STATUS RpcMgmtStatsVectorFree(RPC_STATS_VECTOR** StatsVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqStats(RPC_BINDING_HANDLE Binding, RPC_STATS_VECTOR** Statistics)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtIsServerListening(RPC_BINDING_HANDLE Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtStopServerListening(RPC_BINDING_HANDLE Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtWaitServerListen(void)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtSetServerStackSize(unsigned long ThreadStackSize)
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

RPC_STATUS RpcMgmtInqIfIds(RPC_BINDING_HANDLE Binding, RPC_IF_ID_VECTOR** IfIdVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcIfIdVectorFree(RPC_IF_ID_VECTOR** IfIdVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqServerPrincNameA(RPC_BINDING_HANDLE Binding, unsigned long AuthnSvc, RPC_CSTR* ServerPrincName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtInqServerPrincNameW(RPC_BINDING_HANDLE Binding, unsigned long AuthnSvc, RPC_WSTR* ServerPrincName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqDefaultPrincNameA(unsigned long AuthnSvc, RPC_CSTR* PrincName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerInqDefaultPrincNameW(unsigned long AuthnSvc, RPC_WSTR* PrincName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpResolveBinding(RPC_BINDING_HANDLE Binding, RPC_IF_HANDLE IfSpec)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNsBindingInqEntryNameA(RPC_BINDING_HANDLE Binding, unsigned long EntryNameSyntax, RPC_CSTR* EntryName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcNsBindingInqEntryNameW(RPC_BINDING_HANDLE Binding, unsigned long EntryNameSyntax, RPC_WSTR* EntryName)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}


RPC_STATUS RpcImpersonateClient(RPC_BINDING_HANDLE BindingHandle)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcRevertToSelfEx(RPC_BINDING_HANDLE BindingHandle)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcRevertToSelf()
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientA(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
									RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientW(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
									RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientExA(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
									  RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc, unsigned long Flags)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthClientExW(RPC_BINDING_HANDLE ClientBinding, RPC_AUTHZ_HANDLE* Privs,
									  RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel, unsigned long* AuthnSvc, unsigned long* AuthzSvc, unsigned long Flags)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoA(RPC_BINDING_HANDLE Binding, RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel,
								  unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoW(RPC_BINDING_HANDLE Binding, RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel,
								  unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoA(RPC_BINDING_HANDLE Binding, RPC_CSTR ServerPrincName, unsigned long AuthnLevel,
								  unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoExA(RPC_BINDING_HANDLE Binding, RPC_CSTR ServerPrincName, unsigned long AuthnLevel,
									unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc, RPC_SECURITY_QOS* SecurityQos)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoW(RPC_BINDING_HANDLE Binding, RPC_WSTR ServerPrincName, unsigned long AuthnLevel,
								  unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingSetAuthInfoExW(RPC_BINDING_HANDLE Binding, RPC_WSTR ServerPrincName, unsigned long AuthnLevel,
									unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvc, RPC_SECURITY_QOS* SecurityQOS)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoExA(RPC_BINDING_HANDLE Binding, RPC_CSTR* ServerPrincName, unsigned long* AuthnLevel,
									unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc,
									unsigned long RpcQosVersion, RPC_SECURITY_QOS* SecurityQOS)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcBindingInqAuthInfoExW(RPC_BINDING_HANDLE Binding, RPC_WSTR* ServerPrincName, unsigned long* AuthnLevel,
									unsigned long* AuthnSvc, RPC_AUTH_IDENTITY_HANDLE* AuthIdentity, unsigned long* AuthzSvc,
									unsigned long RpcQosVersion, RPC_SECURITY_QOS* SecurityQOS)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}


RPC_STATUS RpcServerRegisterAuthInfoA(RPC_CSTR ServerPrincName, unsigned long AuthnSvc, RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn, void* Arg)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerRegisterAuthInfoW(RPC_WSTR ServerPrincName, unsigned long AuthnSvc, RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn, void* Arg)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}


RPC_STATUS RpcBindingServerFromClient(RPC_BINDING_HANDLE ClientBinding, RPC_BINDING_HANDLE* ServerBinding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

void RpcRaiseException(RPC_STATUS exception)
{
	WLog_ERR(TAG, "RpcRaiseException: 0x%08luX", exception);
	exit((int) exception);
}

RPC_STATUS RpcTestCancel()
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcServerTestCancel(RPC_BINDING_HANDLE BindingHandle)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcCancelThread(void* Thread)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcCancelThreadEx(void* Thread, long Timeout)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

/**
 * UUID Functions
 */

static UUID UUID_NIL =
{
	0x00000000, 0x0000, 0x0000,
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

RPC_STATUS UuidCreate(UUID* Uuid)
{
	winpr_RAND_pseudo((BYTE*) Uuid, 16);
	return RPC_S_OK;
}

RPC_STATUS UuidCreateSequential(UUID* Uuid)
{
	winpr_RAND_pseudo((BYTE*) Uuid, 16);
	return RPC_S_OK;
}

RPC_STATUS UuidToStringA(UUID* Uuid, RPC_CSTR* StringUuid)
{
	*StringUuid = (RPC_CSTR) malloc(36 + 1);

	if (!(*StringUuid))
		return RPC_S_OUT_OF_MEMORY;

	if (!Uuid)
		Uuid = &UUID_NIL;

	/**
	 * Format is 32 hex digits partitioned in 5 groups:
	 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
	 */
	sprintf_s((char*) *StringUuid, 36 + 1, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			  Uuid->Data1, Uuid->Data2, Uuid->Data3,
			  Uuid->Data4[0], Uuid->Data4[1],
			  Uuid->Data4[2], Uuid->Data4[3], Uuid->Data4[4],
			  Uuid->Data4[5], Uuid->Data4[6], Uuid->Data4[7]);
	return RPC_S_OK;
}

RPC_STATUS UuidToStringW(UUID* Uuid, RPC_WSTR* StringUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS UuidFromStringA(RPC_CSTR StringUuid, UUID* Uuid)
{
	int index;
	BYTE bin[36];

	if (!StringUuid)
		return UuidCreateNil(Uuid);

	if (strlen((char*) StringUuid) != 36)
		return RPC_S_INVALID_STRING_UUID;

	if ((StringUuid[8] != '-') || (StringUuid[13] != '-') ||
			(StringUuid[18] != '-') || (StringUuid[23] != '-'))
	{
		return RPC_S_INVALID_STRING_UUID;
	}

	for (index = 0; index < 36; index++)
	{
		if ((index == 8) || (index == 13) || (index == 18) || (index == 23))
			continue;

		if ((StringUuid[index] >= '0') && (StringUuid[index] <= '9'))
			bin[index] = StringUuid[index] - '0';
		else if ((StringUuid[index] >= 'a') && (StringUuid[index] <= 'f'))
			bin[index] = StringUuid[index] - 'a' + 10;
		else if ((StringUuid[index] >= 'A') && (StringUuid[index] <= 'F'))
			bin[index] = StringUuid[index] - 'A' + 10;
		else
			return RPC_S_INVALID_STRING_UUID;
	}

	Uuid->Data1 = ((bin[0] << 28) | (bin[1] << 24) | (bin[2] << 20) | (bin[3] << 16) |
				   (bin[4] << 12) | (bin[5] << 8) | (bin[6] << 4) | bin[7]);
	Uuid->Data2 = ((bin[9] << 12) | (bin[10] << 8) | (bin[11] << 4) | bin[12]);
	Uuid->Data3 = ((bin[14] << 12) | (bin[15] << 8) | (bin[16] << 4) | bin[17]);
	Uuid->Data4[0] = ((bin[19] << 4) | bin[20]);
	Uuid->Data4[1] = ((bin[21] << 4) | bin[22]);
	Uuid->Data4[2] = ((bin[24] << 4) | bin[25]);
	Uuid->Data4[3] = ((bin[26] << 4) | bin[27]);
	Uuid->Data4[4] = ((bin[28] << 4) | bin[29]);
	Uuid->Data4[5] = ((bin[30] << 4) | bin[31]);
	Uuid->Data4[6] = ((bin[32] << 4) | bin[33]);
	Uuid->Data4[7] = ((bin[34] << 4) | bin[35]);
	return RPC_S_OK;
}

RPC_STATUS UuidFromStringW(RPC_WSTR StringUuid, UUID* Uuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

signed int UuidCompare(UUID* Uuid1, UUID* Uuid2, RPC_STATUS* Status)
{
	int index;
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

	for (index = 0; index < 8; index++)
	{
		if (Uuid1->Data4[index] != Uuid2->Data4[index])
			return (Uuid1->Data4[index] < Uuid2->Data4[index]) ? -1 : 1;
	}

	return 0;
}

RPC_STATUS UuidCreateNil(UUID* NilUuid)
{
	CopyMemory((void*) NilUuid, (void*) &UUID_NIL, 16);
	return RPC_S_OK;
}

int UuidEqual(UUID* Uuid1, UUID* Uuid2, RPC_STATUS* Status)
{
	return ((UuidCompare(Uuid1, Uuid2, Status) == 0) ? TRUE : FALSE);
}

unsigned short UuidHash(UUID* Uuid, RPC_STATUS* Status)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

int UuidIsNil(UUID* Uuid, RPC_STATUS* Status)
{
	return UuidEqual(Uuid, &UUID_NIL, Status);
}

RPC_STATUS RpcEpRegisterNoReplaceA(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_CSTR Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpRegisterNoReplaceW(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_WSTR Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpRegisterA(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_CSTR Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpRegisterW(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector, RPC_WSTR Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcEpUnregister(RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector, UUID_VECTOR* UuidVector)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}


RPC_STATUS DceErrorInqTextA(RPC_STATUS RpcStatus, RPC_CSTR ErrorText)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS DceErrorInqTextW(RPC_STATUS RpcStatus, RPC_WSTR ErrorText)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}


RPC_STATUS RpcMgmtEpEltInqBegin(RPC_BINDING_HANDLE EpBinding, unsigned long InquiryType, RPC_IF_ID* IfId,
								unsigned long VersOption, UUID* ObjectUuid, RPC_EP_INQ_HANDLE* InquiryContext)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqDone(RPC_EP_INQ_HANDLE* InquiryContext)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqNextA(RPC_EP_INQ_HANDLE InquiryContext, RPC_IF_ID* IfId,
								RPC_BINDING_HANDLE* Binding, UUID* ObjectUuid, RPC_CSTR* Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpEltInqNextW(RPC_EP_INQ_HANDLE InquiryContext, RPC_IF_ID* IfId,
								RPC_BINDING_HANDLE* Binding, UUID* ObjectUuid, RPC_WSTR* Annotation)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtEpUnregister(RPC_BINDING_HANDLE EpBinding, RPC_IF_ID* IfId,
							   RPC_BINDING_HANDLE Binding, UUID* ObjectUuid)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

RPC_STATUS RpcMgmtSetAuthorizationFn(RPC_MGMT_AUTHORIZATION_FN AuthorizationFn)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}


RPC_STATUS RpcServerInqBindingHandle(RPC_BINDING_HANDLE* Binding)
{
	WLog_ERR(TAG, "Not implemented");
	return 0;
}

#endif
