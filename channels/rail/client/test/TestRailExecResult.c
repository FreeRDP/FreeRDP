/* FreeRDP: Licensed under the Apache License, Version 2.0. */

#include <stdio.h>
#include <freerdp/freerdp.h>
#include "../rail_main.h"
#include "../rail_orders.h"

static UINT execute_result(RailClientContext* context, const RAIL_EXEC_RESULT_ORDER* result)
{
	size_t* calls = context->custom;
	WINPR_UNUSED(result);
	(*calls)++;
	return ERROR_INTERNAL_ERROR;
}

int TestRailExecResult(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	int rc = -1;
	rdpContext rdp = WINPR_C_ARRAY_INIT;
	rdp.settings = freerdp_settings_new(0);
	if (!rdp.settings)
		return -1;
	const struct
	{
		BOOL enabled;
		UINT16 result;
		UINT expected;
		size_t calls;
	} cases[] = { { TRUE, RAIL_EXEC_E_FILE_NOT_FOUND, CHANNEL_RC_OK, 0 },
		          { TRUE, RAIL_EXEC_S_OK, ERROR_INTERNAL_ERROR, 1 },
		          { FALSE, RAIL_EXEC_E_FILE_NOT_FOUND, ERROR_INTERNAL_ERROR, 1 } };
	for (size_t i = 0; i < ARRAYSIZE(cases); i++)
	{
		if (!freerdp_settings_set_bool(rdp.settings, FreeRDP_RemoteApplicationMultiExec,
		                               cases[i].enabled))
			goto fail;
		size_t calls = 0;
		RailClientContext context = { .custom = &calls, .ServerExecuteResult = execute_result };
		railPlugin rail = { .rdpcontext = &rdp, .context = &context, .log = WLog_Get("test.rail") };
		rail.channelEntryPoints.pInterface = &context;
		wStream* s = Stream_New(nullptr, 16);
		if (!s)
			goto fail;
		Stream_Write_UINT16(s, TS_RAIL_ORDER_EXEC_RESULT);
		Stream_Write_UINT16(s, 16); /* header + fixed result + empty Unicode string */
		Stream_Write_UINT16(s, 0);  /* flags */
		Stream_Write_UINT16(s, cases[i].result);
		Stream_Write_UINT32(s, 0); /* raw result */
		Stream_Write_UINT16(s, 0); /* padding */
		Stream_Write_UINT16(s, 0); /* string length */
		Stream_SealLength(s);
		Stream_ResetPosition(s);
		const UINT result = rail_order_recv(&rail, s); /* frees s */
		if ((result != cases[i].expected) || (calls != cases[i].calls))
		{
			(void)fprintf(stderr, "Execute result case %zu: result=%u calls=%zu\n", i, result,
			              calls);
			goto fail;
		}
	}
	rc = 0;
fail:
	freerdp_settings_free(rdp.settings);
	return rc;
}
