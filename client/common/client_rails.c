
#include <freerdp/freerdp.h>

#include <freerdp/client/rail.h>

UINT client_rail_server_start_cmd(RailClientContext* context)
{
	UINT status;
	char argsAndFile[520] = { 0 };
	RAIL_EXEC_ORDER exec = { 0 };
	RAIL_SYSPARAM_ORDER sysparam = { 0 };
	RAIL_CLIENT_STATUS_ORDER clientStatus = { 0 };
	rdpClientContext* ctx;
	rdpSettings* settings;

	WINPR_ASSERT(context);

	ctx = (rdpClientContext*)context->custom;
	WINPR_ASSERT(ctx);

	settings = ctx->context.settings;
	WINPR_ASSERT(settings);

	clientStatus.flags = TS_RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;

	if (settings->AutoReconnectionEnabled)
		clientStatus.flags |= TS_RAIL_CLIENTSTATUS_AUTORECONNECT;

	clientStatus.flags |= TS_RAIL_CLIENTSTATUS_ZORDER_SYNC;
	clientStatus.flags |= TS_RAIL_CLIENTSTATUS_WINDOW_RESIZE_MARGIN_SUPPORTED;
	clientStatus.flags |= TS_RAIL_CLIENTSTATUS_APPBAR_REMOTING_SUPPORTED;
	clientStatus.flags |= TS_RAIL_CLIENTSTATUS_POWER_DISPLAY_REQUEST_SUPPORTED;
	clientStatus.flags |= TS_RAIL_CLIENTSTATUS_BIDIRECTIONAL_CLOAK_SUPPORTED;
	status = context->ClientInformation(context, &clientStatus);

	if (status != CHANNEL_RC_OK)
		return status;

	if (settings->RemoteAppLanguageBarSupported)
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

	sysparam.params = 0;
	sysparam.params |= SPI_MASK_SET_HIGH_CONTRAST;
	sysparam.highContrast.colorScheme.string = NULL;
	sysparam.highContrast.colorScheme.length = 0;
	sysparam.highContrast.flags = 0x7E;
	sysparam.params |= SPI_MASK_SET_MOUSE_BUTTON_SWAP;
	sysparam.mouseButtonSwap = FALSE;
	sysparam.params |= SPI_MASK_SET_KEYBOARD_PREF;
	sysparam.keyboardPref = FALSE;
	sysparam.params |= SPI_MASK_SET_DRAG_FULL_WINDOWS;
	sysparam.dragFullWindows = FALSE;
	sysparam.params |= SPI_MASK_SET_KEYBOARD_CUES;
	sysparam.keyboardCues = FALSE;
	sysparam.params |= SPI_MASK_SET_WORK_AREA;
	sysparam.workArea.left = 0;
	sysparam.workArea.top = 0;
	sysparam.workArea.right = settings->DesktopWidth;
	sysparam.workArea.bottom = settings->DesktopHeight;
	sysparam.dragFullWindows = FALSE;
	status = context->ClientSystemParam(context, &sysparam);

	if (status != CHANNEL_RC_OK)
		return status;

	if (settings->RemoteApplicationFile && settings->RemoteApplicationCmdLine)
	{
		_snprintf(argsAndFile, ARRAYSIZE(argsAndFile), "%s %s", settings->RemoteApplicationCmdLine,
		          settings->RemoteApplicationFile);
		exec.RemoteApplicationArguments = argsAndFile;
	}
	else if (settings->RemoteApplicationFile)
		exec.RemoteApplicationArguments = settings->RemoteApplicationFile;
	else
		exec.RemoteApplicationArguments = settings->RemoteApplicationCmdLine;
	exec.RemoteApplicationProgram = settings->RemoteApplicationProgram;
	exec.RemoteApplicationWorkingDir = settings->ShellWorkingDirectory;
	return context->ClientExecute(context, &exec);
}
