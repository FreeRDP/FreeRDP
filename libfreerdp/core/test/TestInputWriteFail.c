#include <stdio.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#include <freerdp/freerdp.h>
#include <freerdp/client.h>
#include <freerdp/input.h>
#include <freerdp/transport_io.h>

#include "../connection.h"
#include "../input.h"
#include "../rdp.h"
#include "../transport.h"

/* A connection that drops while the session is active: the first input write fails, and that call
 * says so. From then on the session is no longer active, so the next input is skipped as during
 * the reconnect, before the thread that reads has noticed. */
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
	errno = EPIPE;
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

int TestInputWriteFail(int argc, char* argv[])
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

	{
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
	}

	if (!input_register_client_callbacks(context->input) ||
	    !rdp_client_transition_to_state(context->rdp, CONNECTION_STATE_ACTIVE))
		goto fail;

	/* the connection drops: the first write fails */
	const BOOL first = freerdp_input_send_mouse_event(context->input, PTR_FLAGS_MOVE, 10, 10);
	const UINT32 error = freerdp_get_last_error(context);
	const BOOL active = freerdp_is_active_state(context);
	/* the transport is closed, the reader has not noticed yet: the next input */
	const BOOL second = freerdp_input_send_mouse_event(context->input, PTR_FLAGS_MOVE, 20, 20);

	(void)fprintf(stderr,
	              "input while the connection drops: first %s, then active %s, second %s, last "
	              "error 0x%08" PRIx32 "\n",
	              first ? "TRUE" : "FALSE", active ? "TRUE" : "FALSE", second ? "TRUE" : "FALSE",
	              error);

	if (error != FREERDP_ERROR_CONNECT_TRANSPORT_FAILED)
	{
		(void)fprintf(stderr, "the dropped connection is not reported\n");
		goto fail;
	}
	if (first)
	{
		(void)fprintf(stderr, "the input whose write failed reports success\n");
		goto fail;
	}
	if (active || !second)
	{
		(void)fprintf(stderr, "the session is still active after its transport closed\n");
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
