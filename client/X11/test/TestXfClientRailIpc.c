/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 RAIL launch FIFO construction tests
 *
 * Copyright 2026 Tony Dursun <oraturk75@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <freerdp/client.h>
#include <freerdp/client/rail_ipc.h>
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>

#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/synch.h>

#include "xf_client.h"
#include "xfreerdp.h"

static const char TEST_REMOTE_APP_PROGRAM[] = "C:\\Windows\\System32\\notepad.exe";

static BOOL g_ConnectCalled = FALSE;
static BOOL g_IpcPresentAtConnect = FALSE;
static char g_FifoPath[512] = WINPR_C_ARRAY_INIT;
static HANDLE g_InputEvent = nullptr;
static HANDLE g_X11Mutex = nullptr;
static BOOL g_DriveClientLoop = FALSE;
static BOOL g_FreeRdpChecked = FALSE;
static BOOL g_InjectIpcReadFailure = FALSE;
static BOOL g_XPendingCalled = FALSE;
static rdpContext* g_AbortContext = nullptr;
static size_t g_ExecuteCount = 0;
static RailClientContext g_Rail = WINPR_C_ARRAY_INIT;

static BOOL write_record(const char* path)
{
	static const char record[] = "program=queued-before-transition.exe\n\n";
	if (!path)
		return FALSE;
	const int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return FALSE;
	ssize_t written = 0;
	do
	{
		written = write(fd, record, sizeof(record) - 1U);
	} while ((written < 0) && (errno == EINTR));
	const int savedError = errno;
	close(fd);
	errno = savedError;
	return written == (ssize_t)(sizeof(record) - 1U);
}

static BOOL lock_path_from_fifo(const char* fifoPath, char* lockPath, size_t capacity)
{
	static const char suffix[] = ".fifo";
	const size_t length = strlen(fifoPath);
	if ((length < sizeof(suffix) - 1U) ||
	    (strcmp(fifoPath + length - (sizeof(suffix) - 1U), suffix) != 0))
		return FALSE;
	const size_t prefixLength = length - (sizeof(suffix) - 1U);
	if (prefixLength > capacity - sizeof(".lock"))
		return FALSE;
	CopyMemory(lockPath, fifoPath, prefixLength);
	CopyMemory(lockPath + prefixLength, ".lock", sizeof(".lock"));
	return TRUE;
}

static BOOL remove_lock_for_fifo(const char* fifoPath)
{
	char lockPath[sizeof(g_FifoPath)] = WINPR_C_ARRAY_INIT;
	if (!fifoPath || (fifoPath[0] == '\0'))
		return TRUE;
	if (!lock_path_from_fifo(fifoPath, lockPath, sizeof(lockPath)))
		return FALSE;
	if ((unlink(lockPath) < 0) && (errno != ENOENT))
	{
		fprintf(stderr, "could not remove test lock '%s': %s\n", lockPath, strerror(errno));
		return FALSE;
	}
	return TRUE;
}

static UINT execute_record(WINPR_ATTR_UNUSED RailClientContext* rail,
                           WINPR_ATTR_UNUSED const RAIL_EXEC_ORDER* order)
{
	g_ExecuteCount++;
	return CHANNEL_RC_OK;
}

BOOL __wrap_freerdp_connect(freerdp* instance)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	xfContext* xfc = (xfContext*)instance->context;
	g_ConnectCalled = TRUE;
	g_IpcPresentAtConnect = xfc->railIpc != nullptr;
	const char* path = freerdp_client_rail_ipc_get_path(xfc->railIpc);
	if (path)
		(void)_snprintf(g_FifoPath, sizeof(g_FifoPath), "%s", path);
	if (g_DriveClientLoop)
	{
		g_InputEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		g_X11Mutex = CreateMutex(nullptr, FALSE, nullptr);
		xfc->mutex = g_X11Mutex;
		xfc->x11event = g_InputEvent;
		ZeroMemory(&g_Rail, sizeof(g_Rail));
		g_Rail.ClientExecute = execute_record;
		if (!freerdp_client_rail_ipc_attach(xfc->railIpc, &g_Rail) ||
		    !freerdp_client_rail_ipc_set_ready(xfc->railIpc, TRUE) || !g_InputEvent || !xfc->mutex)
			return FALSE;
		if (g_InjectIpcReadFailure)
		{
			const HANDLE event = freerdp_client_rail_ipc_get_event(xfc->railIpc);
			if (!event)
				return FALSE;
			const int readFd = GetEventFileDescriptor(event);
			const char* directory = getenv("XDG_RUNTIME_DIR");
			if ((readFd < 0) || !directory)
				return FALSE;
			const int directoryFd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
			if (directoryFd < 0)
				return FALSE;
			const int replaced = dup2(directoryFd, readFd);
			close(directoryFd);
			if (replaced != readFd)
				return FALSE;
			g_AbortContext = instance->context;
			return TRUE;
		}
		return write_record(path);
	}
	return FALSE;
}

BOOL __wrap_freerdp_disconnect(WINPR_ATTR_UNUSED freerdp* instance)
{
	return TRUE;
}

DWORD __wrap_freerdp_get_event_handles(rdpContext* context, HANDLE* events, DWORD count)
{
	if (!context || !events || (count < 1))
		return 0;
	events[0] = freerdp_abort_event(context);
	return events[0] ? 1 : 0;
}

BOOL __wrap_freerdp_check_event_handles(rdpContext* context)
{
	WINPR_ASSERT(context);
	if (g_DriveClientLoop)
	{
		xfContext* xfc = (xfContext*)context;
		g_FreeRdpChecked = TRUE;
		if (g_InjectIpcReadFailure)
			return TRUE;
		if (!freerdp_client_rail_ipc_set_ready(xfc->railIpc, FALSE))
			return FALSE;
		freerdp_abort_connect_context(context);
	}
	return TRUE;
}

int __wrap_XPending(WINPR_ATTR_UNUSED Display* display)
{
	if (g_InjectIpcReadFailure)
	{
		g_XPendingCalled = TRUE;
		if (g_AbortContext)
			freerdp_abort_connect_context(g_AbortContext);
	}
	return 0;
}

static BOOL set_test_settings(rdpSettings* settings, BOOL railMode, BOOL authenticationOnly,
                              const char* program)
{
	return freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
	                                   "fifo-construction.example.invalid") &&
	       freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, 3389) &&
	       freerdp_settings_set_string(settings, FreeRDP_Username, "fifo-user") &&
	       freerdp_settings_set_bool(settings, FreeRDP_RemoteApplicationMode, railMode) &&
	       freerdp_settings_set_bool(settings, FreeRDP_AuthenticationOnly, authenticationOnly) &&
	       freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram, program);
}

static BOOL run_case(const RDP_CLIENT_ENTRY_POINTS* entry, BOOL option, BOOL railMode,
                     BOOL authenticationOnly, const char* program, BOOL expectConnect,
                     BOOL expectIpc, DWORD* exitCode)
{
	WINPR_ASSERT(entry);
	g_DriveClientLoop = FALSE;
	g_ConnectCalled = FALSE;
	g_IpcPresentAtConnect = FALSE;
	ZeroMemory(g_FifoPath, sizeof(g_FifoPath));
	rdpContext* context = freerdp_client_context_new(entry);
	if (!context)
		return FALSE;
	BOOL started = FALSE;
	BOOL rc = FALSE;
	xfContext* xfc = (xfContext*)context;
	xfc->railMultiExec = option;
	if (!set_test_settings(context->settings, railMode, authenticationOnly, program) ||
	    (freerdp_client_start(context) != 0))
		goto out;
	started = TRUE;
	HANDLE thread = freerdp_client_get_thread(context);
	DWORD code = 0;
	if (!thread || (WaitForSingleObject(thread, 5000) != WAIT_OBJECT_0) ||
	    !GetExitCodeThread(thread, &code))
		goto out;
	if (exitCode)
		*exitCode = code;
	rc = (g_ConnectCalled == expectConnect) &&
	     (!expectConnect || (g_IpcPresentAtConnect == expectIpc)) && (xfc->railIpc == nullptr);
	if (rc && expectIpc)
	{
		struct stat attributes = WINPR_C_ARRAY_INIT;
		rc = (g_FifoPath[0] != '\0') && (lstat(g_FifoPath, &attributes) < 0);
	}

out:
	if (started)
		(void)freerdp_client_stop(context);
	freerdp_client_context_free(context);
	if (!remove_lock_for_fifo(g_FifoPath))
		rc = FALSE;
	return rc;
}

static BOOL test_freerdp_transition_wins(const RDP_CLIENT_ENTRY_POINTS* entry)
{
	WINPR_ASSERT(entry);
	g_ConnectCalled = FALSE;
	g_IpcPresentAtConnect = FALSE;
	g_FreeRdpChecked = FALSE;
	g_ExecuteCount = 0;
	g_DriveClientLoop = TRUE;
	ZeroMemory(g_FifoPath, sizeof(g_FifoPath));
	rdpContext* context = freerdp_client_context_new(entry);
	if (!context)
		return FALSE;
	BOOL started = FALSE;
	BOOL rc = FALSE;
	xfContext* xfc = (xfContext*)context;
	xfc->railMultiExec = TRUE;
	if (!set_test_settings(context->settings, TRUE, FALSE, TEST_REMOTE_APP_PROGRAM) ||
	    (freerdp_client_start(context) != 0))
		goto out;
	started = TRUE;
	HANDLE thread = freerdp_client_get_thread(context);
	if (!thread || (WaitForSingleObject(thread, 5000) != WAIT_OBJECT_0))
		goto out;
	rc = g_ConnectCalled && g_IpcPresentAtConnect && g_FreeRdpChecked && (g_ExecuteCount == 0) &&
	     (xfc->railIpc == nullptr);

out:
	if (started)
		(void)freerdp_client_stop(context);
	xfc->x11event = nullptr;
	xfc->mutex = nullptr;
	freerdp_client_context_free(context);
	if (g_InputEvent)
		(void)CloseHandle(g_InputEvent);
	g_InputEvent = nullptr;
	if (g_X11Mutex)
		(void)CloseHandle(g_X11Mutex);
	g_X11Mutex = nullptr;
	if (!remove_lock_for_fifo(g_FifoPath))
		rc = FALSE;
	g_DriveClientLoop = FALSE;
	return rc;
}

static BOOL test_ipc_failure_stays_in_client_loop(const RDP_CLIENT_ENTRY_POINTS* entry)
{
	WINPR_ASSERT(entry);
	g_ConnectCalled = FALSE;
	g_IpcPresentAtConnect = FALSE;
	g_FreeRdpChecked = FALSE;
	g_InjectIpcReadFailure = TRUE;
	g_XPendingCalled = FALSE;
	g_AbortContext = nullptr;
	g_ExecuteCount = 0;
	g_DriveClientLoop = TRUE;
	ZeroMemory(g_FifoPath, sizeof(g_FifoPath));
	rdpContext* context = freerdp_client_context_new(entry);
	if (!context)
		return FALSE;
	BOOL started = FALSE;
	BOOL rc = FALSE;
	xfContext* xfc = (xfContext*)context;
	xfc->railMultiExec = TRUE;
	if (!set_test_settings(context->settings, TRUE, FALSE, TEST_REMOTE_APP_PROGRAM) ||
	    (freerdp_client_start(context) != 0))
		goto out;
	started = TRUE;
	HANDLE thread = freerdp_client_get_thread(context);
	if (!thread || (WaitForSingleObject(thread, 5000) != WAIT_OBJECT_0))
		goto out;
	rc = g_ConnectCalled && g_IpcPresentAtConnect && g_FreeRdpChecked && g_XPendingCalled &&
	     (g_ExecuteCount == 0) && (xfc->railIpc == nullptr);

out:
	if (started)
		(void)freerdp_client_stop(context);
	xfc->x11event = nullptr;
	xfc->mutex = nullptr;
	freerdp_client_context_free(context);
	if (g_InputEvent)
		(void)CloseHandle(g_InputEvent);
	g_InputEvent = nullptr;
	if (g_X11Mutex)
		(void)CloseHandle(g_X11Mutex);
	g_X11Mutex = nullptr;
	if (!remove_lock_for_fifo(g_FifoPath))
		rc = FALSE;
	g_DriveClientLoop = FALSE;
	g_InjectIpcReadFailure = FALSE;
	g_AbortContext = nullptr;
	return rc;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	int rc = 1;
	char directory[] = "/tmp/xfc.XXXXXX";
	BOOL directoryCreated = FALSE;
	char ownerLockPath[sizeof(g_FifoPath)] = WINPR_C_ARRAY_INIT;
	RDP_CLIENT_ENTRY_POINTS entry = { .Size = sizeof(entry),
		                              .Version = RDP_CLIENT_INTERFACE_VERSION };
	RailClientIpcContext* owner = nullptr;
	rdpSettings* ownerSettings = nullptr;
	wLog* log = WLog_Get("com.freerdp.test.x11.rail-ipc-construction");

	if (!mkdtemp(directory))
		goto out;
	directoryCreated = TRUE;
	if ((chmod(directory, S_IRWXU) < 0) || !SetEnvironmentVariableA("XDG_RUNTIME_DIR", directory) ||
	    (RdpClientEntry(&entry) != 0))
		goto out;
	if (!run_case(&entry, FALSE, TRUE, FALSE, TEST_REMOTE_APP_PROGRAM, TRUE, FALSE, nullptr) ||
	    !run_case(&entry, TRUE, FALSE, FALSE, TEST_REMOTE_APP_PROGRAM, TRUE, FALSE, nullptr) ||
	    !run_case(&entry, TRUE, TRUE, TRUE, TEST_REMOTE_APP_PROGRAM, TRUE, FALSE, nullptr) ||
	    !run_case(&entry, TRUE, TRUE, FALSE, TEST_REMOTE_APP_PROGRAM, TRUE, TRUE, nullptr))
		goto out;
	printf("PASS the FIFO exists only for opted-in, non-authentication RAIL sessions\n");
	if (!run_case(&entry, TRUE, TRUE, FALSE, "", TRUE, TRUE, nullptr))
		goto out;
	printf("PASS an empty initial RemoteApp program reaches connection setup with the FIFO\n");
	if (!test_freerdp_transition_wins(&entry))
		goto out;
	printf("PASS FreeRDP channel transitions are processed before ready FIFO input\n");
	if (!test_ipc_failure_stays_in_client_loop(&entry))
		goto out;
	printf("PASS an operational FIFO failure stays contained in the client loop\n");
	ownerSettings = freerdp_settings_new(0);
	if (!ownerSettings || !set_test_settings(ownerSettings, TRUE, FALSE, TEST_REMOTE_APP_PROGRAM))
		goto out;
	owner = freerdp_client_rail_ipc_new(ownerSettings, log);
	if (!owner)
		goto out;
	const char* ownerFifoPath = freerdp_client_rail_ipc_get_path(owner);
	if (!ownerFifoPath || !lock_path_from_fifo(ownerFifoPath, ownerLockPath, sizeof(ownerLockPath)))
		goto out;
	DWORD duplicateExit = 0;
	if (!run_case(&entry, TRUE, TRUE, FALSE, TEST_REMOTE_APP_PROGRAM, FALSE, FALSE,
	              &duplicateExit) ||
	    (duplicateExit != XF_EXIT_PRE_CONNECT_FAILED))
		goto out;
	printf("PASS an active primary prevents the X11 client from connecting\n");
	rc = 0;

out:
	freerdp_client_rail_ipc_free(owner);
	freerdp_settings_free(ownerSettings);
	(void)SetEnvironmentVariableA("XDG_RUNTIME_DIR", nullptr);
	if ((ownerLockPath[0] != '\0') && (unlink(ownerLockPath) < 0) && (errno != ENOENT))
	{
		fprintf(stderr, "could not remove owner lock '%s': %s\n", ownerLockPath, strerror(errno));
		rc = 1;
	}
	if (directoryCreated && (rmdir(directory) < 0))
	{
		fprintf(stderr, "could not remove test directory '%s': %s\n", directory, strerror(errno));
		rc = 1;
	}
	return rc;
}
