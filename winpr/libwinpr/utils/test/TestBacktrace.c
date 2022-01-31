#include <stdio.h>
#include <winpr/debug.h>

int TestBacktrace(int argc, char* argv[])
{
	int rc = -1;
	size_t used, x;
	char** msg;
	void* stack = winpr_backtrace(20);

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!stack)
	{
		fprintf(stderr, "winpr_backtrace failed!\n");
		return -1;
	}

	msg = winpr_backtrace_symbols(stack, &used);

	if (msg)
	{
		for (x = 0; x < used; x++)
			printf("%" PRIuz ": %s\n", x, msg[x]);

		rc = 0;
	}

	winpr_backtrace_symbols_fd(stack, fileno(stdout));
	winpr_backtrace_free(stack);
	free(msg);
	return rc;
}
