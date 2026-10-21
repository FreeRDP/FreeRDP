/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <freerdp/config.h>
#include <freerdp/client/cmdline.h>
#include "../xf_cmdline.h"
#include "../xfreerdp.h"

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
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
		xfContext xfc = WINPR_C_ARRAY_INIT;
		rdpSettings* settings = freerdp_settings_new(0);
		if (!settings)
			return 1;
		xfc.common.context.settings = settings;
		/* Parsing starts with the client's default, even when reusing a context. */
		xfc.deferAlt = TRUE;
		char* args[] = { "xfreerdp", "/v:localhost", cases[i].option, nullptr };
		const int count = cases[i].option ? ARRAYSIZE(args) - 1 : ARRAYSIZE(args) - 2;
		const int status = xf_client_settings_parse_command_line(&xfc, count, args);
		const BOOL passed = ((status == 0) == cases[i].valid) &&
		                    (!cases[i].valid || (xfc.deferAlt == cases[i].enabled));
		freerdp_settings_free(settings);
		if (!passed)
		{
			(void)fprintf(stderr, "option %s: status %d, enabled %d\n",
			              cases[i].option ? cases[i].option : "(default)", status, xfc.deferAlt);
			return 1;
		}
	}

	/* The common parser must not expose the X11-only option. */
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		return 1;
	char* args[] = { "freerdp", "/v:localhost", "/defer-alt", nullptr };
	const int status = freerdp_client_settings_parse_command_line_arguments(
	    settings, ARRAYSIZE(args) - 1, args, FALSE);
	freerdp_settings_free(settings);
	return status < 0 ? 0 : 1;
}
