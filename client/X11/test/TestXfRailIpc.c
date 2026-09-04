/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 RAIL launch FIFO integration tests
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/Xatom.h>

#include <freerdp/client.h>
#include <freerdp/client/rail_ipc.h>
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>

#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/synch.h>
#include <winpr/wlog.h>

#include "xf_client.h"
#include "xf_rail.h"
#include "xfreerdp.h"
#include "rail_main.h"

#define TEST_EVENT_COUNT 32U
#define TEST_EVENT_LENGTH 160U

typedef struct
{
	rdpContext* context;
	xfContext* xfc;
	RailClientIpcContext* ipc;
	RailClientContext rail;
	railPlugin plugin;
	rdpGdi gdi;
	_XPrivDisplay display;
	Screen screen;
	UINT executeStatus;
	size_t executeCount;
	size_t eventCount;
	char events[TEST_EVENT_COUNT][TEST_EVENT_LENGTH];
	char lastProgram[TEST_EVENT_LENGTH];
	char lockPath[512];
	BOOL railInitialized;
} testFixture;

static testFixture* g_Fixture = nullptr;

WINPR_API int __wrap_XGetWindowAttributes(WINPR_ATTR_UNUSED Display* display,
                                          WINPR_ATTR_UNUSED Window window,
                                          XWindowAttributes* attributes)
{
	WINPR_ASSERT(attributes);
	ZeroMemory(attributes, sizeof(*attributes));
	return 1;
}

WINPR_API int __wrap_XSelectInput(WINPR_ATTR_UNUSED Display* display,
                                  WINPR_ATTR_UNUSED Window window, WINPR_ATTR_UNUSED long eventMask)
{
	return 1;
}

int __wrap_XGetWindowProperty(WINPR_ATTR_UNUSED Display* display, WINPR_ATTR_UNUSED Window window,
                              Atom property, WINPR_ATTR_UNUSED long offset,
                              WINPR_ATTR_UNUSED long length, WINPR_ATTR_UNUSED Bool deleteProperty,
                              WINPR_ATTR_UNUSED Atom requestedType, Atom* actualType,
                              int* actualFormat, unsigned long* itemCount,
                              unsigned long* bytesAfter, unsigned char** value)
{
	WINPR_ASSERT(g_Fixture);
	WINPR_ASSERT(actualType);
	WINPR_ASSERT(actualFormat);
	WINPR_ASSERT(itemCount);
	WINPR_ASSERT(bytesAfter);
	WINPR_ASSERT(value);
	const size_t count = (property == g_Fixture->xfc->NET_WORKAREA) ? 4U : 1U;
	long* data = calloc(count, sizeof(*data));
	if (!data)
		return BadAlloc;
	if (property == g_Fixture->xfc->NET_NUMBER_OF_DESKTOPS)
		data[0] = 1;
	else if (property == g_Fixture->xfc->NET_CURRENT_DESKTOP)
		data[0] = 0;
	else if (property == g_Fixture->xfc->NET_WORKAREA)
	{
		data[2] = 1280;
		data[3] = 720;
	}
	else
	{
		free(data);
		return BadAtom;
	}
	*actualType = XA_CARDINAL;
	*actualFormat = 32;
	*itemCount = count;
	*bytesAfter = 0;
	*value = WINPR_CXX_COMPAT_CAST(unsigned char*, data);
	return Success;
}

static BOOL record_event(const char* format, ...)
{
	if (!g_Fixture || (g_Fixture->eventCount >= ARRAYSIZE(g_Fixture->events)))
		return FALSE;

	va_list ap;
	va_start(ap, format);
	const int rc = vsnprintf(g_Fixture->events[g_Fixture->eventCount],
	                         sizeof(g_Fixture->events[g_Fixture->eventCount]), format, ap);
	va_end(ap);
	if (rc < 0)
		return FALSE;
	g_Fixture->eventCount++;
	return TRUE;
}

static UINT client_information(WINPR_ATTR_UNUSED RailClientContext* rail,
                               WINPR_ATTR_UNUSED const RAIL_CLIENT_STATUS_ORDER* order)
{
	return record_event("client-information") ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static UINT client_language_bar(WINPR_ATTR_UNUSED RailClientContext* rail,
                                WINPR_ATTR_UNUSED const RAIL_LANGBAR_INFO_ORDER* order)
{
	return record_event("language-bar") ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static UINT client_system_param(WINPR_ATTR_UNUSED RailClientContext* rail,
                                const RAIL_SYSPARAM_ORDER* order)
{
	WINPR_ASSERT(order);
	const char* name =
	    (order->params == SPI_MASK_SET_WORK_AREA) ? "work-area" : "system-parameters";
	return record_event("%s", name) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static UINT client_execute(WINPR_ATTR_UNUSED RailClientContext* rail, const RAIL_EXEC_ORDER* order)
{
	WINPR_ASSERT(g_Fixture);
	WINPR_ASSERT(order);
	const char* program = order->RemoteApplicationProgram ? order->RemoteApplicationProgram : "";
	if (!record_event("execute:%s", program))
		return ERROR_INTERNAL_ERROR;
	(void)_snprintf(g_Fixture->lastProgram, sizeof(g_Fixture->lastProgram), "%s", program);
	g_Fixture->executeCount++;
	return g_Fixture->executeStatus;
}

static BOOL set_test_settings(rdpSettings* settings)
{
	return freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
	                                   "fifo-integration.example.invalid") &&
	       freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, 3389) &&
	       freerdp_settings_set_string(settings, FreeRDP_Username, "fifo-user") &&
	       freerdp_settings_set_bool(settings, FreeRDP_RemoteApplicationMode, TRUE) &&
	       freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram,
	                                   "C:\\Windows\\System32\\notepad.exe") &&
	       freerdp_settings_set_string(settings, FreeRDP_ShellWorkingDirectory, "C:\\Windows") &&
	       freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine, "/test") &&
	       freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationFile, "payload.txt") &&
	       freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1280) &&
	       freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 720);
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

static BOOL remove_lock_file(const char* lockPath)
{
	if (!lockPath || (lockPath[0] == '\0'))
		return TRUE;
	if ((unlink(lockPath) < 0) && (errno != ENOENT))
	{
		fprintf(stderr, "could not remove test lock '%s': %s\n", lockPath, strerror(errno));
		return FALSE;
	}
	return TRUE;
}

static void fixture_free(testFixture* fixture)
{
	if (!fixture)
		return;
	if (fixture->railInitialized)
		(void)xf_rail_uninit(fixture->xfc, &fixture->rail);
	fixture->railInitialized = FALSE;
	freerdp_client_rail_ipc_free(fixture->ipc);
	fixture->ipc = nullptr;
	if (fixture->xfc)
	{
		fixture->xfc->railIpc = nullptr;
		fixture->xfc->display = nullptr;
		fixture->context->gdi = nullptr;
	}
	free(fixture->display);
	fixture->display = nullptr;
	if (g_Fixture == fixture)
		g_Fixture = nullptr;
	freerdp_client_context_free(fixture->context);
	fixture->context = nullptr;
	fixture->xfc = nullptr;
}

static BOOL fixture_init(testFixture* fixture)
{
	WINPR_ASSERT(fixture);
	ZeroMemory(fixture, sizeof(*fixture));
	RDP_CLIENT_ENTRY_POINTS entry = { .Size = sizeof(entry),
		                              .Version = RDP_CLIENT_INTERFACE_VERSION };
	if (RdpClientEntry(&entry) != 0)
		return FALSE;
	fixture->context = freerdp_client_context_new(&entry);
	if (!fixture->context)
		return FALSE;
	fixture->xfc = (xfContext*)fixture->context;
	fixture->display = calloc(1, sizeof(*fixture->display));
	if (!fixture->display || !set_test_settings(fixture->context->settings))
		goto fail;
	fixture->screen.root = 1;
	fixture->display->screens = &fixture->screen;
	fixture->display->nscreens = 1;
	fixture->display->default_screen = 0;
	fixture->xfc->display = WINPR_CXX_COMPAT_CAST(Display*, fixture->display);
	fixture->xfc->remote_app = TRUE;
	fixture->xfc->railMultiExec = TRUE;
	fixture->xfc->NET_NUMBER_OF_DESKTOPS = 1;
	fixture->xfc->NET_CURRENT_DESKTOP = 2;
	fixture->xfc->NET_WORKAREA = 3;
	fixture->xfc->railWorkArea.width = 1;
	fixture->xfc->railWorkArea.height = 1;
	fixture->context->gdi = &fixture->gdi;
	fixture->plugin.rdpcontext = fixture->context;
	fixture->rail.handle = &fixture->plugin;
	fixture->rail.ClientExecute = client_execute;
	fixture->rail.ClientInformation = client_information;
	fixture->rail.ClientLanguageBarInfo = client_language_bar;
	fixture->rail.ClientSystemParam = client_system_param;
	fixture->executeStatus = CHANNEL_RC_OK;
	g_Fixture = fixture;
	fixture->ipc = freerdp_client_rail_ipc_new(fixture->context->settings, fixture->xfc->log);
	if (!fixture->ipc)
		goto fail;
	const char* fifoPath = freerdp_client_rail_ipc_get_path(fixture->ipc);
	if (!fifoPath || !lock_path_from_fifo(fifoPath, fixture->lockPath, sizeof(fixture->lockPath)))
		goto fail;
	fixture->xfc->railIpc = fixture->ipc;
	if (xf_rail_init(fixture->xfc, &fixture->rail) != 1)
		goto fail;
	fixture->railInitialized = TRUE;
	return fixture->context->update->window->MonitoredDesktop &&
	       fixture->context->update->window->NonMonitoredDesktop &&
	       fixture->rail.ServerExecuteResult;

fail:
	fixture_free(fixture);
	return FALSE;
}

static BOOL write_record(const testFixture* fixture, const char* program)
{
	WINPR_ASSERT(fixture);
	WINPR_ASSERT(program);
	const char* path = freerdp_client_rail_ipc_get_path(fixture->ipc);
	char record[512] = WINPR_C_ARRAY_INIT;
	const int length = _snprintf(record, sizeof(record), "program=%s\n\n", program);
	if (!path || (length < 0) || ((size_t)length >= sizeof(record)))
		return FALSE;
	const int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return FALSE;
	ssize_t written = 0;
	do
	{
		written = write(fd, record, (size_t)length);
	} while ((written < 0) && (errno == EINTR));
	const int savedError = errno;
	close(fd);
	errno = savedError;
	return written == length;
}

static BOOL monitored(testFixture* fixture, UINT32 flags)
{
	const WINDOW_ORDER_INFO info = { .fieldFlags = WINDOW_ORDER_TYPE_DESKTOP | flags };
	const MONITORED_DESKTOP_ORDER order = WINPR_C_ARRAY_INIT;
	return fixture->context->update->window->MonitoredDesktop(fixture->context, &info, &order);
}

static BOOL non_monitored(testFixture* fixture)
{
	const WINDOW_ORDER_INFO info = { .fieldFlags = WINDOW_ORDER_TYPE_DESKTOP |
		                                           WINDOW_ORDER_FIELD_DESKTOP_NONE };
	fixture->xfc->remote_app = FALSE;
	const BOOL rc = fixture->context->update->window->NonMonitoredDesktop(fixture->context, &info);
	fixture->xfc->remote_app = TRUE;
	return rc;
}

static BOOL post_result(testFixture* fixture, UINT16 result)
{
	const RAIL_EXEC_RESULT_ORDER order = { .execResult = result };
	return fixture->rail.ServerExecuteResult(&fixture->rail, &order) == CHANNEL_RC_OK;
}

static BOOL expect_events(const testFixture* fixture, const char* const* expected, size_t count)
{
	if (fixture->eventCount != count)
	{
		fprintf(stderr, "expected %" PRIuz " events, got %" PRIuz "\n", count, fixture->eventCount);
		return FALSE;
	}
	for (size_t x = 0; x < count; x++)
	{
		if (strcmp(fixture->events[x], expected[x]) != 0)
		{
			fprintf(stderr, "event %" PRIuz ": expected '%s', got '%s'\n", x, expected[x],
			        fixture->events[x]);
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL test_preamble_before_fifo(testFixture* fixture)
{
	static const char* const expected[] = { "client-information", "system-parameters",
		                                    "execute:C:\\Windows\\System32\\notepad.exe",
		                                    "work-area", "execute:queued-before-arc.exe" };
	if (!write_record(fixture, "queued-before-arc.exe"))
	{
		fprintf(stderr, "could not queue the pre-ARC record\n");
		return FALSE;
	}
	if (freerdp_client_rail_ipc_get_event(fixture->ipc))
	{
		fprintf(stderr, "FIFO event exposed before ARC_COMPLETED\n");
		return FALSE;
	}
	if (!monitored(fixture, WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED))
	{
		fprintf(stderr, "ARC_COMPLETED handler failed\n");
		return FALSE;
	}
	if (fixture->eventCount != ARRAYSIZE(expected) - 1U)
	{
		fprintf(stderr, "ARC_COMPLETED produced %" PRIuz " events, expected %" PRIuz "\n",
		        fixture->eventCount, ARRAYSIZE(expected) - 1U);
		for (size_t x = 0; x < fixture->eventCount; x++)
			fprintf(stderr, "event %" PRIuz ": '%s'\n", x, fixture->events[x]);
		return FALSE;
	}
	if (!freerdp_client_rail_ipc_get_event(fixture->ipc))
	{
		fprintf(stderr, "FIFO event hidden after ARC_COMPLETED\n");
		return FALSE;
	}
	return freerdp_client_rail_ipc_check_event(fixture->ipc) &&
	       expect_events(fixture, expected, ARRAYSIZE(expected));
}

static BOOL test_empty_program_starts_from_fifo(testFixture* fixture)
{
	BOOL rc = FALSE;
	char program[TEST_EVENT_LENGTH] = WINPR_C_ARRAY_INIT;
	static const char queuedProgram[] = "queued-while-program-empty.exe";
	static const char* const expected[] = { "client-information", "system-parameters", "work-area",
		                                    "execute:queued-while-program-empty.exe" };
	const char* configured =
	    freerdp_settings_get_string(fixture->context->settings, FreeRDP_RemoteApplicationProgram);
	if (!configured || (_snprintf(program, sizeof(program), "%s", configured) < 0))
		return FALSE;
	const size_t before = fixture->executeCount;
	fixture->eventCount = 0;
	fixture->xfc->railWorkArea.width = 1;
	fixture->xfc->railWorkArea.height = 1;

	if (!monitored(fixture, WINDOW_ORDER_FIELD_DESKTOP_ARC_BEGAN) ||
	    !freerdp_settings_set_string(fixture->context->settings, FreeRDP_RemoteApplicationProgram,
	                                 "") ||
	    !write_record(fixture, queuedProgram))
	{
		fprintf(stderr, "could not prepare the empty-program FIFO launch\n");
		goto out;
	}
	if (freerdp_client_rail_ipc_get_event(fixture->ipc) || (fixture->executeCount != before) ||
	    (fixture->eventCount != 0))
	{
		fprintf(stderr, "FIFO input became ready before the empty-program preamble\n");
		goto out;
	}
	if (!monitored(fixture, WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED))
	{
		fprintf(stderr, "empty-program ARC_COMPLETED handler failed\n");
		goto out;
	}
	if (!freerdp_client_rail_ipc_get_event(fixture->ipc) ||
	    (fixture->eventCount != ARRAYSIZE(expected) - 1U) || (fixture->executeCount != before))
	{
		fprintf(stderr,
		        "empty-program preamble produced %" PRIuz " events and %" PRIuz " Executes\n",
		        fixture->eventCount, fixture->executeCount - before);
		for (size_t x = 0; x < fixture->eventCount; x++)
			fprintf(stderr, "event %" PRIuz ": '%s'\n", x, fixture->events[x]);
		goto out;
	}
	rc = freerdp_client_rail_ipc_check_event(fixture->ipc) &&
	     (fixture->executeCount == before + 1U) &&
	     (strcmp(fixture->lastProgram, queuedProgram) == 0) &&
	     expect_events(fixture, expected, ARRAYSIZE(expected));

out:
	return freerdp_settings_set_string(fixture->context->settings, FreeRDP_RemoteApplicationProgram,
	                                   program) &&
	       rc;
}

static BOOL test_suspend_edge(testFixture* fixture, UINT32 flags, BOOL useNonMonitored,
                              const char* program)
{
	char startupEvent[TEST_EVENT_LENGTH] = WINPR_C_ARRAY_INIT;
	char queuedEvent[TEST_EVENT_LENGTH] = WINPR_C_ARRAY_INIT;
	const char* startupProgram =
	    freerdp_settings_get_string(fixture->context->settings, FreeRDP_RemoteApplicationProgram);
	if (!startupProgram ||
	    (_snprintf(startupEvent, sizeof(startupEvent), "execute:%s", startupProgram) < 0) ||
	    (_snprintf(queuedEvent, sizeof(queuedEvent), "execute:%s", program) < 0))
		return FALSE;
	const char* const expected[] = { "client-information", "system-parameters", startupEvent,
		                             queuedEvent };
	const size_t before = fixture->executeCount;
	fixture->eventCount = 0;
	if (!write_record(fixture, program))
		return FALSE;
	const HANDLE observed = freerdp_client_rail_ipc_get_event(fixture->ipc);
	if (!observed || (WaitForSingleObject(observed, 1000) != WAIT_OBJECT_0))
		return FALSE;
	if (useNonMonitored ? !non_monitored(fixture) : !monitored(fixture, flags))
		return FALSE;
	if (!freerdp_client_rail_ipc_check_event(fixture->ipc))
		return FALSE;
	if (freerdp_client_rail_ipc_get_event(fixture->ipc) || (fixture->executeCount != before) ||
	    (fixture->eventCount != 0))
		return FALSE;
	if (!monitored(fixture, WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED) ||
	    !freerdp_client_rail_ipc_get_event(fixture->ipc) ||
	    (fixture->eventCount != ARRAYSIZE(expected) - 1U) || (fixture->executeCount != before + 1U))
		return FALSE;
	return freerdp_client_rail_ipc_check_event(fixture->ipc) &&
	       (fixture->executeCount == before + 2U) && (strcmp(fixture->lastProgram, program) == 0) &&
	       expect_events(fixture, expected, ARRAYSIZE(expected));
}

static BOOL test_local_and_server_failures(testFixture* fixture)
{
	const size_t before = fixture->executeCount;
	fixture->executeStatus = ERROR_INTERNAL_ERROR;
	if (!write_record(fixture, "local-failure.exe"))
		return FALSE;
	if (!freerdp_client_rail_ipc_check_event(fixture->ipc))
		return FALSE;
	if ((fixture->executeCount != before + 1U) ||
	    freerdp_shall_disconnect_context(fixture->context))
		return FALSE;
	fixture->executeStatus = CHANNEL_RC_OK;
	if (!write_record(fixture, "after-local-failure.exe"))
		return FALSE;
	if (!freerdp_client_rail_ipc_check_event(fixture->ipc))
		return FALSE;
	if ((fixture->executeCount != before + 2U) ||
	    (strcmp(fixture->lastProgram, "after-local-failure.exe") != 0))
		return FALSE;
	if (!post_result(fixture, RAIL_EXEC_S_OK) || !post_result(fixture, RAIL_EXEC_E_FILE_NOT_FOUND))
		return FALSE;
	return !freerdp_shall_disconnect_context(fixture->context);
}

static BOOL test_channel_teardown_wins(testFixture* fixture)
{
	const size_t before = fixture->executeCount;
	if (!write_record(fixture, "queued-before-detach.exe"))
		return FALSE;
	const HANDLE observed = freerdp_client_rail_ipc_get_event(fixture->ipc);
	if (!observed || (WaitForSingleObject(observed, 1000) != WAIT_OBJECT_0) ||
	    (xf_rail_uninit(fixture->xfc, &fixture->rail) != 1))
		return FALSE;
	fixture->railInitialized = FALSE;
	return freerdp_client_rail_ipc_check_event(fixture->ipc) &&
	       !freerdp_client_rail_ipc_get_event(fixture->ipc) && (fixture->executeCount == before);
}

static BOOL test_x11_failure_callers(void)
{
	BOOL rc = FALSE;
	testFixture fixture = WINPR_C_ARRAY_INIT;
	RailClientContext other = WINPR_C_ARRAY_INIT;
	RailClientContext blocker = WINPR_C_ARRAY_INIT;

	if (!fixture_init(&fixture))
		return FALSE;

	const size_t before = fixture.executeCount;
	if (!freerdp_client_rail_ipc_detach(fixture.ipc, &fixture.rail) ||
	    !monitored(&fixture, WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED) ||
	    (fixture.executeCount != before + 1U) || freerdp_client_rail_ipc_get_event(fixture.ipc) ||
	    !freerdp_client_rail_ipc_attach(fixture.ipc, &fixture.rail) ||
	    !freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE))
		goto out;

	if ((xf_rail_uninit(fixture.xfc, &other) != 1) || fixture.xfc->rail || fixture.rail.custom ||
	    fixture.xfc->railWindows || fixture.xfc->railIconCache)
		goto out;
	fixture.railInitialized = FALSE;
	if (!freerdp_client_rail_ipc_detach(fixture.ipc, &fixture.rail) ||
	    !freerdp_client_rail_ipc_attach(fixture.ipc, &blocker))
		goto out;

	if ((xf_rail_init(fixture.xfc, &fixture.rail) != 0) || fixture.xfc->rail ||
	    fixture.rail.custom || freerdp_client_rail_ipc_get_event(fixture.ipc) ||
	    !freerdp_client_rail_ipc_detach(fixture.ipc, &blocker))
		goto out;
	rc = TRUE;

out:
	fixture_free(&fixture);
	return rc;
}

static BOOL test_execute_result_policy(void)
{
	BOOL rc = FALSE;
	testFixture fixture = WINPR_C_ARRAY_INIT;
	if (!fixture_init(&fixture))
		return FALSE;
	fixture.xfc->railMultiExec = FALSE;
	if (!post_result(&fixture, RAIL_EXEC_E_FILE_NOT_FOUND) ||
	    !freerdp_shall_disconnect_context(fixture.context))
		goto out;
	fixture_free(&fixture);

	if (!fixture_init(&fixture) || !monitored(&fixture, WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED))
		goto out;
	const size_t before = fixture.executeCount;
	if (!post_result(&fixture, RAIL_EXEC_E_FILE_NOT_FOUND) ||
	    freerdp_shall_disconnect_context(fixture.context) ||
	    !write_record(&fixture, "after-server-failure.exe") ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc) ||
	    (fixture.executeCount != before + 1U) ||
	    (strcmp(fixture.lastProgram, "after-server-failure.exe") != 0))
		goto out;
	fixture_free(&fixture);

	if (!fixture_init(&fixture) || !post_result(&fixture, RAIL_EXEC_S_OK) ||
	    freerdp_shall_disconnect_context(fixture.context) ||
	    !post_result(&fixture, RAIL_EXEC_E_FILE_NOT_FOUND) ||
	    freerdp_shall_disconnect_context(fixture.context))
		goto out;
	rc = TRUE;

out:
	fixture_free(&fixture);
	return rc;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	int rc = 1;
	char directory[] = "/tmp/xfi.XXXXXX";
	BOOL directoryCreated = FALSE;
	testFixture fixture = WINPR_C_ARRAY_INIT;

	if (!mkdtemp(directory))
		goto out;
	directoryCreated = TRUE;
	if ((chmod(directory, S_IRWXU) < 0) || !SetEnvironmentVariableA("XDG_RUNTIME_DIR", directory) ||
	    !fixture_init(&fixture))
		goto out;
	if (!test_preamble_before_fifo(&fixture))
	{
		fprintf(stderr, "preamble/readiness integration failed\n");
		goto out;
	}
	printf("PASS ARC_COMPLETED completed the preamble and original EXEC before FIFO dispatch\n");
	if (!test_empty_program_starts_from_fifo(&fixture))
	{
		fprintf(stderr, "empty-program readiness integration failed\n");
		goto out;
	}
	printf("PASS an empty program sent the preamble and accepted the first FIFO launch\n");

	if (!test_suspend_edge(&fixture, WINDOW_ORDER_FIELD_DESKTOP_ARC_BEGAN, FALSE,
	                       "queued-during-arc-began.exe") ||
	    !test_suspend_edge(&fixture, WINDOW_ORDER_FIELD_DESKTOP_HOOKED, FALSE,
	                       "queued-during-hooked.exe") ||
	    !test_suspend_edge(&fixture, 0, TRUE, "queued-during-non-monitored.exe"))
	{
		fprintf(stderr, "readiness-loss integration failed\n");
		goto out;
	}
	printf("PASS every desktop readiness-loss edge stopped FIFO draining until ARC_COMPLETED\n");

	if (!test_local_and_server_failures(&fixture))
	{
		fprintf(stderr, "failure-policy integration failed\n");
		goto out;
	}
	printf("PASS local FIFO failure and a later server rejection preserved the session\n");

	if (!test_channel_teardown_wins(&fixture))
	{
		fprintf(stderr, "channel-detach integration failed\n");
		goto out;
	}
	printf("PASS a channel transition after wake prevented stale FIFO dispatch\n");
	fixture_free(&fixture);
	if (!test_x11_failure_callers())
	{
		fprintf(stderr, "X11 IPC failure handling failed\n");
		goto out;
	}
	printf("PASS X11 contains readiness and detach failures and rolls back attach failure\n");
	if (!test_execute_result_policy())
	{
		fprintf(stderr, "mode-based Execute failure policy failed\n");
		goto out;
	}
	printf("PASS Execute failures are fatal only outside multi-exec mode\n");
	rc = 0;

out:
	fixture_free(&fixture);
	(void)SetEnvironmentVariableA("XDG_RUNTIME_DIR", nullptr);
	if (!remove_lock_file(fixture.lockPath))
		rc = 1;
	if (directoryCreated && (rmdir(directory) < 0))
	{
		fprintf(stderr, "could not remove test directory '%s': %s\n", directory, strerror(errno));
		rc = 1;
	}
	return rc;
}
