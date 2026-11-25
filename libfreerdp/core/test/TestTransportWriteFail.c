#include <stdio.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#include <freerdp/freerdp.h>
#include <freerdp/client.h>
#include <freerdp/transport_io.h>

#include "../transport.h"

/* A layer whose writes fail and whose read side never gets ready, like a WebSocket the client
 * closed that stays CLOSING without a close event: only the failed write tells the transport. */
static HANDLE s_layerEvent = nullptr;

WINPR_ATTR_NODISCARD
static int layer_read(WINPR_ATTR_UNUSED void* userContext, WINPR_ATTR_UNUSED void* data,
                      WINPR_ATTR_UNUSED int bytes)
{
	errno = EAGAIN;
	return -1;
}

WINPR_ATTR_NODISCARD
static int layer_write(WINPR_ATTR_UNUSED void* userContext, WINPR_ATTR_UNUSED const void* data,
                       WINPR_ATTR_UNUSED int bytes)
{
	errno = ECONNRESET;
	return -1;
}

WINPR_ATTR_NODISCARD
static BOOL layer_wait(WINPR_ATTR_UNUSED void* userContext, WINPR_ATTR_UNUSED BOOL waitWrite,
                       WINPR_ATTR_UNUSED DWORD timeout)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static HANDLE layer_get_event(WINPR_ATTR_UNUSED void* userContext)
{
	return s_layerEvent;
}

WINPR_ATTR_NODISCARD
static BOOL any_event_set(rdpTransport* transport)
{
	HANDLE events[MAXIMUM_WAIT_OBJECTS] = WINPR_C_ARRAY_INIT;
	const DWORD count = transport_get_event_handles(transport, events, ARRAYSIZE(events));
	if (count == 0)
		return FALSE;
	const DWORD status = WaitForMultipleObjects(count, events, FALSE, 0);
	return (status >= WAIT_OBJECT_0) && (status < WAIT_OBJECT_0 + count);
}

int TestTransportWriteFail(int argc, char* argv[])
{
	int rc = -1;
	rdpTransportLayer* layer = nullptr;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = WINPR_C_ARRAY_INIT;
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;
	clientEntryPoints.ContextSize = sizeof(rdpClientContext);

	rdpContext* context = freerdp_client_context_new(&clientEntryPoints);
	if (!context)
		return -1;

	s_layerEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!s_layerEvent)
		goto fail;

	rdpTransport* transport = freerdp_get_transport(context);
	if (!transport)
		goto fail;

	layer = transport_layer_new(transport, 0);
	if (!layer)
		goto fail;
	layer->Read = layer_read;
	layer->Write = layer_write;
	layer->Wait = layer_wait;
	layer->GetEvent = layer_get_event;

	if (!transport_attach_layer(transport, layer))
		goto fail;
	layer = nullptr; /* the transport owns it now */

	if (any_event_set(transport))
	{
		(void)fprintf(stderr, "an event is set before the write\n");
		goto fail;
	}

	{
		wStream* s = transport_send_stream_init(transport, 16);
		if (!s)
			goto fail;
		Stream_Write_UINT32(s, 0x12345678);
		const int status = transport_write(transport, s);
		Stream_Free(s, TRUE);
		if (status >= 0)
		{
			(void)fprintf(stderr, "the write did not fail (%d)\n", status);
			goto fail;
		}
	}

	if (!any_event_set(transport))
	{
		(void)fprintf(stderr,
		              "a failed write does not wake the thread that waits on the transport\n");
		goto fail;
	}

	if (transport_check_fds(transport) >= 0)
	{
		(void)fprintf(stderr, "transport_check_fds does not report the closed layer\n");
		goto fail;
	}

	if (freerdp_get_last_error(context) != FREERDP_ERROR_CONNECT_TRANSPORT_FAILED)
	{
		(void)fprintf(stderr, "last error 0x%08" PRIx32 " is not CONNECT_TRANSPORT_FAILED\n",
		              freerdp_get_last_error(context));
		goto fail;
	}

	if (any_event_set(transport))
	{
		(void)fprintf(stderr, "the event is still set after transport_check_fds\n");
		goto fail;
	}

	rc = 0;
fail:
	transport_layer_free(layer);
	freerdp_client_context_free(context);
	if (s_layerEvent)
		(void)CloseHandle(s_layerEvent);
	s_layerEvent = nullptr;
	return rc;
}
