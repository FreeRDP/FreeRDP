#include <stdio.h>
#include <string.h>

#include <winpr/wlog.h>

/* Every WLog_* call writes under its own tag, also when one file logs under several tags: the
 * logger a call looks up must not be shared with the other calls of its file. */

static const char* expected[] = { "com.test.tags.first", "com.test.tags.second",
	                              "com.test.tags.second", "com.test.tags.first" };
static size_t received = 0;
static BOOL success = TRUE;

static BOOL message(const wLogMessage* msg)
{
	if (!msg || (received >= ARRAYSIZE(expected)))
	{
		success = FALSE;
		return TRUE;
	}
	if (strcmp(msg->PrefixString, expected[received]) != 0)
	{
		(void)fprintf(stderr, "message %" PRIuz " '%s' came under %s, expected %s\n", received,
		              msg->FormatString, msg->PrefixString, expected[received]);
		success = FALSE;
	}
	received++;
	return TRUE;
}

static BOOL ignore(WINPR_ATTR_UNUSED const wLogMessage* msg)
{
	return TRUE;
}

int TestWLogTags(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	wLog* root = WLog_GetRoot();
	if (!WLog_SetLogAppenderType(root, WLOG_APPENDER_CALLBACK))
		return -1;

	wLogAppender* appender = WLog_GetLogAppender(root);
	if (!appender)
		return -1;

	wLogCallbacks callbacks = {
		.data = ignore, .image = ignore, .message = message, .package = ignore
	};
	if (!WLog_ConfigureAppender(appender, "callbacks", (void*)&callbacks))
		return -1;

	wLogLayout* layout = WLog_GetLogLayout(root);
	if (!WLog_Layout_SetPrefixFormat(root, layout, "%mn"))
		return -1;
	if (!WLog_OpenAppender(root))
		return -1;

	for (size_t x = 0; x < ARRAYSIZE(expected); x++)
	{
		if (!WLog_SetLogLevel(WLog_Get(expected[x]), WLOG_TRACE))
			return -1;
	}

	WLog_INFO("com.test.tags.first", "first");
	WLog_INFO("com.test.tags.second", "second");
	WLog_ERR("com.test.tags.second", "an error");
	WLog_DBG("com.test.tags.first", "first again");

	if (!WLog_CloseAppender(root))
		return -1;

	if (received != ARRAYSIZE(expected))
	{
		(void)fprintf(stderr, "%" PRIuz " messages, expected %" PRIuz "\n", received,
		              ARRAYSIZE(expected));
		return -1;
	}
	return success ? 0 : -1;
}
