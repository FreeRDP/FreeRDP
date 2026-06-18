/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for rail (Remote Applications Integrated Locally) PDU parsing
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include <freerdp/client/rail.h>

#include "../rail_main.h"
#include "../rail_orders.h"

static railPlugin* g_rail = nullptr;

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2)
		return 0;
	if (size > (1u << 20))
		return 0;

	if (!g_rail)
	{
		g_rail = (railPlugin*)calloc(1, sizeof(railPlugin));
		if (!g_rail)
			return 0;

		g_rail->log = WLog_Get("fuzz.rail");

		/* A context is required (handlers bail on NULL); NULL callbacks skip dispatch. */
		RailClientContext* context = (RailClientContext*)calloc(1, sizeof(RailClientContext));
		g_rail->context = context;
		g_rail->channelEntryPoints.pInterface = context;
	}

	/* rail_order_recv owns and frees the stream (Stream_Free(s, TRUE)); give it an owned copy. */
	wStream* s = Stream_New(NULL, size);
	if (!s)
		return 0;

	Stream_Write(s, data, size);
	Stream_SealLength(s);
	Stream_SetPosition(s, 0);

	(void)rail_order_recv(g_rail, s);

	return 0;
}
