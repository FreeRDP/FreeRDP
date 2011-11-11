/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Remote Applications Integrated Locally (RAIL)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
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

#ifndef __RAIL_GLOBAL_H
#define __RAIL_GLOBAL_H

#include <freerdp/types.h>

/* RAIL PDU flags */
#define RAIL_EXEC_FLAG_EXPAND_WORKINGDIRECTORY		0x0001
#define RAIL_EXEC_FLAG_TRANSLATE_FILES			0x0002
#define RAIL_EXEC_FLAG_FILE				0x0004
#define RAIL_EXEC_FLAG_EXPAND_ARGUMENTS			0x0008

/* Notification Icon Balloon Tooltip */
#define NIIF_NONE					0x00000000
#define NIIF_INFO					0x00000001
#define NIIF_WARNING					0x00000002
#define NIIF_ERROR					0x00000003
#define NIIF_NOSOUND					0x00000010
#define NIIF_LARGE_ICON					0x00000020

/* Client Execute PDU Flags */
#define RAIL_EXEC_FLAG_EXPAND_WORKING_DIRECTORY		0x0001
#define RAIL_EXEC_FLAG_TRANSLATE_FILES			0x0002
#define RAIL_EXEC_FLAG_FILE				0x0004
#define RAIL_EXEC_FLAG_EXPAND_ARGUMENTS			0x0008

/* Server Execute Result PDU */
#define RAIL_EXEC_S_OK					0x0000
#define RAIL_EXEC_E_HOOK_NOT_LOADED			0x0001
#define RAIL_EXEC_E_DECODE_FAILED			0x0002
#define RAIL_EXEC_E_NOT_IN_ALLOWLIST			0x0003
#define RAIL_EXEC_E_FILE_NOT_FOUND			0x0005
#define RAIL_EXEC_E_FAIL				0x0006
#define RAIL_EXEC_E_SESSION_LOCKED			0x0007

/* Client System Parameters Update PDU */
#define SPI_SET_DRAG_FULL_WINDOWS			0x00000025
#define SPI_SET_KEYBOARD_CUES				0x0000100B
#define SPI_SET_KEYBOARD_PREF				0x00000045
#define SPI_SET_MOUSE_BUTTON_SWAP			0x00000021
#define SPI_SET_WORK_AREA				0x0000002F
#define SPI_DISPLAY_CHANGE				0x0000F001
#define SPI_TASKBAR_POS					0x0000F000
#define SPI_SET_HIGH_CONTRAST				0x00000043

/* Server System Parameters Update PDU */
#define SPI_SET_SCREEN_SAVE_ACTIVE			0x00000011
#define SPI_SET_SCREEN_SAVE_SECURE			0x00000077

/*Bit mask values for SPI_ parameters*/
enum SPI_MASK
{
	SPI_MASK_SET_DRAG_FULL_WINDOWS 		= 0x00000001,
	SPI_MASK_SET_KEYBOARD_CUES 			= 0x00000002,
	SPI_MASK_SET_KEYBOARD_PREF 			= 0x00000004,
	SPI_MASK_SET_MOUSE_BUTTON_SWAP 		= 0x00000008,
	SPI_MASK_SET_WORK_AREA 				= 0x00000010,
	SPI_MASK_DISPLAY_CHANGE 			= 0x00000020,
	SPI_MASK_TASKBAR_POS 				= 0x00000040,
	SPI_MASK_SET_HIGH_CONTRAST 			= 0x00000080,
	SPI_MASK_SET_SCREEN_SAVE_ACTIVE 	= 0x00000100,
	SPI_MASK_SET_SET_SCREEN_SAVE_SECURE = 0x00000200
};


/* Client System Command PDU */
#define SC_SIZE						0xF000
#define SC_MOVE						0xF010
#define SC_MINIMIZE					0xF020
#define SC_MAXIMIZE					0xF030
#define SC_CLOSE					0xF060
#define SC_KEYMENU					0xF100
#define SC_RESTORE					0xF120
#define SC_DEFAULT					0xF160

/* Client Notify Event PDU */
#ifndef _WIN32
#define WM_LBUTTONDOWN					0x00000201
#define WM_LBUTTONUP					0x00000202
#define WM_RBUTTONDOWN					0x00000204
#define WM_RBUTTONUP					0x00000205
#define WM_CONTEXTMENU					0x0000007B
#define WM_LBUTTONDBLCLK				0x00000203
#define WM_RBUTTONDBLCLK				0x00000206

#define NIN_SELECT					0x00000400
#define NIN_KEYSELECT					0x00000401
#define NIN_BALLOONSHOW					0x00000402
#define NIN_BALLOONHIDE					0x00000403
#define NIN_BALLOONTIMEOUT				0x00000404
#define NIN_BALLOONUSERCLICK				0x00000405
#endif

/* Client Information PDU */
#define RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE		0x00000001
#define RAIL_CLIENTSTATUS_AUTORECONNECT			0x00000002

/* HIGHCONTRAST flags values */
#define HCF_AVAILABLE 					0x00000002
#define HCF_CONFIRMHOTKEY 				0x00000008
#define HCF_HIGHCONTRASTON 				0x00000001
#define HCF_HOTKEYACTIVE 				0x00000004
#define HCF_HOTKEYAVAILABLE				0x00000040
#define HCF_HOTKEYSOUND 				0x00000010
#define HCF_INDICATOR 					0x00000020

/* Server Move/Size Start PDU */
#define RAIL_WMSZ_LEFT					0x0001
#define RAIL_WMSZ_RIGHT					0x0002
#define RAIL_WMSZ_TOP					0x0003
#define RAIL_WMSZ_TOPLEFT				0x0004
#define RAIL_WMSZ_TOPRIGHT				0x0005
#define RAIL_WMSZ_BOTTOM				0x0006
#define RAIL_WMSZ_BOTTOMLEFT				0x0007
#define RAIL_WMSZ_BOTTOMRIGHT				0x0008
#define RAIL_WMSZ_MOVE					0x0009
#define RAIL_WMSZ_KEYMOVE				0x000A
#define RAIL_WMSZ_KEYSIZE				0x000B

/* Language Bar Information PDU */
#define TF_SFT_SHOWNORMAL				0x00000001
#define TF_SFT_DOCK					0x00000002
#define TF_SFT_MINIMIZED				0x00000004
#define TF_SFT_HIDDEN					0x00000008
#define TF_SFT_NOTRANSPARENCY				0x00000010
#define TF_SFT_LOWTRANSPARENCY				0x00000020
#define TF_SFT_HIGHTRANSPARENCY				0x00000040
#define TF_SFT_LABELS					0x00000080
#define TF_SFT_NOLABELS					0x00000100
#define TF_SFT_EXTRAICONSONMINIMIZED			0x00000200
#define TF_SFT_NOEXTRAICONSONMINIMIZED			0x00000400
#define TF_SFT_DESKBAND					0x00000800

struct _UNICODE_STRING
{
	uint16 length;
	uint8* string;
};
typedef struct _UNICODE_STRING UNICODE_STRING;

struct _HIGH_CONTRAST
{
	uint32 flags;
	uint32 colorSchemeLength;
	UNICODE_STRING colorScheme;
};
typedef struct _HIGH_CONTRAST HIGH_CONTRAST;

/* RAIL Orders */

struct _RAIL_HANDSHAKE_ORDER
{
	uint32 buildNumber;
};
typedef struct _RAIL_HANDSHAKE_ORDER RAIL_HANDSHAKE_ORDER;

struct _RAIL_CLIENT_STATUS_ORDER
{
	uint32 flags;
};
typedef struct _RAIL_CLIENT_STATUS_ORDER RAIL_CLIENT_STATUS_ORDER;

struct _RAIL_EXEC_ORDER
{
	uint16 flags;
	UNICODE_STRING exeOrFile;
	UNICODE_STRING workingDir;
	UNICODE_STRING arguments;
};
typedef struct _RAIL_EXEC_ORDER RAIL_EXEC_ORDER;

struct _RAIL_EXEC_RESULT_ORDER
{
	uint16 flags;
	uint16 execResult;
	uint32 rawResult;
	UNICODE_STRING exeOrFile;
};
typedef struct _RAIL_EXEC_RESULT_ORDER RAIL_EXEC_RESULT_ORDER;

struct _RAIL_SYSPARAM_ORDER
{
	uint32 param;
	uint32 params;
	boolean dragFullWindows;
	boolean keyboardCues;
	boolean keyboardPref;
	boolean mouseButtonSwap;
	RECTANGLE_16 workArea;
	RECTANGLE_16 displayChange;
	RECTANGLE_16 taskbarPos;
	HIGH_CONTRAST highContrast;
	boolean setScreenSaveActive;
	boolean setScreenSaveSecure;
};
typedef struct _RAIL_SYSPARAM_ORDER RAIL_SYSPARAM_ORDER;

struct _RAIL_ACTIVATE_ORDER
{
	uint32 windowId;
	boolean enabled;
};
typedef struct _RAIL_ACTIVATE_ORDER RAIL_ACTIVATE_ORDER;

struct _RAIL_SYSMENU_ORDER
{
	uint32 windowId;
	uint16 left;
	uint16 top;
};
typedef struct _RAIL_SYSMENU_ORDER RAIL_SYSMENU_ORDER;

struct _RAIL_SYSCOMMAND_ORDER
{
	uint32 windowId;
	uint16 command;
};
typedef struct _RAIL_SYSCOMMAND_ORDER RAIL_SYSCOMMAND_ORDER;

struct _RAIL_NOTIFY_EVENT_ORDER
{
	uint32 windowId;
	uint32 notifyIconId;
	uint32 message;
};
typedef struct _RAIL_NOTIFY_EVENT_ORDER RAIL_NOTIFY_EVENT_ORDER;

struct _RAIL_MINMAXINFO_ORDER
{
	uint32 windowId;
	uint16 maxWidth;
	uint16 maxHeight;
	uint16 maxPosX;
	uint16 maxPosY;
	uint16 minTrackWidth;
	uint16 minTrackHeight;
	uint16 maxTrackWidth;
	uint16 maxTrackHeight;
};
typedef struct _RAIL_MINMAXINFO_ORDER RAIL_MINMAXINFO_ORDER;

struct _RAIL_LOCALMOVESIZE_ORDER
{
	uint32 windowId;
	boolean isMoveSizeStart;
	uint16 moveSizeType;
	uint16 posX;
	uint16 posY;
};
typedef struct _RAIL_LOCALMOVESIZE_ORDER RAIL_LOCALMOVESIZE_ORDER;

struct _RAIL_WINDOWMOVE_ORDER
{
	uint32 windowId;
	uint16 left;
	uint16 top;
	uint16 right;
	uint16 bottom;
};
typedef struct _RAIL_WINDOWMOVE_ORDER RAIL_WINDOW_MOVE_ORDER;

struct _RAIL_GET_APPID_REQ_ORDER
{
	uint32 windowId;
};
typedef struct _RAIL_GET_APPID_REQ_ORDER RAIL_GET_APPID_REQ_ORDER;

struct _RAIL_GET_APPID_RESP_ORDER
{
	uint32 windowId;
	UNICODE_STRING applicationId;
	uint8 applicationIdBuffer[512];
};
typedef struct _RAIL_GET_APPID_RESP_ORDER RAIL_GET_APPID_RESP_ORDER;

struct _RAIL_LANGBARINFO_ORDER
{
	uint32 languageBarStatus;
};
typedef struct _RAIL_LANGBARINFO_ORDER RAIL_LANGBAR_INFO_ORDER;

/* RAIL Constants */

enum RDP_RAIL_PDU_TYPE
{
	RDP_RAIL_ORDER_EXEC		= 0x0001,
	RDP_RAIL_ORDER_ACTIVATE		= 0x0002,
	RDP_RAIL_ORDER_SYSPARAM		= 0x0003,
	RDP_RAIL_ORDER_SYSCOMMAND	= 0x0004,
	RDP_RAIL_ORDER_HANDSHAKE	= 0x0005,
	RDP_RAIL_ORDER_NOTIFY_EVENT	= 0x0006,
	RDP_RAIL_ORDER_WINDOWMOVE	= 0x0008,
	RDP_RAIL_ORDER_LOCALMOVESIZE	= 0x0009,
	RDP_RAIL_ORDER_MINMAXINFO	= 0x000A,
	RDP_RAIL_ORDER_CLIENTSTATUS	= 0x000B,
	RDP_RAIL_ORDER_SYSMENU		= 0x000C,
	RDP_RAIL_ORDER_LANGBARINFO	= 0x000D,
	RDP_RAIL_ORDER_EXEC_RESULT	= 0x0080,
	RDP_RAIL_ORDER_GET_APPID_REQ	= 0x000E,
	RDP_RAIL_ORDER_GET_APPID_RESP	= 0x000F
};

enum RDP_EVENT_TYPE_RAIL
{
	RDP_EVENT_TYPE_RAIL_CHANNEL_GET_SYSPARAMS = 1,
	RDP_EVENT_TYPE_RAIL_CHANNEL_EXEC_RESULTS,
	RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_SYSPARAM,
	RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_MINMAXINFO,
	RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_LOCALMOVESIZE,
	RDP_EVENT_TYPE_RAIL_CHANNEL_APPID_RESP,
	RDP_EVENT_TYPE_RAIL_CHANNEL_LANGBARINFO,

	RDP_EVENT_TYPE_RAIL_CLIENT_SET_SYSPARAMS,
	RDP_EVENT_TYPE_RAIL_CLIENT_EXEC_REMOTE_APP,
	RDP_EVENT_TYPE_RAIL_CLIENT_ACTIVATE,
	RDP_EVENT_TYPE_RAIL_CLIENT_SYSMENU,
	RDP_EVENT_TYPE_RAIL_CLIENT_SYSCOMMAND,
	RDP_EVENT_TYPE_RAIL_CLIENT_NOTIFY_EVENT,
	RDP_EVENT_TYPE_RAIL_CLIENT_WINDOW_MOVE,
	RDP_EVENT_TYPE_RAIL_CLIENT_APPID_REQ,
	RDP_EVENT_TYPE_RAIL_CLIENT_LANGBARINFO
};

#endif /* __RAIL_GLOBAL_H */

