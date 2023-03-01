#pragma once

#include "pch.h"
#include "winpr/wlog.h"

namespace Logging
{

#define MAX_TRACE_MSG 2048
#define DT_ERROR(format, ...) Logging::Log(WLOG_ERROR, format, __VA_ARGS__)
#define DT_TRACE(format, ...) Logging::Log(WLOG_TRACE, format, __VA_ARGS__)

#define CHECK_HRESULT_RET_HR(Stmt)													  \
	{                                                                                 \
		HRESULT hrTmp = Stmt;                                                         \
		if (FAILED(hrTmp))                                                            \
		{                                                                             \
			DT_ERROR(L"%S:%d: error: %u [%x]", __FUNCTION__, __LINE__, hrTmp, hrTmp); \
			return hrTmp;                                                             \
		}                                                                             \
	}

	using pLogCallback = void (*)(char* category, DWORD errorLevel, wchar_t* message);
	using pRegisterThreadScopeCallback = void (*)(char* category);

	EXTERN_C __declspec(dllexport) HRESULT STDAPICALLTYPE
	    InitializeLogging(
			pLogCallback logCallback, 
			pRegisterThreadScopeCallback registerThreadScopeCallback 
		);

	void Log(DWORD level, const wchar_t* fmt, ...);
	void RegisterCurrentThreadScope(char* scope);
}