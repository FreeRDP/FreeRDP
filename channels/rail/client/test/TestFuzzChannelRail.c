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
#include <freerdp/freerdp.h>

#include "../rail_main.h"
#include "../rail_orders.h"

static void dealloc(railPlugin* plugin)
{
	if (!plugin)
		return;
	freerdp_settings_free(plugin->rdpcontext->settings);
	free(plugin->rdpcontext);
	free(plugin);
}

WINPR_ATTR_MALLOC(dealloc, 1)
static railPlugin* alloc(void)
{
	railPlugin* rail = (railPlugin*)calloc(1, sizeof(railPlugin));
	if (!rail)
		return nullptr;
	rail->rdpcontext = calloc(1, sizeof(rdpContext));
	if (!rail->rdpcontext)
		goto fail;

	rail->rdpcontext->settings = freerdp_settings_new(0);
	if (!rail->rdpcontext->settings)
		goto fail;
	return rail;
fail:
	dealloc(rail);
	return nullptr;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2)
		return 0;
	if (size > (1u << 20))
		return 0;

	int rc = -1;

	wLog* root = WLog_GetRoot();
	(void)WLog_SetLogLevel(root, WLOG_TRACE);
	(void)WLog_SetLogAppenderType(root, WLOG_APPENDER_CALLBACK);

	railPlugin* g_rail = alloc();
	RailClientContext* context = (RailClientContext*)calloc(1, sizeof(RailClientContext));
	wStream* s = Stream_New(nullptr, size);
	if (!g_rail || !context || !s)
		goto fail;

	g_rail->log = WLog_Get("fuzz.rail");

	/* A context is required (handlers bail on nullptr); nullptr callbacks skip dispatch. */

	g_rail->context = context;
	g_rail->channelEntryPoints.pInterface = context;

	/* rail_order_recv owns and frees the stream (Stream_Free(s, TRUE)); give it an owned copy. */

	Stream_Write(s, data, size);
	Stream_SealLength(s);
	if (!Stream_SetPosition(s, 0))
		goto fail;

	(void)rail_order_recv(g_rail, s);
	s = nullptr; // Freed up by rail_order_recv

	rc = 0;

fail:
	Stream_Free(s, TRUE);
	free(context);
	dealloc(g_rail);
	return rc;
}
