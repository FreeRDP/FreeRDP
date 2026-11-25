
#include <winpr/crt.h>
#include <winpr/synch.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

static BOOL test_auto_reset(void)
{
	HANDLE event = CreateEvent(nullptr, FALSE, TRUE, nullptr);
	if (!event)
	{
		printf("CreateEvent (auto-reset) failure\n");
		return FALSE;
	}

	BOOL rc = FALSE;
	if (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
		printf("auto-reset: initial state not signaled\n");
	else if (WaitForSingleObject(event, 0) != WAIT_TIMEOUT)
		printf("auto-reset: still signaled after a successful wait\n");
	else if (!SetEvent(event) || (WaitForSingleObject(event, 0) != WAIT_OBJECT_0))
		printf("auto-reset: not signaled after SetEvent\n");
	else if (WaitForSingleObject(event, 0) != WAIT_TIMEOUT)
		printf("auto-reset: still signaled after the second wait\n");
	else
		rc = TRUE;

	(void)CloseHandle(event);
	return rc;
}

#ifndef _WIN32
/* An event that wraps a descriptor of the caller must not consume the caller's data. */
static BOOL test_attached_descriptor(void)
{
	int fds[2] = WINPR_C_ARRAY_INIT;
	if (pipe(fds) != 0)
	{
		printf("pipe failure\n");
		return FALSE;
	}

	BOOL rc = FALSE;
	HANDLE event = nullptr;
	const char data[] = "abc";
	char buffer[sizeof(data)] = WINPR_C_ARRAY_INIT;

	const int flags = fcntl(fds[0], F_GETFL);
	if ((flags < 0) || (fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) < 0))
	{
		printf("fcntl failure\n");
		goto out;
	}

	event = CreateFileDescriptorEvent(nullptr, FALSE, FALSE, fds[0], WINPR_FD_READ);
	if (!event)
	{
		printf("CreateFileDescriptorEvent failure\n");
		goto out;
	}

	if (WaitForSingleObject(event, 0) != WAIT_TIMEOUT)
	{
		printf("attached descriptor: signaled without data\n");
		goto out;
	}

	if (write(fds[1], data, sizeof(data)) != (ssize_t)sizeof(data))
	{
		printf("write failure\n");
		goto out;
	}

	if (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
	{
		printf("attached descriptor: not signaled with data\n");
		goto out;
	}

	if (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
	{
		printf("attached descriptor: data consumed by the first wait\n");
		goto out;
	}

	if ((read(fds[0], buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer)) ||
	    (memcmp(buffer, data, sizeof(data)) != 0))
	{
		printf("attached descriptor: data lost\n");
		goto out;
	}

	rc = TRUE;
out:
	if (event)
		(void)CloseHandle(event);
	(void)close(fds[0]);
	(void)close(fds[1]);
	return rc;
}
#endif

int TestSynchEvent(int argc, char* argv[])
{
	HANDLE event = nullptr;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	if (ResetEvent(nullptr))
	{
		printf("ResetEvent(nullptr) unexpectedly succeeded\n");
		return -1;
	}

	if (SetEvent(nullptr))
	{
		printf("SetEvent(nullptr) unexpectedly succeeded\n");
		return -1;
	}

	event = CreateEvent(nullptr, TRUE, TRUE, nullptr);

	if (!event)
	{
		printf("CreateEvent failure\n");
		return -1;
	}

	if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failure 1\n");
		return -1;
	}

	if (!ResetEvent(event))
	{
		printf("ResetEvent failure with signaled event object\n");
		return -1;
	}

	if (WaitForSingleObject(event, 0) != WAIT_TIMEOUT)
	{
		printf("WaitForSingleObject failure 2\n");
		return -1;
	}

	if (!ResetEvent(event))
	{
		/* Note: ResetEvent must also succeed if event is currently nonsignaled */
		printf("ResetEvent failure with nonsignaled event object\n");
		return -1;
	}

	if (!SetEvent(event))
	{
		printf("SetEvent failure with nonsignaled event object\n");
		return -1;
	}

	if (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failure 3\n");
		return -1;
	}

	for (int i = 0; i < 10000; i++)
	{
		if (!SetEvent(event))
		{
			printf("SetEvent failure with signaled event object (i = %d)\n", i);
			return -1;
		}
	}

	if (!ResetEvent(event))
	{
		printf("ResetEvent failure after multiple SetEvent calls\n");
		return -1;
	}

	/* Independent of the amount of the previous SetEvent calls, a single
	   ResetEvent must be sufficient to get into nonsignaled state */

	if (WaitForSingleObject(event, 0) != WAIT_TIMEOUT)
	{
		printf("WaitForSingleObject failure 4\n");
		return -1;
	}

	(void)CloseHandle(event);

	if (!test_auto_reset())
		return -1;

#ifndef _WIN32
	if (!test_attached_descriptor())
		return -1;
#endif

	return 0;
}
