
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/path.h>
#include <winpr/wlog.h>

int TestWLog(int argc, char* argv[])
{
	wLog* log;
	char* TempPath;
	wLogLayout* layout;
	wLogAppender* appender;

	log = WLog_New("com.log.test.console");

	WLog_SetLogLevel(log, WLOG_INFO);

	WLog_SetLogAppenderType(log, WLOG_APPENDER_CONSOLE);
	appender = WLog_GetLogAppender(log);

	layout = WLog_GetLogLayout(log);
	WLog_Layout_SetPrefixFormat(log, layout, "[%lv:%mn] [%fl|%fn|%ln] - ");

	WLog_ConsoleAppender_SetOutputStream(log, (wLogConsoleAppender*) appender, WLOG_CONSOLE_STDERR);
	WLog_OpenAppender(log);

	WLog_Print(log, WLOG_INFO, "this is a test");
	WLog_Print(log, WLOG_WARN, "this is a %dnd %s", 2, "test");
	WLog_Print(log, WLOG_ERROR, "this is an error");
	WLog_Print(log, WLOG_TRACE, "this is a trace output");

	WLog_CloseAppender(log);
	WLog_Free(log);

	log = WLog_New("com.log.test.file");

	WLog_SetLogLevel(log, WLOG_WARN);

	WLog_SetLogAppenderType(log, WLOG_APPENDER_FILE);
	appender = WLog_GetLogAppender(log);

	layout = WLog_GetLogLayout(log);
	WLog_Layout_SetPrefixFormat(log, layout, "[%lv:%mn] [%fl|%fn|%ln] - ");

	TempPath = GetKnownSubPath(KNOWN_PATH_TEMP, "wlog");

	WLog_FileAppender_SetOutputFilePath(log, (wLogFileAppender*) appender, TempPath);
	WLog_OpenAppender(log);

	free(TempPath);

	WLog_Print(log, WLOG_INFO, "this is a test");
	WLog_Print(log, WLOG_WARN, "this is a %dnd %s", 2, "test");
	WLog_Print(log, WLOG_ERROR, "this is an error");
	WLog_Print(log, WLOG_TRACE, "this is a trace output");

	WLog_CloseAppender(log);
	WLog_Free(log);

	return 0;
}
