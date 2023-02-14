#pragma once

#include "pch.h"

namespace FreeRdpClient
{
	typedef struct
	{
		long Width;
		long Height;
		long Depth;
		BOOL FontSmoothing;
		BSTR User;
		BSTR Domain;
		BSTR Pass;
		BSTR ClientName;
		BSTR HostName;
		long Port;
	} ConnectOptions;

	EXTERN_C __declspec(dllexport) HRESULT STDAPICALLTYPE
	    RdpLogon(ConnectOptions* rdpOptions, BSTR& releaseEventName);
	EXTERN_C __declspec(dllexport) HRESULT STDAPICALLTYPE RdpRelease(BSTR releaseEventName);
}