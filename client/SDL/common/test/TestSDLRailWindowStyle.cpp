#include "../../SDL3/sdl_rail_window_style.hpp"

int TestSDLRailWindowStyle(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	/* Reproducer: WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX, unowned tool window. */
	const auto autocomplete = classifySdlRailWindowStyle(
	    WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX, WS_EX_TOOLWINDOW | WS_EX_TOPMOST, false);
	if (!autocomplete.popup)
		return 1;

	/* A captioned popup remains an ordinary dialog, even when it has an owner. */
	const auto dialog = classifySdlRailWindowStyle(
	    WS_POPUP | WS_CAPTION | WS_SYSMENU, WS_EX_TOOLWINDOW | WS_EX_LAYERED, true);
	if (dialog.popup || dialog.layered)
		return 2;

	/* The same caption-less style remains a toplevel when owned, preserving custom drag loops. */
	const auto ownedStyledPopup = classifySdlRailWindowStyle(
	    WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX, WS_EX_TOOLWINDOW | WS_EX_TOPMOST, true);
	if (ownedStyledPopup.popup)
		return 3;

	/* Preserve the existing owned tool-window path used by application-driven drag loops. */
	const auto ownedTool = classifySdlRailWindowStyle(WS_POPUP, WS_EX_TOOLWINDOW, true);
	if (!ownedTool.popup)
		return 4;

	/* WS_EX_NOACTIVATE remains sufficient even when the window is not a WS_POPUP. */
	const auto noActivate = classifySdlRailWindowStyle(WS_OVERLAPPED, WS_EX_NOACTIVATE, true);
	if (!noActivate.popup)
		return 5;

	return 0;
}
