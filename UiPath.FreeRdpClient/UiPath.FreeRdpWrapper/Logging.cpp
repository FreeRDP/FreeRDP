#include "pch.h"
#include "Logging.h"
#include "winpr/wlog.h"

namespace Logging
{
	static pRegisterThreadScopeCallback _registerThreadScopeCallback = nullptr;
	static pLogCallback _clientLogCallback = nullptr;
	static wLogCallbacks _wlogCallbacks = { 0 };
	static char _defaultCategory[] = "UiPath.FreeRdpWrapper";

	BOOL wLog_Message(const wLogMessage* msg)
	{
		wchar_t wbuffer[MAX_TRACE_MSG];
		mbstowcs_s(nullptr, wbuffer, msg->TextString, MAX_TRACE_MSG);

		_clientLogCallback(msg->PrefixString, msg->Level, wbuffer);
		return true;
	}

	HRESULT STDAPICALLTYPE InitializeLogging(
	    pLogCallback logCallback,
		pRegisterThreadScopeCallback registerThreadScopeCallback,
	    BOOL forwardFreeRdpLogs
	)
	{
		_clientLogCallback = logCallback;
		_registerThreadScopeCallback = registerThreadScopeCallback;
		if (!_clientLogCallback)
		{
			return S_OK;
		}

		wLog* log = WLog_GetRoot();
		if (!forwardFreeRdpLogs)
		{
			log = WLog_Get(_defaultCategory);
		}

		WLog_SetLogAppenderType(log, WLOG_APPENDER_CALLBACK);
		auto appender = WLog_GetLogAppender(log);
		_wlogCallbacks.message = wLog_Message;
		WLog_ConfigureAppender(appender, "callbacks", &_wlogCallbacks);
		auto layout = WLog_GetLogLayout(log);
		WLog_Layout_SetPrefixFormat(log, layout, "%mn");
		WLog_SetLogLevel(log, WLOG_INFO);

		auto negoLog = WLog_Get("com.freerdp.core.nego");
		WLog_SetLogLevel(negoLog, WLOG_TRACE);
		DT_TRACE(L"Native logging forwarding initialized. (forwardFreeRdpLogs:%s)", forwardFreeRdpLogs ? L"true" : L"false");
		return S_OK;
	}

	void Log(DWORD level, const wchar_t* fmt, ...)
	{
		if (!_clientLogCallback)
			return;

		va_list args;
		va_start(args, fmt);
		wchar_t wBuffer[MAX_TRACE_MSG];
		vswprintf(wBuffer, _countof(wBuffer), fmt, args);
		va_end(args);
		_clientLogCallback(_defaultCategory, level, wBuffer);
	}
	
	void RegisterCurrentThreadScope(char* scope)
	{
		if (!_registerThreadScopeCallback)
			return;
		
		_registerThreadScopeCallback(scope);
	}
}