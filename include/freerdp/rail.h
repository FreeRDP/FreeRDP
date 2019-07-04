/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_RAIL_GLOBAL_H
#define FREERDP_RAIL_GLOBAL_H

#include <winpr/wnd.h>
#include <winpr/windows.h>

#include <freerdp/types.h>

#define RAIL_SVC_CHANNEL_NAME "rail"

/* DEPRECATED: RAIL PDU flags use the spec conformant naming with TS_ prefix */
#define RAIL_EXEC_FLAG_EXPAND_WORKINGDIRECTORY 0x0001
#define RAIL_EXEC_FLAG_TRANSLATE_FILES 0x0002
#define RAIL_EXEC_FLAG_FILE 0x0004
#define RAIL_EXEC_FLAG_EXPAND_ARGUMENTS 0x0008

/* RAIL PDU flags */
#define TS_RAIL_EXEC_FLAG_EXPAND_WORKINGDIRECTORY 0x0001
#define TS_RAIL_EXEC_FLAG_TRANSLATE_FILES 0x0002
#define TS_RAIL_EXEC_FLAG_FILE 0x0004
#define TS_RAIL_EXEC_FLAG_EXPAND_ARGUMENTS 0x0008
#define TS_RAIL_EXEC_FLAG_APP_USER_MODEL_ID 0x0010

/* Notification Icon Balloon Tooltip */
#define NIIF_NONE 0x00000000
#define NIIF_INFO 0x00000001
#define NIIF_WARNING 0x00000002
#define NIIF_ERROR 0x00000003
#define NIIF_NOSOUND 0x00000010
#define NIIF_LARGE_ICON 0x00000020

/* Client Execute PDU Flags */
#define RAIL_EXEC_FLAG_EXPAND_WORKING_DIRECTORY 0x0001
#define RAIL_EXEC_FLAG_TRANSLATE_FILES 0x0002
#define RAIL_EXEC_FLAG_FILE 0x0004
#define RAIL_EXEC_FLAG_EXPAND_ARGUMENTS 0x0008
#define RAIL_EXEC_FLAG_APP_USER_MODEL_ID 0x0010

/* Server Execute Result PDU */
#define RAIL_EXEC_S_OK 0x0000
#define RAIL_EXEC_E_HOOK_NOT_LOADED 0x0001
#define RAIL_EXEC_E_DECODE_FAILED 0x0002
#define RAIL_EXEC_E_NOT_IN_ALLOWLIST 0x0003
#define RAIL_EXEC_E_FILE_NOT_FOUND 0x0005
#define RAIL_EXEC_E_FAIL 0x0006
#define RAIL_EXEC_E_SESSION_LOCKED 0x0007

/* DEPRECATED: Server System Parameters Update PDU
 * use the spec conformant naming scheme from winpr/windows.h
 */
#define SPI_SET_SCREEN_SAVE_ACTIVE 0x00000011
#define SPI_SET_SCREEN_SAVE_SECURE 0x00000077

/*Bit mask values for SPI_ parameters*/
enum SPI_MASK
{
	SPI_MASK_SET_DRAG_FULL_WINDOWS = 0x00000001,
	SPI_MASK_SET_KEYBOARD_CUES = 0x00000002,
	SPI_MASK_SET_KEYBOARD_PREF = 0x00000004,
	SPI_MASK_SET_MOUSE_BUTTON_SWAP = 0x00000008,
	SPI_MASK_SET_WORK_AREA = 0x00000010,
	SPI_MASK_DISPLAY_CHANGE = 0x00000020,
	SPI_MASK_TASKBAR_POS = 0x00000040,
	SPI_MASK_SET_HIGH_CONTRAST = 0x00000080,
	SPI_MASK_SET_SCREEN_SAVE_ACTIVE = 0x00000100,
	SPI_MASK_SET_SET_SCREEN_SAVE_SECURE = 0x00000200,
	SPI_MASK_SET_CARET_WIDTH = 0x00000400,
	SPI_MASK_SET_STICKY_KEYS = 0x00000800,
	SPI_MASK_SET_TOGGLE_KEYS = 0x00001000,
	SPI_MASK_SET_FILTER_KEYS = 0x00002000,
};

/* Client System Parameters Update PDU
 * some are defined in winuser.h (winpr/windows.h wrapper)
 */
#define SPI_SET_DRAG_FULL_WINDOWS 0x00000025
#define SPI_SET_KEYBOARD_CUES 0x0000100B
#define SPI_SET_KEYBOARD_PREF 0x00000045
#define SPI_SET_MOUSE_BUTTON_SWAP 0x00000021
#define SPI_SET_WORK_AREA 0x0000002F
#define SPI_DISPLAY_CHANGE 0x0000F001
#define SPI_TASKBAR_POS 0x0000F000
#define SPI_SET_HIGH_CONTRAST 0x00000043

/* Client System Command PDU */
#define SC_SIZE 0xF000
#define SC_MOVE 0xF010
#define SC_MINIMIZE 0xF020
#define SC_MAXIMIZE 0xF030
#define SC_CLOSE 0xF060
#define SC_KEYMENU 0xF100
#define SC_RESTORE 0xF120
#define SC_DEFAULT 0xF160

/* Client Notify Event PDU */
#ifndef _WIN32
#define NIN_SELECT 0x00000400
#define NIN_KEYSELECT 0x00000401
#define NIN_BALLOONSHOW 0x00000402
#define NIN_BALLOONHIDE 0x00000403
#define NIN_BALLOONTIMEOUT 0x00000404
#define NIN_BALLOONUSERCLICK 0x00000405
#else
#include <shellapi.h>
#endif

/* DEPRECATED: Client Information PDU
 * use the spec conformant naming scheme TS_ below
 */
#define RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE 0x00000001
#define RAIL_CLIENTSTATUS_AUTORECONNECT 0x00000002

/* Client Information PDU */
#define TS_RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE 0x00000001
#define TS_RAIL_CLIENTSTATUS_AUTORECONNECT 0x00000002
#define TS_RAIL_CLIENTSTATUS_ZORDER_SYNC 0x00000004
#define TS_RAIL_CLIENTSTATUS_WINDOW_RESIZE_MARGIN_SUPPORTED 0x00000010
#define TS_RAIL_CLIENTSTATUS_HIGH_DPI_ICONS_SUPPORTED 0x00000020
#define TS_RAIL_CLIENTSTATUS_APPBAR_REMOTING_SUPPORTED 0x00000040
#define TS_RAIL_CLIENTSTATUS_POWER_DISPLAY_REQUEST_SUPPORTED 0x00000080
#define TS_RAIL_CLIENTSTATUS_BIDIRECTIONAL_CLOAK_SUPPORTED 0x00000200

/* Server Move/Size Start PDU */
#define RAIL_WMSZ_LEFT 0x0001
#define RAIL_WMSZ_RIGHT 0x0002
#define RAIL_WMSZ_TOP 0x0003
#define RAIL_WMSZ_TOPLEFT 0x0004
#define RAIL_WMSZ_TOPRIGHT 0x0005
#define RAIL_WMSZ_BOTTOM 0x0006
#define RAIL_WMSZ_BOTTOMLEFT 0x0007
#define RAIL_WMSZ_BOTTOMRIGHT 0x0008
#define RAIL_WMSZ_MOVE 0x0009
#define RAIL_WMSZ_KEYMOVE 0x000A
#define RAIL_WMSZ_KEYSIZE 0x000B

/* Language Bar Information PDU */
#define TF_SFT_SHOWNORMAL 0x00000001
#define TF_SFT_DOCK 0x00000002
#define TF_SFT_MINIMIZED 0x00000004
#define TF_SFT_HIDDEN 0x00000008
#define TF_SFT_NOTRANSPARENCY 0x00000010
#define TF_SFT_LOWTRANSPARENCY 0x00000020
#define TF_SFT_HIGHTRANSPARENCY 0x00000040
#define TF_SFT_LABELS 0x00000080
#define TF_SFT_NOLABELS 0x00000100
#define TF_SFT_EXTRAICONSONMINIMIZED 0x00000200
#define TF_SFT_NOEXTRAICONSONMINIMIZED 0x00000400
#define TF_SFT_DESKBAND 0x00000800

/* DEPRECATED: Extended Handshake Flags
 * use the spec conformant naming scheme TS_ below
 */
#define RAIL_ORDER_HANDSHAKEEX_FLAGS_HIDEF 0x00000001
#define RAIL_ORDER_HANDSHAKE_EX_FLAGS_EXTENDED_SPI_SUPPORTED 0x00000002
#define RAIL_ORDER_HANDSHAKE_EX_FLAGS_SNAP_ARRANGE_SUPPORTED 0x00000004

/* Extended Handshake Flags */
#define TS_RAIL_ORDER_HANDSHAKEEX_FLAGS_HIDEF 0x00000001
#define TS_RAIL_ORDER_HANDSHAKE_EX_FLAGS_EXTENDED_SPI_SUPPORTED 0x00000002
#define TS_RAIL_ORDER_HANDSHAKE_EX_FLAGS_SNAP_ARRANGE_SUPPORTED 0x00000004

/* Language Profile Information Flags */
#define TF_PROFILETYPE_INPUTPROCESSOR 0x00000001
#define TF_PROFILETYPE_KEYBOARDLAYOUT 0x00000002

/* LanguageProfileCLSID and ProfileGUID */
#ifndef _WIN32
#define GUID_NULL                                                                  \
	{                                                                              \
		0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 \
	}
#else
#include <cguid.h>
#endif
#define GUID_MSIME_JPN                                                             \
	{                                                                              \
		0x03B5835F, 0xF03C, 0x411B, 0x9C, 0xE2, 0xAA, 0x23, 0xE1, 0x17, 0x1E, 0x36 \
	}
#define GUID_MSIME_KOR                                                             \
	{                                                                              \
		0xA028AE76, 0x01B1, 0x46C2, 0x99, 0xC4, 0xAC, 0xD9, 0x85, 0x8A, 0xE0, 0x02 \
	}
#define GUID_CHSIME                                                                \
	{                                                                              \
		0x81D4E9C9, 0x1D3B, 0x41BC, 0x9E, 0x6C, 0x4B, 0x40, 0xBF, 0x79, 0xE3, 0x5E \
	}
#define GUID_CHTIME                                                                \
	{                                                                              \
		0x531FDEBF, 0x9B4C, 0x4A43, 0xA2, 0xAA, 0x96, 0x0E, 0x8F, 0xCD, 0xC7, 0x32 \
	}
#define GUID_PROFILE_NEWPHONETIC                                                   \
	{                                                                              \
		0xB2F9C502, 0x1742, 0x11D4, 0x97, 0x90, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E \
	}
#define GUID_PROFILE_CHANGJIE                                                      \
	{                                                                              \
		0x4BDF9F03, 0xC7D3, 0x11D4, 0xB2, 0xAB, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E \
	}
#define GUID_PROFILE_QUICK                                                         \
	{                                                                              \
		0x6024B45F, 0x5C54, 0x11D4, 0xB9, 0x21, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E \
	}
#define GUID_PROFILE_CANTONESE                                                     \
	{                                                                              \
		0x0AEC109C, 0x7E96, 0x11D4, 0xB2, 0xEF, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E \
	}
#define GUID_PROFILE_PINYIN                                                        \
	{                                                                              \
		0xF3BA9077, 0x6C7E, 0x11D4, 0x97, 0xFA, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E \
	}
#define GUID_PROFILE_SIMPLEFAST                                                    \
	{                                                                              \
		0xFA550B04, 0x5AD7, 0x411F, 0xA5, 0xAC, 0xCA, 0x03, 0x8E, 0xC5, 0x15, 0xD7 \
	}
#define GUID_GUID_PROFILE_MSIME_JPN                                                \
	{                                                                              \
		0xA76C93D9, 0x5523, 0x4E90, 0xAA, 0xFA, 0x4D, 0xB1, 0x12, 0xF9, 0xAC, 0x76 \
	}
#define GUID_PROFILE_MSIME_KOR                                                     \
	{                                                                              \
		0xB5FE1F02, 0xD5F2, 0x4445, 0x9C, 0x03, 0xC5, 0x68, 0xF2, 0x3C, 0x99, 0xA1 \
	}

/* ImeState */
#define IME_STATE_CLOSED 0x00000000
#define IME_STATE_OPEN 0x00000001

/* ImeConvMode */
#ifndef _IME_CMODES_
#define IME_CMODE_NATIVE 0x00000001
#define IME_CMODE_KATAKANA 0x00000002
#define IME_CMODE_FULLSHAPE 0x00000008
#define IME_CMODE_ROMAN 0x00000010
#define IME_CMODE_CHARCODE 0x00000020
#define IME_CMODE_HANJACONVERT 0x00000040
#define IME_CMODE_SOFTKBD 0x00000080
#define IME_CMODE_NOCONVERSION 0x00000100
#define IME_CMODE_EUDC 0x00000200
#define IME_CMODE_SYMBOL 0x00000400
#define IME_CMODE_FIXED 0x00000800
#endif

/* ImeSentenceMode */
#ifndef _IMM_
#define IME_SMODE_NONE 0x00000000
#define IME_SMODE_PLURALCASE 0x00000001
#define IME_SMODE_SINGLECONVERT 0x00000002
#define IME_SMODE_AUTOMATIC 0x00000004
#define IME_SMODE_PHRASEPREDICT 0x00000008
#define IME_SMODE_CONVERSATION 0x00000010
#endif

/* KANAMode */
#define KANA_MODE_OFF 0x00000000
#define KANA_MODE_ON 0x00000001

/* Taskbar */
#define RAIL_TASKBAR_MSG_TAB_REGISTER 0x00000001
#define RAIL_TASKBAR_MSG_TAB_UNREGISTER 0x00000002
#define RAIL_TASKBAR_MSG_TAB_ORDER 0x00000003
#define RAIL_TASKBAR_MSG_TAB_ACTIVE 0x00000004
#define RAIL_TASKBAR_MSG_TAB_PROPERTIES 0x00000005

/* Taskbar body */
#define RAIL_TASKBAR_MSG_TAB_REGISTER 0x00000001
#define RAIL_TASKBAR_MSG_TAB_UNREGISTER 0x00000002
#define RAIL_TASKBAR_MSG_TAB_ORDER 0x00000003
#define RAIL_TASKBAR_MSG_TAB_ACTIVE 0x00000004
#define RAIL_TASKBAR_MSG_TAB_PROPERTIES 0x00000005

struct _RAIL_UNICODE_STRING
{
	UINT16 length;
	BYTE* string;
};
typedef struct _RAIL_UNICODE_STRING RAIL_UNICODE_STRING;

struct _RAIL_HIGH_CONTRAST
{
	UINT32 flags;
	UINT32 colorSchemeLength;
	RAIL_UNICODE_STRING colorScheme;
};
typedef struct _RAIL_HIGH_CONTRAST RAIL_HIGH_CONTRAST;

/* RAIL Orders */

struct _RAIL_HANDSHAKE_ORDER
{
	UINT32 buildNumber;
};
typedef struct _RAIL_HANDSHAKE_ORDER RAIL_HANDSHAKE_ORDER;

struct _RAIL_HANDSHAKE_EX_ORDER
{
	UINT32 buildNumber;
	UINT32 railHandshakeFlags;
};
typedef struct _RAIL_HANDSHAKE_EX_ORDER RAIL_HANDSHAKE_EX_ORDER;

struct _RAIL_CLIENT_STATUS_ORDER
{
	UINT32 flags;
};
typedef struct _RAIL_CLIENT_STATUS_ORDER RAIL_CLIENT_STATUS_ORDER;

struct _RAIL_EXEC_ORDER
{
	UINT16 flags;
	char* RemoteApplicationProgram;
	char* RemoteApplicationWorkingDir;
	char* RemoteApplicationArguments;
};
typedef struct _RAIL_EXEC_ORDER RAIL_EXEC_ORDER;

struct _RAIL_EXEC_RESULT_ORDER
{
	UINT16 flags;
	UINT16 execResult;
	UINT32 rawResult;
	RAIL_UNICODE_STRING exeOrFile;
};
typedef struct _RAIL_EXEC_RESULT_ORDER RAIL_EXEC_RESULT_ORDER;

struct _TS_FILTERKEYS
{
	UINT32 Flags;
	UINT32 WaitTime;
	UINT32 DelayTime;
	UINT32 RepeatTime;
	UINT32 BounceTime;
};
typedef struct _TS_FILTERKEYS TS_FILTERKEYS;

struct _RAIL_SYSPARAM_ORDER
{
	UINT32 param;
	UINT32 params;
	BOOL dragFullWindows;
	BOOL keyboardCues;
	BOOL keyboardPref;
	BOOL mouseButtonSwap;
	RECTANGLE_16 workArea;
	RECTANGLE_16 displayChange;
	RECTANGLE_16 taskbarPos;
	RAIL_HIGH_CONTRAST highContrast;
	UINT32 caretWidth;
	UINT32 stickyKeys;
	UINT32 toggleKeys;
	TS_FILTERKEYS filterKeys;
	BOOL setScreenSaveActive;
	BOOL setScreenSaveSecure;
};
typedef struct _RAIL_SYSPARAM_ORDER RAIL_SYSPARAM_ORDER;

struct _RAIL_ACTIVATE_ORDER
{
	UINT32 windowId;
	BOOL enabled;
};
typedef struct _RAIL_ACTIVATE_ORDER RAIL_ACTIVATE_ORDER;

struct _RAIL_SYSMENU_ORDER
{
	UINT32 windowId;
	INT16 left;
	INT16 top;
};
typedef struct _RAIL_SYSMENU_ORDER RAIL_SYSMENU_ORDER;

struct _RAIL_SYSCOMMAND_ORDER
{
	UINT32 windowId;
	UINT16 command;
};
typedef struct _RAIL_SYSCOMMAND_ORDER RAIL_SYSCOMMAND_ORDER;

struct _RAIL_NOTIFY_EVENT_ORDER
{
	UINT32 windowId;
	UINT32 notifyIconId;
	UINT32 message;
};
typedef struct _RAIL_NOTIFY_EVENT_ORDER RAIL_NOTIFY_EVENT_ORDER;

struct _RAIL_MINMAXINFO_ORDER
{
	UINT32 windowId;
	INT16 maxWidth;
	INT16 maxHeight;
	INT16 maxPosX;
	INT16 maxPosY;
	INT16 minTrackWidth;
	INT16 minTrackHeight;
	INT16 maxTrackWidth;
	INT16 maxTrackHeight;
};
typedef struct _RAIL_MINMAXINFO_ORDER RAIL_MINMAXINFO_ORDER;

struct _RAIL_LOCALMOVESIZE_ORDER
{
	UINT32 windowId;
	BOOL isMoveSizeStart;
	UINT16 moveSizeType;
	INT16 posX;
	INT16 posY;
};
typedef struct _RAIL_LOCALMOVESIZE_ORDER RAIL_LOCALMOVESIZE_ORDER;

struct _RAIL_WINDOWMOVE_ORDER
{
	UINT32 windowId;
	INT16 left;
	INT16 top;
	INT16 right;
	INT16 bottom;
};
typedef struct _RAIL_WINDOWMOVE_ORDER RAIL_WINDOW_MOVE_ORDER;

struct _RAIL_GET_APPID_REQ_ORDER
{
	UINT32 windowId;
};
typedef struct _RAIL_GET_APPID_REQ_ORDER RAIL_GET_APPID_REQ_ORDER;

struct _RAIL_GET_APPID_RESP_ORDER
{
	UINT32 windowId;
	WCHAR applicationId[260];
};
typedef struct _RAIL_GET_APPID_RESP_ORDER RAIL_GET_APPID_RESP_ORDER;

struct _RAIL_LANGBAR_INFO_ORDER
{
	UINT32 languageBarStatus;
};
typedef struct _RAIL_LANGBAR_INFO_ORDER RAIL_LANGBAR_INFO_ORDER;

struct _RAIL_COMPARTMENT_INFO_ORDER
{
	UINT32 ImeState;
	UINT32 ImeConvMode;
	UINT32 ImeSentenceMode;
	UINT32 KanaMode;
};
typedef struct _RAIL_COMPARTMENT_INFO_ORDER RAIL_COMPARTMENT_INFO_ORDER;

struct _RAIL_ZORDER_SYNC
{
	UINT32 windowIdMarker;
};
typedef struct _RAIL_ZORDER_SYNC RAIL_ZORDER_SYNC;

struct _RAIL_CLOAK
{
	UINT32 windowId;
	BOOL cloak;
};
typedef struct _RAIL_CLOAK RAIL_CLOAK;

struct _RAIL_POWER_DISPLAY_REQUEST
{
	UINT32 active;
};
typedef struct _RAIL_POWER_DISPLAY_REQUEST RAIL_POWER_DISPLAY_REQUEST;

struct _RAIL_TASKBAR_INFO_ORDER
{
	UINT32 TaskbarMessage;
	UINT32 WindowIdTab;
	UINT32 Body;
};
typedef struct _RAIL_TASKBAR_INFO_ORDER RAIL_TASKBAR_INFO_ORDER;

struct _RAIL_LANGUAGEIME_INFO_ORDER
{
	UINT32 ProfileType;
	UINT32 LanguageID;
	GUID LanguageProfileCLSID;
	GUID ProfileGUID;
	UINT32 KeyboardLayout;
};
typedef struct _RAIL_LANGUAGEIME_INFO_ORDER RAIL_LANGUAGEIME_INFO_ORDER;

struct _RAIL_SNAP_ARRANGE
{
	UINT32 windowId;
	INT16 left;
	INT16 top;
	INT16 right;
	INT16 bottom;
};
typedef struct _RAIL_SNAP_ARRANGE RAIL_SNAP_ARRANGE;

struct _RAIL_GET_APPID_RESP_EX
{
	UINT32 windowID;
	WCHAR applicationID[520 / sizeof(WCHAR)];
	UINT32 processId;
	WCHAR processImageName[520 / sizeof(WCHAR)];
};
typedef struct _RAIL_GET_APPID_RESP_EX RAIL_GET_APPID_RESP_EX;

/* DEPRECATED: RAIL Constants
 * use the spec conformant naming scheme TS_ below
 */

#define RDP_RAIL_ORDER_EXEC 0x0001
#define RDP_RAIL_ORDER_ACTIVATE 0x0002
#define RDP_RAIL_ORDER_SYSPARAM 0x0003
#define RDP_RAIL_ORDER_SYSCOMMAND 0x0004
#define RDP_RAIL_ORDER_HANDSHAKE 0x0005
#define RDP_RAIL_ORDER_NOTIFY_EVENT 0x0006
#define RDP_RAIL_ORDER_WINDOWMOVE 0x0008
#define RDP_RAIL_ORDER_LOCALMOVESIZE 0x0009
#define RDP_RAIL_ORDER_MINMAXINFO 0x000A
#define RDP_RAIL_ORDER_CLIENTSTATUS 0x000B
#define RDP_RAIL_ORDER_SYSMENU 0x000C
#define RDP_RAIL_ORDER_LANGBARINFO 0x000D
#define RDP_RAIL_ORDER_EXEC_RESULT 0x0080
#define RDP_RAIL_ORDER_GET_APPID_REQ 0x000E
#define RDP_RAIL_ORDER_GET_APPID_RESP 0x000F
#define RDP_RAIL_ORDER_LANGUAGEIMEINFO 0x0011
#define RDP_RAIL_ORDER_COMPARTMENTINFO 0x0012
#define RDP_RAIL_ORDER_HANDSHAKE_EX 0x0013
#define RDP_RAIL_ORDER_ZORDER_SYNC 0x0014
#define RDP_RAIL_ORDER_CLOAK 0x0015
#define RDP_RAIL_ORDER_POWER_DISPLAY_REQUEST 0x0016
#define RDP_RAIL_ORDER_SNAP_ARRANGE 0x0017
#define RDP_RAIL_ORDER_GET_APPID_RESP_EX 0x0018

/* RAIL Constants */

#define TS_RAIL_ORDER_EXEC 0x0001
#define TS_RAIL_ORDER_ACTIVATE 0x0002
#define TS_RAIL_ORDER_SYSPARAM 0x0003
#define TS_RAIL_ORDER_SYSCOMMAND 0x0004
#define TS_RAIL_ORDER_HANDSHAKE 0x0005
#define TS_RAIL_ORDER_NOTIFY_EVENT 0x0006
#define TS_RAIL_ORDER_WINDOWMOVE 0x0008
#define TS_RAIL_ORDER_LOCALMOVESIZE 0x0009
#define TS_RAIL_ORDER_MINMAXINFO 0x000A
#define TS_RAIL_ORDER_CLIENTSTATUS 0x000B
#define TS_RAIL_ORDER_SYSMENU 0x000C
#define TS_RAIL_ORDER_LANGBARINFO 0x000D
#define TS_RAIL_ORDER_GET_APPID_REQ 0x000E
#define TS_RAIL_ORDER_GET_APPID_RESP 0x000F
#define TS_RAIL_ORDER_TASKBARINFO 0x0010
#define TS_RAIL_ORDER_LANGUAGEIMEINFO 0x0011
#define TS_RAIL_ORDER_COMPARTMENTINFO 0x0012
#define TS_RAIL_ORDER_HANDSHAKE_EX 0x0013
#define TS_RAIL_ORDER_ZORDER_SYNC 0x0014
#define TS_RAIL_ORDER_CLOAK 0x0015
#define TS_RAIL_ORDER_POWER_DISPLAY_REQUEST 0x0016
#define TS_RAIL_ORDER_SNAP_ARRANGE 0x0017
#define TS_RAIL_ORDER_GET_APPID_RESP_EX 0x0018
#define TS_RAIL_ORDER_EXEC_RESULT 0x0080

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL rail_read_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string);
	FREERDP_API BOOL utf8_string_to_rail_string(const char* string,
	                                            RAIL_UNICODE_STRING* unicode_string);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_RAIL_GLOBAL_H */
