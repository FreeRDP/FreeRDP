/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <freerdp/config.h>
#include <freerdp/client/cmdline.h>
#include "../xf_client.h"
#include "../xf_cmdline.h"
#include "../xfreerdp.h"

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	if (RdpClientEntry(&entry) != 0)
		return 1;
	static const struct
	{
		char* option;
		BOOL enabled;
		BOOL valid;
	} cases[] = { { nullptr, FALSE, TRUE },
		          { "/defer-alt", TRUE, TRUE },
		          { "+defer-alt", TRUE, TRUE },
		          { "-defer-alt", FALSE, TRUE },
		          { "/defer-alt:invalid", FALSE, FALSE },
		          { "/kbd:defer-alt", FALSE, FALSE } };

	for (size_t i = 0; i < ARRAYSIZE(cases); i++)
	{
		xfContext* xfc = (xfContext*)freerdp_client_context_new(&entry);
		if (!xfc)
			return 1;
		/* Parsing starts with the client's default, even when reusing a context. */
		xfc->deferAlt = TRUE;
		char* args[] = { "xfreerdp", "/v:localhost", cases[i].option, nullptr };
		const int count = cases[i].option ? ARRAYSIZE(args) - 1 : ARRAYSIZE(args) - 2;
		const int status = xf_client_settings_parse_command_line(xfc, count, args);
		const BOOL enabled = xfc->deferAlt;
		const BOOL passed =
		    ((status == 0) == cases[i].valid) && (!cases[i].valid || (enabled == cases[i].enabled));
		freerdp_client_context_free(&xfc->common.context);
		if (!passed)
		{
			(void)fprintf(stderr, "option %s: status %d, enabled %d\n",
			              cases[i].option ? cases[i].option : "(default)", status, enabled);
			return 1;
		}
	}

	static const struct
	{
		char* option;
		BOOL enabled;
		BOOL valid;
		BOOL reparse;
	} railCases[] = { { nullptr, FALSE, TRUE, FALSE },
		              { nullptr, FALSE, TRUE, TRUE },
		              { "/rail-multi-exec", TRUE, TRUE, FALSE },
		              { "+rail-multi-exec", TRUE, TRUE, FALSE },
		              { "-rail-multi-exec", FALSE, TRUE, TRUE },
		              { "/rail-multi-exec:invalid", FALSE, FALSE, FALSE } };
#if defined(CHANNEL_RAIL_CLIENT)
	const BOOL supported = TRUE;
#else
	const BOOL supported = FALSE;
#endif
	for (size_t i = 0; i < ARRAYSIZE(railCases); i++)
	{
		xfContext* xfc = (xfContext*)freerdp_client_context_new(&entry);
		if (!xfc)
			return 1;
		if (railCases[i].reparse)
		{
			char* enable[] = { "xfreerdp", "/v:localhost", "+rail-multi-exec", nullptr };
			const int status =
			    xf_client_settings_parse_command_line(xfc, ARRAYSIZE(enable) - 1, enable);
			if (((status == 0) != supported) || (xfc->railMultiExec != supported))
			{
				(void)fprintf(stderr, "multi-exec enable before reparse failed: %d\n", status);
				freerdp_client_context_free(&xfc->common.context);
				return 1;
			}
		}
		char* args[] = { "xfreerdp", "/v:localhost", railCases[i].option, nullptr };
		const int count = railCases[i].option ? ARRAYSIZE(args) - 1 : ARRAYSIZE(args) - 2;
		const int status = xf_client_settings_parse_command_line(xfc, count, args);
		const BOOL valid = railCases[i].valid && (!railCases[i].enabled || supported);
		const BOOL enabled = xfc->railMultiExec;
		const BOOL passed = (valid ? (status == 0) : (status < 0)) &&
		                    (!valid || (enabled == railCases[i].enabled)) &&
		                    (!enabled || (xfc->common.railIpc != nullptr));
		freerdp_client_context_free(&xfc->common.context);
		if (!passed)
		{
			(void)fprintf(stderr, "multi-exec option %s (reparse %d): status %d, enabled %d\n",
			              railCases[i].option ? railCases[i].option : "(default)",
			              railCases[i].reparse, status, enabled);
			return 1;
		}
	}

	/* The common parser must not expose the client-only options. */
	char* options[] = { "/defer-alt", "+rail-multi-exec" };
	for (size_t i = 0; i < ARRAYSIZE(options); i++)
	{
		xfContext* xfc = (xfContext*)freerdp_client_context_new(&entry);
		if (!xfc)
			return 1;
		char* args[] = { "freerdp", "/v:localhost", options[i], nullptr };
		const int status = freerdp_client_settings_parse_command_line_arguments(
		    xfc->common.context.settings, ARRAYSIZE(args) - 1, args, FALSE);
		freerdp_client_context_free(&xfc->common.context);
		if (status >= 0)
		{
			(void)fprintf(stderr, "common parser accepted %s: %d\n", options[i], status);
			return 1;
		}
	}
	return 0;
}
