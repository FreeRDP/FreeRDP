
#include <winpr/cast.h>

#include <freerdp/freerdp.h>
#include <freerdp/client/rail.h>

#include "rail_main.h"

UINT client_rail_server_start_cmd(RailClientContext* context)
{
	WINPR_ASSERT(context);
	railPlugin* rail = context->handle;
	WINPR_ASSERT(rail);
	WINPR_ASSERT(rail->rdpcontext);

	const rdpSettings* settings = rail->rdpcontext->settings;
	WINPR_ASSERT(settings);

	RAIL_CLIENT_STATUS_ORDER clientStatus = { .flags = freerdp_settings_get_uint32(
		                                          settings, FreeRDP_RemoteAppFeatureFlags) };

	if (freerdp_settings_get_bool(settings, FreeRDP_AutoReconnectionEnabled))
		clientStatus.flags |= TS_RAIL_CLIENTSTATUS_AUTORECONNECT;
	else
		clientStatus.flags &= ~TS_RAIL_CLIENTSTATUS_AUTORECONNECT;

	UINT status = context->ClientInformation(context, &clientStatus);

	if (status != CHANNEL_RC_OK)
		return status;

	if (freerdp_settings_get_bool(settings, FreeRDP_RemoteAppLanguageBarSupported))
	{
		RAIL_LANGBAR_INFO_ORDER langBarInfo;
		langBarInfo.languageBarStatus = 0x00000008; /* TF_SFT_HIDDEN */
		status = context->ClientLanguageBarInfo(context, &langBarInfo);

		/* We want the language bar, but the server might not support it. */
		switch (status)
		{
			case CHANNEL_RC_OK:
			case ERROR_BAD_CONFIGURATION:
				break;
			default:
				return status;
		}
	}

	const UINT32 w = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
	const UINT32 h = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);

	const RAIL_SYSPARAM_ORDER sysparam = {
		.params = SPI_MASK_SET_HIGH_CONTRAST | SPI_MASK_SET_MOUSE_BUTTON_SWAP |
		          SPI_MASK_SET_KEYBOARD_PREF | SPI_MASK_SET_DRAG_FULL_WINDOWS |
		          SPI_MASK_SET_KEYBOARD_CUES | SPI_MASK_SET_WORK_AREA,
		.highContrast.flags = 0x7E,
		.workArea.right = WINPR_ASSERTING_INT_CAST(UINT16, w),
		.workArea.bottom = WINPR_ASSERTING_INT_CAST(UINT16, h)
	};

	status = context->ClientSystemParam(context, &sysparam);

	if (status != CHANNEL_RC_OK)
		return status;

	const char* RemoteApplicationFile =
	    freerdp_settings_get_string(settings, FreeRDP_RemoteApplicationFile);
	const char* RemoteApplicationCmdLine =
	    freerdp_settings_get_string(settings, FreeRDP_RemoteApplicationCmdLine);

	char argsAndFile[520] = WINPR_C_ARRAY_INIT;
	RAIL_EXEC_ORDER exec = WINPR_C_ARRAY_INIT;
	if (RemoteApplicationFile && RemoteApplicationCmdLine)
	{
		(void)_snprintf(argsAndFile, ARRAYSIZE(argsAndFile), "%s %s", RemoteApplicationCmdLine,
		                RemoteApplicationFile);
		exec.RemoteApplicationArguments = argsAndFile;
	}
	else if (RemoteApplicationFile)
		exec.RemoteApplicationArguments = RemoteApplicationFile;
	else
		exec.RemoteApplicationArguments = RemoteApplicationCmdLine;
	exec.RemoteApplicationProgram =
	    freerdp_settings_get_string(settings, FreeRDP_RemoteApplicationProgram);
	exec.RemoteApplicationWorkingDir =
	    freerdp_settings_get_string(settings, FreeRDP_ShellWorkingDirectory);
	return context->ClientExecute(context, &exec);
}
