#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/path.h>
#include <winpr/wlog.h>

typedef struct
{
	UINT32 level;
	char* msg;
	char* channel;
} test_t;

static const char* function = NULL;
static const char* channels[] = { "com.test.channelA", "com.test.channelB" };

static const test_t messages[] = { { WLOG_INFO, "this is a test", "com.test.channelA" },
	                               { WLOG_INFO, "Just some info", "com.test.channelB" },
	                               { WLOG_WARN, "this is a %dnd %s", "com.test.channelA" },
	                               { WLOG_WARN, "we're warning a %dnd %s", "com.test.channelB" },
	                               { WLOG_ERROR, "this is an error", "com.test.channelA" },
	                               { WLOG_ERROR, "we've got an error", "com.test.channelB" },
	                               { WLOG_TRACE, "this is a trace output", "com.test.channelA" },
	                               { WLOG_TRACE, "leaving a trace behind", "com.test.channelB" } };

static BOOL success = TRUE;
static int pos = 0;

static BOOL check(const wLogMessage* msg)
{
	BOOL rc = TRUE;
	if (!msg)
		rc = FALSE;
	else if (strcmp(msg->FileName, __FILE__))
		rc = FALSE;
	else if (strcmp(msg->FunctionName, function))
		rc = FALSE;
	else if (strcmp(msg->PrefixString, messages[pos].channel))
		rc = FALSE;
	else if (msg->Level != messages[pos].level)
		rc = FALSE;
	else if (strcmp(msg->FormatString, messages[pos].msg))
		rc = FALSE;
	pos++;

	if (!rc)
	{
		fprintf(stderr, "Test failed!\n");
		success = FALSE;
	}
	return rc;
}

static BOOL CallbackAppenderMessage(const wLogMessage* msg)
{
	check(msg);
	return TRUE;
}

static BOOL CallbackAppenderData(const wLogMessage* msg)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	return TRUE;
}

static BOOL CallbackAppenderImage(const wLogMessage* msg)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	return TRUE;
}

static BOOL CallbackAppenderPackage(const wLogMessage* msg)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	return TRUE;
}

int TestWLogCallback(int argc, char* argv[])
{
	wLog* root;
	wLog* logA;
	wLog* logB;
	wLogLayout* layout;
	wLogAppender* appender;
	wLogCallbacks callbacks;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	function = __FUNCTION__;

	root = WLog_GetRoot();

	WLog_SetLogAppenderType(root, WLOG_APPENDER_CALLBACK);

	appender = WLog_GetLogAppender(root);

	callbacks.data = CallbackAppenderData;
	callbacks.image = CallbackAppenderImage;
	callbacks.message = CallbackAppenderMessage;
	callbacks.package = CallbackAppenderPackage;

	if (!WLog_ConfigureAppender(appender, "callbacks", (void*)&callbacks))
		return -1;

	layout = WLog_GetLogLayout(root);
	WLog_Layout_SetPrefixFormat(root, layout, "%mn");

	WLog_OpenAppender(root);

	logA = WLog_Get(channels[0]);
	logB = WLog_Get(channels[1]);

	WLog_SetLogLevel(logA, WLOG_TRACE);
	WLog_SetLogLevel(logB, WLOG_TRACE);

	WLog_Print(logA, messages[0].level, messages[0].msg);
	WLog_Print(logB, messages[1].level, messages[1].msg);
	WLog_Print(logA, messages[2].level, messages[2].msg, 2, "test");
	WLog_Print(logB, messages[3].level, messages[3].msg, 2, "time");
	WLog_Print(logA, messages[4].level, messages[4].msg);
	WLog_Print(logB, messages[5].level, messages[5].msg);
	WLog_Print(logA, messages[6].level, messages[6].msg);
	WLog_Print(logB, messages[7].level, messages[7].msg);

	WLog_CloseAppender(root);

	return success ? 0 : -1;
}
