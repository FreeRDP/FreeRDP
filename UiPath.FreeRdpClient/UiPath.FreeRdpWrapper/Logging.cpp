#include "pch.h"
#include "Logging.h"

namespace Logging
{
	static pLogCallback _logCallback;

	HRESULT STDAPICALLTYPE InitializeLogging(pLogCallback logCallback)
	{
		_logCallback = logCallback;
		return S_OK;
	}

	void Log(LogLevel level, const wchar_t* fmt, ...)
	{
		if (!_logCallback)
			return;
		va_list args;
		va_start(args, fmt);
		wchar_t buffer[MAX_TRACE_MSG];
		vswprintf(buffer, _countof(buffer), fmt, args);
		_logCallback(level, buffer);
		va_end(args);
	}	
}