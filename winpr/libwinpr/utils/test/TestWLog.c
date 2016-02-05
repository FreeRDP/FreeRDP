
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/path.h>
#include <winpr/wlog.h>

int TestWLog(int argc, char* argv[])
{
	wLog* root;
	wLog* logA;
	wLog* logB;
	wLogLayout* layout;
	wLogAppender* appender;

	WLog_Init();

	root = WLog_GetRoot();

	WLog_SetLogAppenderType(root, WLOG_APPENDER_BINARY);

	appender = WLog_GetLogAppender(root);
	if(!WLog_ConfigureAppender(appender, "outputfilename", "test_w.log"))
		return 1;
	if(!WLog_ConfigureAppender(appender, "outputfilepath", "/tmp/"))
		return 1;

	layout = WLog_GetLogLayout(root);
	WLog_Layout_SetPrefixFormat(root, layout, "[%lv:%mn] [%fl|%fn|%ln] - ");

	WLog_OpenAppender(root);

	logA = WLog_Get("com.test.ChannelA");
	logB = WLog_Get("com.test.ChannelB");

	WLog_SetLogLevel(logA, WLOG_INFO);
	WLog_SetLogLevel(logB, WLOG_ERROR);

	WLog_Print(logA, WLOG_INFO, "this is a test");
	WLog_Print(logA, WLOG_WARN, "this is a %dnd %s", 2, "test");
	WLog_Print(logA, WLOG_ERROR, "this is an error");
	WLog_Print(logA, WLOG_TRACE, "this is a trace output");

	WLog_Print(logB, WLOG_INFO, "just some info");
	WLog_Print(logB, WLOG_WARN, "we're warning a %dnd %s", 2, "time");
	WLog_Print(logB, WLOG_ERROR, "we've got an error");
	WLog_Print(logB, WLOG_TRACE, "leaving a trace behind");

	WLog_CloseAppender(root);

	WLog_Uninit();

	return 0;
}
