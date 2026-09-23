#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/wlog.h>

int TestWLog(int argc, char* argv[])
{
	wLog* root = nullptr;
	wLog* logA = nullptr;
	wLog* logB = nullptr;
	wLogLayout* layout = nullptr;
	wLogAppender* appender = nullptr;
	char* tmp_path = nullptr;
	char* wlog_file = nullptr;
	int result = 1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!(tmp_path = GetKnownPath(KNOWN_PATH_TEMP)))
	{
		(void)fprintf(stderr, "Failed to get temporary directory!\n");
		goto out;
	}

	root = WLog_GetRoot();
	WINPR_ASSERT(root);

	if (!WLog_SetLogAppenderType(root, WLOG_APPENDER_BINARY))
		goto out;

	appender = WLog_GetLogAppender(root);
	if (!WLog_ConfigureAppender(appender, "outputfilename", "test_w.log"))
		goto out;
	if (!WLog_ConfigureAppender(appender, "outputfilepath", tmp_path))
		goto out;

	layout = WLog_GetLogLayout(root);
	if (!WLog_Layout_SetPrefixFormat(root, layout, "[%lv:%mn] [%fl|%fn|%ln] - "))
		goto out;

	if (!WLog_OpenAppender(root))
		goto out;

	logA = WLog_Get("com.test.ChannelA");
	WINPR_ASSERT(logA);

	logB = WLog_Get("com.test.ChannelB");
	WINPR_ASSERT(logB);

	if (!WLog_SetLogLevel(logA, WLOG_INFO))
		goto out;
	if (!WLog_SetLogLevel(logB, WLOG_ERROR))
		goto out;

	WLog_Print(logA, WLOG_INFO, "this is a test");
	WLog_Print(logA, WLOG_WARN, "this is a %dnd %s", 2, "test");
	WLog_Print(logA, WLOG_ERROR, "this is an error");
	WLog_Print(logA, WLOG_TRACE, "this is a trace output");

	WLog_Print(logB, WLOG_INFO, "just some info");
	WLog_Print(logB, WLOG_WARN, "we're warning a %dnd %s", 2, "time");
	WLog_Print(logB, WLOG_ERROR, "we've got an error");
	WLog_Print(logB, WLOG_TRACE, "leaving a trace behind");

	if (!WLog_CloseAppender(root))
		goto out;

	if ((wlog_file = GetCombinedPath(tmp_path, "test_w.log")))
		winpr_DeleteFile(wlog_file);

	result = 0;
out:
	free(wlog_file);
	free(tmp_path);

	return result;
}
