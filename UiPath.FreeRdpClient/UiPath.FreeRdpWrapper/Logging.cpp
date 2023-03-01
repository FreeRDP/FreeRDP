#include "pch.h"
#include "Logging.h"
#include "winpr/wlog.h"

namespace Logging
{
	static pRegisterThreadScopeCallback _registerThreadScopeCallback;
	static pLogCallback _clientLogCallback;
	static wLogCallbacks _wlogCallbacks = { 0 };
	static char defaultCategory[] = "UiPath.FreeRdpWrapper";

	BOOL wLog_Message(const wLogMessage* msg)
	{
		wchar_t wbuffer[MAX_TRACE_MSG];
		mbstowcs_s(nullptr, wbuffer, msg->TextString, MAX_TRACE_MSG);

		_clientLogCallback(msg->PrefixString, msg->Level, wbuffer);
		return true;
	}

	HRESULT STDAPICALLTYPE InitializeLogging(
	    pLogCallback logCallback,
		pRegisterThreadScopeCallback registerThreadScopeCallback
	)
	{
		_clientLogCallback = logCallback;
		_registerThreadScopeCallback = registerThreadScopeCallback;
		if (!_clientLogCallback)
		{
			return S_OK;
		}

		wLog* log = WLog_GetRoot();
		WLog_SetLogAppenderType(log, WLOG_APPENDER_CALLBACK);
		auto appender = WLog_GetLogAppender(log);
		_wlogCallbacks.message = wLog_Message;
		WLog_ConfigureAppender(appender, "callbacks", &_wlogCallbacks);
		auto layout = WLog_GetLogLayout(log);
		WLog_Layout_SetPrefixFormat(log, layout, "%mn");
		WLog_SetLogLevel(log, WLOG_TRACE);

		return S_OK;
	}

	void Log(DWORD level, const wchar_t* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		wchar_t wBuffer[MAX_TRACE_MSG];
		vswprintf(wBuffer, _countof(wBuffer), fmt, args);
		_clientLogCallback(defaultCategory, level, wBuffer);
		va_end(args);
	}
	
	void RegisterCurrentThreadScope(char* scope)
	{
		_registerThreadScopeCallback(scope);
	}
}