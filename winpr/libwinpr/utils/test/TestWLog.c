
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/wlog.h>

int TestWLog(int argc, char* argv[])
{
	WLog_TraceA("TEST", "this is a test");
	WLog_TraceA("TEST", "this is a %dnd %s", 2, "test");

	return 0;
}
