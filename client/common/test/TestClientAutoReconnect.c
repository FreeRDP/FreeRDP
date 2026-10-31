#include <stdio.h>

#include <winpr/crt.h>

#include <freerdp/freerdp.h>
#include <freerdp/client.h>
#include <freerdp/transport_io.h>

#define MAX_RETRIES 5

static size_t s_attempts = 0;
static UINT32 s_connectError = FREERDP_ERROR_SUCCESS;

/* Stands in for the server: every reconnect attempt fails with s_connectError. */
WINPR_ATTR_NODISCARD
static int test_tcp_connect(rdpContext* context, WINPR_ATTR_UNUSED rdpSettings* settings,
                            WINPR_ATTR_UNUSED const char* hostname, WINPR_ATTR_UNUSED int port,
                            WINPR_ATTR_UNUSED DWORD timeout)
{
	freerdp_set_last_error_log(context, s_connectError);
	return -1;
}

WINPR_ATTR_NODISCARD
static SSIZE_T test_retry_dialog(WINPR_ATTR_UNUSED freerdp* instance,
                                 WINPR_ATTR_UNUSED const char* what, size_t current,
                                 WINPR_ATTR_UNUSED void* userarg)
{
	s_attempts = current + 1;
	return 0;
}

WINPR_ATTR_NODISCARD
static BOOL test_reconnect(UINT32 connectError, size_t expectedAttempts)
{
	BOOL rc = FALSE;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = WINPR_C_ARRAY_INIT;
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;
	clientEntryPoints.ContextSize = sizeof(rdpClientContext);

	rdpContext* context = freerdp_client_context_new(&clientEntryPoints);
	if (!context)
		return FALSE;

	s_attempts = 0;
	s_connectError = connectError;
	context->instance->RetryDialog = test_retry_dialog;

	rdpSettings* settings = context->settings;
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, "127.0.0.1") ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_AutoReconnectMaxRetries, MAX_RETRIES))
		goto fail;

	const rdpTransportIo* defaultIo = freerdp_get_io_callbacks(context);
	if (!defaultIo)
		goto fail;
	rdpTransportIo io = *defaultIo;
	io.TCPConnect = test_tcp_connect;
	if (!freerdp_set_io_callbacks(context, &io))
		goto fail;

	if (client_auto_reconnect_ex(context->instance, nullptr))
	{
		(void)fprintf(stderr, "[%s] reconnect succeeded\n",
		              freerdp_get_last_error_name(connectError));
		goto fail;
	}

	if (s_attempts != expectedAttempts)
	{
		(void)fprintf(stderr, "[%s] expected %" PRIuz " attempts, got %" PRIuz "\n",
		              freerdp_get_last_error_name(connectError), expectedAttempts, s_attempts);
		goto fail;
	}

	const UINT32 lastError = freerdp_get_last_error(context);
	if (lastError != connectError)
	{
		(void)fprintf(stderr, "[%s] last error is %s\n", freerdp_get_last_error_name(connectError),
		              freerdp_get_last_error_name(lastError));
		goto fail;
	}

	rc = TRUE;
fail:
	freerdp_client_context_free(context);
	return rc;
}

int TestClientAutoReconnect(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* A network error keeps retrying until the limit. */
	if (!test_reconnect(FREERDP_ERROR_CONNECT_FAILED, MAX_RETRIES))
		return -1;

	/* Rejected credentials stop after the attempt that got them. */
	const UINT32 rejected[] = { FREERDP_ERROR_CONNECT_LOGON_FAILURE,
		                        FREERDP_ERROR_CONNECT_WRONG_PASSWORD,
		                        FREERDP_ERROR_CONNECT_ACCOUNT_LOCKED_OUT };
	for (size_t x = 0; x < ARRAYSIZE(rejected); x++)
	{
		if (!test_reconnect(rejected[x], 1))
			return -1;
	}

	return 0;
}
