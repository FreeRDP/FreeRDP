/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/print.h>

#include "win_wds.h"

#undef DEFINE_GUID
#define INITGUID

#include <initguid.h>

DEFINE_GUID(CLSID_RDPSession,0x9B78F0E6,0x3E05,0x4A5B,0xB2,0xE8,0xE7,0x43,0xA8,0x95,0x6B,0x65);
DEFINE_GUID(DIID__IRDPSessionEvents,0x98a97042,0x6698,0x40e9,0x8e,0xfd,0xb3,0x20,0x09,0x90,0x00,0x4b);
DEFINE_GUID(IID_IRDPSRAPISharingSession,0xeeb20886,0xe470,0x4cf6,0x84,0x2b,0x27,0x39,0xc0,0xec,0x5c,0xfb);
DEFINE_GUID(IID_IRDPSRAPIAttendeeManager,0xba3a37e8,0x33da,0x4749,0x8d,0xa0,0x07,0xfa,0x34,0xda,0x79,0x44);
DEFINE_GUID(IID_IRDPSRAPISessionProperties,0x339b24f2,0x9bc0,0x4f16,0x9a,0xac,0xf1,0x65,0x43,0x3d,0x13,0xd4);
DEFINE_GUID(CLSID_RDPSRAPIApplicationFilter,0xe35ace89,0xc7e8,0x427e,0xa4,0xf9,0xb9,0xda,0x07,0x28,0x26,0xbd);
DEFINE_GUID(CLSID_RDPSRAPIInvitationManager,0x53d9c9db,0x75ab,0x4271,0x94,0x8a,0x4c,0x4e,0xb3,0x6a,0x8f,0x2b);

static HRESULT STDMETHODCALLTYPE Shadow_IRDPSessionEvents_QueryInterface( 
            __RPC__in _IRDPSessionEvents * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject)
{
	GUID* pGuid = (GUID*) riid;

	printf("Shadow_IRDPSessionEvents_QueryInterface\n");

	printf("{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n",
		pGuid->Data1, pGuid->Data2, pGuid->Data3,
		pGuid->Data4[0], pGuid->Data4[1],
		pGuid->Data4[2], pGuid->Data4[3], pGuid->Data4[4],
		pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7]);

	*ppvObject = NULL;

	if (IsEqualIID(riid, &DIID__IRDPSessionEvents) ||
		IsEqualIID(riid, &IID_IDispatch) || IsEqualIID(riid, &IID_IUnknown))
	{
		*ppvObject = This;
	}

	if (!(*ppvObject))
		return E_NOINTERFACE;

	This->lpVtbl->AddRef(This);

	return S_OK;
}
        
static ULONG STDMETHODCALLTYPE Shadow_IRDPSessionEvents_AddRef( 
            __RPC__in _IRDPSessionEvents * This)
{
	printf("Shadow_IRDPSessionEvents_AddRef\n");
	return 1;
}
        
static ULONG STDMETHODCALLTYPE Shadow_IRDPSessionEvents_Release( 
            __RPC__in _IRDPSessionEvents * This)
{
	printf("Shadow_IRDPSessionEvents_Release\n");
	return 0;
}
        
static HRESULT STDMETHODCALLTYPE Shadow_IRDPSessionEvents_GetTypeInfoCount( 
            __RPC__in _IRDPSessionEvents * This,
            /* [out] */ __RPC__out UINT *pctinfo)
{
	printf("Shadow_IRDPSessionEvents_GetTypeInfoCount\n");
	*pctinfo = 1;
	return S_OK;
}
        
static HRESULT STDMETHODCALLTYPE Shadow_IRDPSessionEvents_GetTypeInfo( 
            __RPC__in _IRDPSessionEvents * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ __RPC__deref_out_opt ITypeInfo **ppTInfo)
{
	printf("Shadow_IRDPSessionEvents_GetTypeInfo\n");
	return E_NOTIMPL;
}
        
static HRESULT STDMETHODCALLTYPE Shadow_IRDPSessionEvents_GetIDsOfNames( 
            __RPC__in _IRDPSessionEvents * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR *rgszNames,
            /* [range][in] */ __RPC__in_range(0,16384) UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID *rgDispId)
{
	printf("Shadow_IRDPSessionEvents_GetIDsOfNames\n");
	return E_NOTIMPL;
}
        
static HRESULT STDMETHODCALLTYPE Shadow_IRDPSessionEvents_Invoke( 
            _IRDPSessionEvents * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr)
{
	printf("Shadow_IRDPSessionEvents_Invoke\n");
	return S_OK;
}

static _IRDPSessionEventsVtbl Shadow_IRDPSessionEventsVtbl =
{
	/* IUnknown */
	Shadow_IRDPSessionEvents_QueryInterface,
	Shadow_IRDPSessionEvents_AddRef,
	Shadow_IRDPSessionEvents_Release,

	/* IDispatch */
	Shadow_IRDPSessionEvents_GetTypeInfoCount,
	Shadow_IRDPSessionEvents_GetTypeInfo,
	Shadow_IRDPSessionEvents_GetIDsOfNames,
	Shadow_IRDPSessionEvents_Invoke
};

static _IRDPSessionEvents Shadow_IRDPSessionEvents =
{
	&Shadow_IRDPSessionEventsVtbl
};

int win_shadow_wds_init(winShadowSubsystem* subsystem)
{
	int status;
	HRESULT hr;
	DWORD dwCookie;
	long left, top;
	long right, bottom;
	IUnknown* pUnknown;
	BSTR bstrAuthString;
	BSTR bstrGroupName;
	BSTR bstrPassword;
	BSTR bstrConnectionString;
	BSTR bstrPropertyName;
	VARIANT varPropertyValue;
	IConnectionPoint* pCP;
	IConnectionPointContainer* pCPC;

	hr = OleInitialize(NULL);

	if (FAILED(hr))
	{
		fprintf(stderr, "OleInitialize() failure\n");
		return -1;
	}

	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (FAILED(hr))
	{
		fprintf(stderr, "CoInitialize() failure\n");
		return -1;
	}

	hr = CoCreateInstance(&CLSID_RDPSession, NULL, CLSCTX_ALL,
		&IID_IRDPSRAPISharingSession, (void**) &(subsystem->pSharingSession));

	if (FAILED(hr))
	{
		fprintf(stderr, "CoCreateInstance(IRDPSRAPISharingSession) failure: 0x%08X\n", hr);
		return -1;
	}

	pUnknown = (IUnknown*) subsystem->pSharingSession;
	hr = pUnknown->lpVtbl->QueryInterface(pUnknown, &IID_IConnectionPointContainer, (void**) &pCPC);

	if (FAILED(hr))
	{
		fprintf(stderr, "QueryInterface(IID_IConnectionPointContainer) failure: 0x%08X\n", hr);
		return -1;
	}

	pCPC->lpVtbl->FindConnectionPoint(pCPC, &DIID__IRDPSessionEvents, &pCP);

	if (FAILED(hr))
	{
		fprintf(stderr, "IConnectionPointContainer::FindConnectionPoint(_IRDPSessionEvents) failure: 0x%08X\n", hr);
		return -1;
	}

	hr = pCP->lpVtbl->Advise(pCP, (IUnknown*) &Shadow_IRDPSessionEvents, &dwCookie);

	if (FAILED(hr))
	{
		fprintf(stderr, "IConnectionPoint::Advise(Shadow_IRDPSessionEvents) failure: 0x%08X\n", hr);
		return -1;
	}

	hr = subsystem->pSharingSession->lpVtbl->put_ColorDepth(subsystem->pSharingSession, 32);

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISharingSession::put_ColorDepth() failure: 0x%08X\n", hr);
		return -1;
	}

	hr = subsystem->pSharingSession->lpVtbl->GetDesktopSharedRect(subsystem->pSharingSession,
		&left, &top, &right, &bottom);

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISharingSession::GetDesktopSharedRect() failure: 0x%08X\n", hr);
		return -1;
	}

	printf("GetDesktopSharedRect(): left: %d top: %d right: %d bottom: %d\n",
		left, top, right, bottom);

	hr = subsystem->pSharingSession->lpVtbl->get_VirtualChannelManager(subsystem->pSharingSession,
		&(subsystem->pVirtualChannelMgr));

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISharingSession::get_VirtualChannelManager() failure: 0x%08X\n", hr);
		return -1;
	}

	hr = subsystem->pSharingSession->lpVtbl->get_ApplicationFilter(subsystem->pSharingSession,
		&(subsystem->pApplicationFilter));

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISharingSession::get_ApplicationFilter() failure: 0x%08X\n", hr);
		return -1;
	}

	hr = subsystem->pSharingSession->lpVtbl->get_Attendees(subsystem->pSharingSession,
		&(subsystem->pAttendeeMgr));

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISharingSession::get_Attendees() failure: 0x%08X\n", hr);
		return -1;
	}

	hr = subsystem->pSharingSession->lpVtbl->get_Properties(subsystem->pSharingSession, &(subsystem->pSessionProperties));

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISharingSession::get_Properties() failure: 0x%08X\n", hr);
		return -1;
	}

	bstrPropertyName = SysAllocString(L"PortId");
	varPropertyValue.vt = VT_I4;
	varPropertyValue.intVal = 40000;

	hr = subsystem->pSessionProperties->lpVtbl->put_Property(subsystem->pSessionProperties,
		bstrPropertyName, varPropertyValue);

	SysFreeString(bstrPropertyName);

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISessionProperties::put_Property(PortId) failure: 0x%08X\n", hr);
		return -1;
	}

	bstrPropertyName = SysAllocString(L"DrvConAttach");
	varPropertyValue.vt = VT_BOOL;
	varPropertyValue.boolVal = VARIANT_TRUE;

	hr = subsystem->pSessionProperties->lpVtbl->put_Property(subsystem->pSessionProperties,
		bstrPropertyName, varPropertyValue);

	SysFreeString(bstrPropertyName);

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISessionProperties::put_Property(DrvConAttach) failure: 0x%08X\n", hr);
		return -1;
	}

	bstrPropertyName = SysAllocString(L"PortProtocol");
	varPropertyValue.vt = VT_I4;

	//varPropertyValue.intVal = 0; // AF_UNSPEC
	varPropertyValue.intVal = 2; // AF_INET
	//varPropertyValue.intVal = 23; // AF_INET6

	hr = subsystem->pSessionProperties->lpVtbl->put_Property(subsystem->pSessionProperties,
		bstrPropertyName, varPropertyValue);

	SysFreeString(bstrPropertyName);

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISessionProperties::put_Property(PortProtocol) failure: 0x%08X\n", hr);
		return -1;
	}

	hr = subsystem->pSharingSession->lpVtbl->Open(subsystem->pSharingSession);

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISharingSession::Open() failure: 0x%08X\n", hr);
		return -1;
	}

	hr = subsystem->pSharingSession->lpVtbl->get_Invitations(subsystem->pSharingSession,
		&(subsystem->pInvitationMgr));

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPISharingSession::get_Invitations() failure\n");
		return -1;
	}

	bstrAuthString = SysAllocString(L"Shadow");
	bstrGroupName = SysAllocString(L"ShadowGroup");
	bstrPassword = SysAllocString(L"Shadow123!");

	hr = subsystem->pInvitationMgr->lpVtbl->CreateInvitation(subsystem->pInvitationMgr, bstrAuthString,
		bstrGroupName, bstrPassword, 5, &(subsystem->pInvitation));

	SysFreeString(bstrAuthString);
	SysFreeString(bstrGroupName);
	SysFreeString(bstrPassword);

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPIInvitationManager::CreateInvitation() failure: 0x%08X\n", hr);
		return -1;
	}

	subsystem->pInvitation->lpVtbl->get_ConnectionString(subsystem->pInvitation, &bstrConnectionString);

	if (FAILED(hr))
	{
		fprintf(stderr, "IRDPSRAPIInvitation::get_ConnectionString() failure: 0x%08X\n", hr);
		return -1;
	}

	subsystem->pAssistanceFile = freerdp_assistance_file_new();

	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) bstrConnectionString, ((UINT32*) bstrConnectionString)[-1],
		&(subsystem->pAssistanceFile->ConnectionString2), 0, NULL, NULL);

	status = freerdp_assistance_parse_connection_string2(subsystem->pAssistanceFile);

	if (status < 0)
		return -1;

	printf("ConnectionString: %s\n", subsystem->pAssistanceFile->ConnectionString2);

	printf("RemoteAssistanceSessionId: %s\n", subsystem->pAssistanceFile->RASessionId);
	printf("RemoteAssistanceRCTicket: %s\n", subsystem->pAssistanceFile->RCTicket);
	printf("RemoteAssistancePassStub: %s\n", subsystem->pAssistanceFile->PassStub);
	printf("RemoteAssistanceMachineAddress: %s\n", subsystem->pAssistanceFile->MachineAddress);
	printf("RemoteAssistanceMachinePort: %s\n", subsystem->pAssistanceFile->MachinePort);

	if (1)
	{
		FILE* fp;
		size_t size;

		fp = fopen("inv.xml", "w+b");

		if (fp)
		{
			size = strlen(subsystem->pAssistanceFile->ConnectionString2);
			fwrite(subsystem->pAssistanceFile->ConnectionString2, 1, size, fp);
			fwrite("\r\n", 1, 2, fp);
			fclose(fp);
		}
	}

	return 1;
}

int win_shadow_wds_uninit(winShadowSubsystem* subsystem)
{
	printf("win_shadow_wds_uninit\n");

	if (subsystem->pSharingSession)
	{
		subsystem->pSharingSession->lpVtbl->Close(subsystem->pSharingSession);
		subsystem->pSharingSession->lpVtbl->Release(subsystem->pSharingSession);
		subsystem->pSharingSession = NULL;
	}

	if (subsystem->pVirtualChannelMgr)
	{
		subsystem->pVirtualChannelMgr->lpVtbl->Release(subsystem->pVirtualChannelMgr);
		subsystem->pVirtualChannelMgr = NULL;
	}

	if (subsystem->pApplicationFilter)
	{
		subsystem->pApplicationFilter->lpVtbl->Release(subsystem->pApplicationFilter);
		subsystem->pApplicationFilter = NULL;
	}

	if (subsystem->pAttendeeMgr)
	{
		subsystem->pAttendeeMgr->lpVtbl->Release(subsystem->pAttendeeMgr);
		subsystem->pAttendeeMgr = NULL;
	}

	if (subsystem->pSessionProperties)
	{
		subsystem->pSessionProperties->lpVtbl->Release(subsystem->pSessionProperties);
		subsystem->pSessionProperties = NULL;
	}

	if (subsystem->pInvitationMgr)
	{
		subsystem->pInvitationMgr->lpVtbl->Release(subsystem->pInvitationMgr);
		subsystem->pInvitationMgr = NULL;
	}

	if (subsystem->pInvitation)
	{
		subsystem->pInvitation->lpVtbl->Release(subsystem->pInvitation);
		subsystem->pInvitation = NULL;
	}

	if (subsystem->pAssistanceFile)
	{
		freerdp_assistance_file_free(subsystem->pAssistanceFile);
		subsystem->pAssistanceFile = NULL;
	}

	return 1;
}
