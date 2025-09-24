/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 helper utilities
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyringht 2025 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Do not include! X11 has conflicting defines #include "xf_utils.h"
const char* request_code_2_str(int code);

#include <X11/Xproto.h>

const char* request_code_2_str(int code)
{
#define CASE2STR(x) \
	case x:         \
		return #x
	switch (code)
	{
		CASE2STR(X_CreateWindow);
		CASE2STR(X_ChangeWindowAttributes);
		CASE2STR(X_GetWindowAttributes);
		CASE2STR(X_DestroyWindow);
		CASE2STR(X_DestroySubwindows);
		CASE2STR(X_ChangeSaveSet);
		CASE2STR(X_ReparentWindow);
		CASE2STR(X_MapWindow);
		CASE2STR(X_MapSubwindows);
		CASE2STR(X_UnmapWindow);
		CASE2STR(X_UnmapSubwindows);
		CASE2STR(X_ConfigureWindow);
		CASE2STR(X_CirculateWindow);
		CASE2STR(X_GetGeometry);
		CASE2STR(X_QueryTree);
		CASE2STR(X_InternAtom);
		CASE2STR(X_GetAtomName);
		CASE2STR(X_ChangeProperty);
		CASE2STR(X_DeleteProperty);
		CASE2STR(X_GetProperty);
		CASE2STR(X_ListProperties);
		CASE2STR(X_SetSelectionOwner);
		CASE2STR(X_GetSelectionOwner);
		CASE2STR(X_ConvertSelection);
		CASE2STR(X_SendEvent);
		CASE2STR(X_GrabPointer);
		CASE2STR(X_UngrabPointer);
		CASE2STR(X_GrabButton);
		CASE2STR(X_UngrabButton);
		CASE2STR(X_ChangeActivePointerGrab);
		CASE2STR(X_GrabKeyboard);
		CASE2STR(X_UngrabKeyboard);
		CASE2STR(X_GrabKey);
		CASE2STR(X_UngrabKey);
		CASE2STR(X_AllowEvents);
		CASE2STR(X_GrabServer);
		CASE2STR(X_UngrabServer);
		CASE2STR(X_QueryPointer);
		CASE2STR(X_GetMotionEvents);
		CASE2STR(X_TranslateCoords);
		CASE2STR(X_WarpPointer);
		CASE2STR(X_SetInputFocus);
		CASE2STR(X_GetInputFocus);
		CASE2STR(X_QueryKeymap);
		CASE2STR(X_OpenFont);
		CASE2STR(X_CloseFont);
		CASE2STR(X_QueryFont);
		CASE2STR(X_QueryTextExtents);
		CASE2STR(X_ListFonts);
		CASE2STR(X_ListFontsWithInfo);
		CASE2STR(X_SetFontPath);
		CASE2STR(X_GetFontPath);
		CASE2STR(X_CreatePixmap);
		CASE2STR(X_FreePixmap);
		CASE2STR(X_CreateGC);
		CASE2STR(X_ChangeGC);
		CASE2STR(X_CopyGC);
		CASE2STR(X_SetDashes);
		CASE2STR(X_SetClipRectangles);
		CASE2STR(X_FreeGC);
		CASE2STR(X_ClearArea);
		CASE2STR(X_CopyArea);
		CASE2STR(X_CopyPlane);
		CASE2STR(X_PolyPoint);
		CASE2STR(X_PolyLine);
		CASE2STR(X_PolySegment);
		CASE2STR(X_PolyRectangle);
		CASE2STR(X_PolyArc);
		CASE2STR(X_FillPoly);
		CASE2STR(X_PolyFillRectangle);
		CASE2STR(X_PolyFillArc);
		CASE2STR(X_PutImage);
		CASE2STR(X_GetImage);
		CASE2STR(X_PolyText8);
		CASE2STR(X_PolyText16);
		CASE2STR(X_ImageText8);
		CASE2STR(X_ImageText16);
		CASE2STR(X_CreateColormap);
		CASE2STR(X_FreeColormap);
		CASE2STR(X_CopyColormapAndFree);
		CASE2STR(X_InstallColormap);
		CASE2STR(X_UninstallColormap);
		CASE2STR(X_ListInstalledColormaps);
		CASE2STR(X_AllocColor);
		CASE2STR(X_AllocNamedColor);
		CASE2STR(X_AllocColorCells);
		CASE2STR(X_AllocColorPlanes);
		CASE2STR(X_FreeColors);
		CASE2STR(X_StoreColors);
		CASE2STR(X_StoreNamedColor);
		CASE2STR(X_QueryColors);
		CASE2STR(X_LookupColor);
		CASE2STR(X_CreateCursor);
		CASE2STR(X_CreateGlyphCursor);
		CASE2STR(X_FreeCursor);
		CASE2STR(X_RecolorCursor);
		CASE2STR(X_QueryBestSize);
		CASE2STR(X_QueryExtension);
		CASE2STR(X_ListExtensions);
		CASE2STR(X_ChangeKeyboardMapping);
		CASE2STR(X_GetKeyboardMapping);
		CASE2STR(X_ChangeKeyboardControl);
		CASE2STR(X_GetKeyboardControl);
		CASE2STR(X_Bell);
		CASE2STR(X_ChangePointerControl);
		CASE2STR(X_GetPointerControl);
		CASE2STR(X_SetScreenSaver);
		CASE2STR(X_GetScreenSaver);
		CASE2STR(X_ChangeHosts);
		CASE2STR(X_ListHosts);
		CASE2STR(X_SetAccessControl);
		CASE2STR(X_SetCloseDownMode);
		CASE2STR(X_KillClient);
		CASE2STR(X_RotateProperties);
		CASE2STR(X_ForceScreenSaver);
		CASE2STR(X_SetPointerMapping);
		CASE2STR(X_GetPointerMapping);
		CASE2STR(X_SetModifierMapping);
		CASE2STR(X_GetModifierMapping);
		CASE2STR(X_NoOperation);

		default:
			return "UNKNOWN";
	}
}
