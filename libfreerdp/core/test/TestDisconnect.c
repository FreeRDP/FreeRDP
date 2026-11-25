#include <stdio.h>

#include <winpr/crt.h>

#include <freerdp/freerdp.h>
#include <freerdp/client.h>
#include <freerdp/transport_io.h>

#include "../connection.h"

static rdpContext* s_context = nullptr;
static pTransportFkt s_defaultDisconnect = nullptr;
static size_t s_disconnects = 0;
static size_t s_activeDisconnects = 0;

/* The input functions run on other threads than the one that disconnects and only check that
 * the session is active, so it must not be while the transport is torn down. */
WINPR_ATTR_NODISCARD
static BOOL test_transport_disconnect(rdpTransport* transport)
{
	s_disconnects++;
	if (freerdp_is_active_state(s_context))
		s_activeDisconnects++;

	return s_defaultDisconnect(transport);
}

int TestDisconnect(int argc, char* argv[])
{
	int rc = -1;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = WINPR_C_ARRAY_INIT;
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;
	clientEntryPoints.ContextSize = sizeof(rdpClientContext);

	s_context = freerdp_client_context_new(&clientEntryPoints);
	if (!s_context)
		return -1;

	const rdpTransportIo* defaultIo = freerdp_get_io_callbacks(s_context);
	if (!defaultIo)
		goto fail;
	rdpTransportIo io = *defaultIo;
	s_defaultDisconnect = io.TransportDisconnect;
	io.TransportDisconnect = test_transport_disconnect;
	if (!freerdp_set_io_callbacks(s_context, &io))
		goto fail;

	if (!rdp_client_transition_to_state(s_context->rdp, CONNECTION_STATE_ACTIVE))
		goto fail;

	if (!rdp_client_disconnect(s_context->rdp))
		goto fail;

	if (s_disconnects == 0)
	{
		(void)fprintf(stderr, "the transport was not disconnected\n");
		goto fail;
	}

	if (s_activeDisconnects != 0)
	{
		(void)fprintf(stderr,
		              "%" PRIuz " of %" PRIuz " transport disconnects ran in the active state\n",
		              s_activeDisconnects, s_disconnects);
		goto fail;
	}

	rc = 0;
fail:
	freerdp_client_context_free(s_context);
	s_context = nullptr;
	return rc;
}
