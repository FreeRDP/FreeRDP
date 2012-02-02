/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Microsoft Virtual Key Code Definitions and Conversion Tables
 *
 * Copyright 2009 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

/* Microsoft Windows Virtual Key Codes: http://msdn.microsoft.com/en-us/library/ms645540.aspx */

#ifndef __VKCODES_H
#define __VKCODES_H

#include <stddef.h>
#include <freerdp/api.h>
#include <freerdp/kbd/layouts.h>

/* Mouse buttons */

#define VK_LBUTTON	0x01 /* Left mouse button */
#define VK_RBUTTON	0x02 /* Right mouse button */
#define VK_CANCEL	0x03 /* Control-break processing */
#define VK_MBUTTON	0x04 /* Middle mouse button (three-button mouse) */
#define VK_XBUTTON1	0x05 /* Windows 2000/XP: X1 mouse button */
#define VK_XBUTTON2	0x06 /* Windows 2000/XP: X2 mouse button */

/* 0x07 is undefined */

#define VK_BACK		0x08 /* BACKSPACE key */
#define VK_TAB		0x09 /* TAB key */

/* 0x0A to 0x0B are reserved */

#define VK_CLEAR	0x0C /* CLEAR key */
#define VK_RETURN	0x0D /* ENTER key */

/* 0x0E to 0x0F are undefined */

#define VK_SHIFT	0x10 /* SHIFT key */
#define VK_CONTROL	0x11 /* CTRL key */
#define VK_MENU		0x12 /* ALT key */
#define VK_PAUSE	0x13 /* PAUSE key */
#define VK_CAPITAL	0x14 /* CAPS LOCK key */
#define VK_KANA		0x15 /* Input Method Editor (IME) Kana mode */
#define VK_HANGUEL	0x15 /* IME Hanguel mode (maintained for compatibility; use #define VK_HANGUL) */
#define VK_HANGUL	0x15 /* IME Hangul mode */

/* 0x16 is undefined */

#define VK_JUNJA	0x17 /* IME Junja mode */
#define VK_FINAL	0x18 /* IME final mode */
#define VK_HANJA	0x19 /* IME Hanja mode */
#define VK_KANJI	0x19 /* IME Kanji mode */

/* 0x1A is undefined */

#define VK_ESCAPE	0x1B /* ESC key */
#define VK_CONVERT	0x1C /* IME convert */
#define VK_NONCONVERT	0x1D /* IME nonconvert */
#define VK_ACCEPT	0x1E /* IME accept */
#define VK_MODECHANGE	0x1F /* IME mode change request */

#define VK_SPACE	0x20 /* SPACEBAR */
#define VK_PRIOR	0x21 /* PAGE UP key */
#define VK_NEXT		0x22 /* PAGE DOWN key */
#define VK_END		0x23 /* END key */
#define VK_HOME		0x24 /* HOME key */
#define VK_LEFT		0x25 /* LEFT ARROW key */
#define VK_UP		0x26 /* UP ARROW key */
#define VK_RIGHT	0x27 /* RIGHT ARROW key */
#define VK_DOWN		0x28 /* DOWN ARROW key */
#define VK_SELECT	0x29 /* SELECT key */
#define VK_PRINT	0x2A /* PRINT key */
#define VK_EXECUTE	0x2B /* EXECUTE key */
#define VK_SNAPSHOT	0x2C /* PRINT SCREEN key */
#define VK_INSERT	0x2D /* INS key */
#define VK_DELETE	0x2E /* DEL key */
#define VK_HELP		0x2F /* HELP key */

/* Digits, the last 4 bits of the code represent the corresponding digit */

#define VK_KEY_0	0x30 /* '0' key */
#define VK_KEY_1	0x31 /* '1' key */
#define VK_KEY_2	0x32 /* '2' key */
#define VK_KEY_3	0x33 /* '3' key */
#define VK_KEY_4	0x34 /* '4' key */
#define VK_KEY_5	0x35 /* '5' key */
#define VK_KEY_6	0x36 /* '6' key */
#define VK_KEY_7	0x37 /* '7' key */
#define VK_KEY_8	0x38 /* '8' key */
#define VK_KEY_9	0x39 /* '9' key */

/* 0x3A to 0x40 are undefined */

/* The alphabet, the code corresponds to the capitalized letter in the ASCII code */

#define VK_KEY_A	0x41 /* 'A' key */
#define VK_KEY_B	0x42 /* 'B' key */
#define VK_KEY_C	0x43 /* 'C' key */
#define VK_KEY_D	0x44 /* 'D' key */
#define VK_KEY_E	0x45 /* 'E' key */
#define VK_KEY_F	0x46 /* 'F' key */
#define VK_KEY_G	0x47 /* 'G' key */
#define VK_KEY_H	0x48 /* 'H' key */
#define VK_KEY_I	0x49 /* 'I' key */
#define VK_KEY_J	0x4A /* 'J' key */
#define VK_KEY_K	0x4B /* 'K' key */
#define VK_KEY_L	0x4C /* 'L' key */
#define VK_KEY_M	0x4D /* 'M' key */
#define VK_KEY_N	0x4E /* 'N' key */
#define VK_KEY_O	0x4F /* 'O' key */
#define VK_KEY_P	0x50 /* 'P' key */
#define VK_KEY_Q	0x51 /* 'Q' key */
#define VK_KEY_R	0x52 /* 'R' key */
#define VK_KEY_S	0x53 /* 'S' key */
#define VK_KEY_T	0x54 /* 'T' key */
#define VK_KEY_U	0x55 /* 'U' key */
#define VK_KEY_V	0x56 /* 'V' key */
#define VK_KEY_W	0x57 /* 'W' key */
#define VK_KEY_X	0x58 /* 'X' key */
#define VK_KEY_Y	0x59 /* 'Y' key */
#define VK_KEY_Z	0x5A /* 'Z' key */

#define VK_LWIN		0x5B /* Left Windows key (Microsoft Natural keyboard) */
#define VK_RWIN		0x5C /* Right Windows key (Natural keyboard) */
#define VK_APPS		0x5D /* Applications key (Natural keyboard) */

/* 0x5E is reserved */

#define VK_SLEEP	0x5F /* Computer Sleep key */

/* Numeric keypad digits, the last four bits of the code represent the corresponding digit */

#define VK_NUMPAD0	0x60 /* Numeric keypad '0' key */
#define VK_NUMPAD1	0x61 /* Numeric keypad '1' key */
#define VK_NUMPAD2	0x62 /* Numeric keypad '2' key */
#define VK_NUMPAD3	0x63 /* Numeric keypad '3' key */
#define VK_NUMPAD4	0x64 /* Numeric keypad '4' key */
#define VK_NUMPAD5	0x65 /* Numeric keypad '5' key */
#define VK_NUMPAD6	0x66 /* Numeric keypad '6' key */
#define VK_NUMPAD7	0x67 /* Numeric keypad '7' key */
#define VK_NUMPAD8	0x68 /* Numeric keypad '8' key */
#define VK_NUMPAD9	0x69 /* Numeric keypad '9' key */

/* Numeric keypad operators and special keys */

#define VK_MULTIPLY	0x6A /* Multiply key */
#define VK_ADD		0x6B /* Add key */
#define VK_SEPARATOR	0x6C /* Separator key */
#define VK_SUBTRACT	0x6D /* Subtract key */
#define VK_DECIMAL	0x6E /* Decimal key */
#define VK_DIVIDE	0x6F /* Divide key */

/* Function keys, from F1 to F24 */

#define VK_F1		0x70 /* F1 key */
#define VK_F2		0x71 /* F2 key */
#define VK_F3		0x72 /* F3 key */
#define VK_F4		0x73 /* F4 key */
#define VK_F5		0x74 /* F5 key */
#define VK_F6		0x75 /* F6 key */
#define VK_F7		0x76 /* F7 key */
#define VK_F8		0x77 /* F8 key */
#define VK_F9		0x78 /* F9 key */
#define VK_F10		0x79 /* F10 key */
#define VK_F11		0x7A /* F11 key */
#define VK_F12		0x7B /* F12 key */
#define VK_F13		0x7C /* F13 key */
#define VK_F14		0x7D /* F14 key */
#define VK_F15		0x7E /* F15 key */
#define VK_F16		0x7F /* F16 key */
#define VK_F17		0x80 /* F17 key */
#define VK_F18		0x81 /* F18 key */
#define VK_F19		0x82 /* F19 key */
#define VK_F20		0x83 /* F20 key */
#define VK_F21		0x84 /* F21 key */
#define VK_F22		0x85 /* F22 key */
#define VK_F23		0x86 /* F23 key */
#define VK_F24		0x87 /* F24 key */

/* 0x88 to 0x8F are unassigned */

#define VK_NUMLOCK	0x90 /* NUM LOCK key */
#define VK_SCROLL	0x91 /* SCROLL LOCK key */

/* 0x92 to 0x96 are OEM specific */
/* 0x97 to 0x9F are unassigned */

/* Modifier keys */

#define VK_LSHIFT	0xA0 /* Left SHIFT key */
#define VK_RSHIFT	0xA1 /* Right SHIFT key */
#define VK_LCONTROL	0xA2 /* Left CONTROL key */
#define VK_RCONTROL	0xA3 /* Right CONTROL key */
#define VK_LMENU	0xA4 /* Left MENU key */
#define VK_RMENU	0xA5 /* Right MENU key */

/* Browser related keys */

#define VK_BROWSER_BACK		0xA6 /* Windows 2000/XP: Browser Back key */
#define VK_BROWSER_FORWARD	0xA7 /* Windows 2000/XP: Browser Forward key */
#define VK_BROWSER_REFRESH	0xA8 /* Windows 2000/XP: Browser Refresh key */
#define VK_BROWSER_STOP		0xA9 /* Windows 2000/XP: Browser Stop key */
#define VK_BROWSER_SEARCH	0xAA /* Windows 2000/XP: Browser Search key */
#define VK_BROWSER_FAVORITES	0xAB /* Windows 2000/XP: Browser Favorites key */
#define VK_BROWSER_HOME		0xAC /* Windows 2000/XP: Browser Start and Home key */

/* Volume related keys */

#define VK_VOLUME_MUTE		0xAD /* Windows 2000/XP: Volume Mute key */
#define VK_VOLUME_DOWN		0xAE /* Windows 2000/XP: Volume Down key */
#define VK_VOLUME_UP		0xAF /* Windows 2000/XP: Volume Up key */

/* Media player related keys */

#define VK_MEDIA_NEXT_TRACK	0xB0 /* Windows 2000/XP: Next Track key */
#define VK_MEDIA_PREV_TRACK	0xB1 /* Windows 2000/XP: Previous Track key */
#define VK_MEDIA_STOP		0xB2 /* Windows 2000/XP: Stop Media key */
#define VK_MEDIA_PLAY_PAUSE	0xB3 /* Windows 2000/XP: Play/Pause Media key */

/* Application launcher keys */

#define VK_LAUNCH_MAIL		0xB4 /* Windows 2000/XP: Start Mail key */
#define VK_LAUNCH_MEDIA_SELECT	0xB5 /* Windows 2000/XP: Select Media key */
#define VK_LAUNCH_APP1		0xB6 /* Windows 2000/XP: Start Application 1 key */
#define VK_LAUNCH_APP2		0xB7 /* Windows 2000/XP: Start Application 2 key */

/* 0xB8 and 0xB9 are reserved */

/* OEM keys */

#define VK_OEM_1	0xBA /* Used for miscellaneous characters; it can vary by keyboard. */
			     /* Windows 2000/XP: For the US standard keyboard, the ';:' key */

#define VK_OEM_PLUS	0xBB /* Windows 2000/XP: For any country/region, the '+' key */
#define VK_OEM_COMMA	0xBC /* Windows 2000/XP: For any country/region, the ',' key */
#define VK_OEM_MINUS	0xBD /* Windows 2000/XP: For any country/region, the '-' key */
#define VK_OEM_PERIOD	0xBE /* Windows 2000/XP: For any country/region, the '.' key */

#define VK_OEM_2	0xBF /* Used for miscellaneous characters; it can vary by keyboard. */
			     /* Windows 2000/XP: For the US standard keyboard, the '/?' key */

#define VK_OEM_3	0xC0 /* Used for miscellaneous characters; it can vary by keyboard. */
			     /* Windows 2000/XP: For the US standard keyboard, the '`~' key */

/* 0xC1 to 0xD7 are reserved */
#define VK_ABNT_C1	0xC1 /* Brazilian (ABNT) Keyboard */
#define VK_ABNT_C2	0xC2 /* Brazilian (ABNT) Keyboard */

/* 0xD8 to 0xDA are unassigned */

#define VK_OEM_4	0xDB /* Used for miscellaneous characters; it can vary by keyboard. */
			     /* Windows 2000/XP: For the US standard keyboard, the '[{' key */

#define VK_OEM_5	0xDC /* Used for miscellaneous characters; it can vary by keyboard. */
			     /* Windows 2000/XP: For the US standard keyboard, the '\|' key */

#define VK_OEM_6	0xDD /* Used for miscellaneous characters; it can vary by keyboard. */
			     /* Windows 2000/XP: For the US standard keyboard, the ']}' key */

#define VK_OEM_7	0xDE /* Used for miscellaneous characters; it can vary by keyboard. */
			     /* Windows 2000/XP: For the US standard keyboard, the 'single-quote/double-quote' key */

#define VK_OEM_8	0xDF /* Used for miscellaneous characters; it can vary by keyboard. */

/* 0xE0 is reserved */
/* 0xE1 is OEM specific */

#define VK_OEM_102	0xE2 /* Windows 2000/XP: Either the angle bracket key or */
			     /* the backslash key on the RT 102-key keyboard */

/* 0xE3 and 0xE4 are OEM specific */

#define VK_PROCESSKEY	0xE5 /* Windows 95/98/Me, Windows NT 4.0, Windows 2000/XP: IME PROCESS key */

/* 0xE6 is OEM specific */

#define VK_PACKET	0xE7	/* Windows 2000/XP: Used to pass Unicode characters as if they were keystrokes. */
				/* The #define VK_PACKET key is the low word of a 32-bit Virtual Key value used */
				/* for non-keyboard input methods. For more information, */
				/* see Remark in KEYBDINPUT, SendInput, WM_KEYDOWN, and WM_KEYUP */

/* 0xE8 is unassigned */
/* 0xE9 to 0xF5 are OEM specific */

#define VK_ATTN		0xF6 /* Attn key */
#define VK_CRSEL	0xF7 /* CrSel key */
#define VK_EXSEL	0xF8 /* ExSel key */
#define VK_EREOF	0xF9 /* Erase EOF key */
#define VK_PLAY		0xFA /* Play key */
#define VK_ZOOM		0xFB /* Zoom key */
#define VK_NONAME	0xFC /* Reserved */
#define VK_PA1		0xFD /* PA1 key */
#define VK_OEM_CLEAR	0xFE /* Clear key */

/* Use the virtual key code as an index in this array in order to get its associated scan code */

typedef struct _virtualKey
{
	/* Windows "scan code", aka keycode in RDP */
	unsigned char scancode;

	/* Windows "extended" flag, boolean */
	unsigned char extended;

	/* Windows virtual key name */
	char *name;

	/* XKB keyname */
	char *x_keyname;
} virtualKey;

static const virtualKey virtualKeyboard[256 + 2] =
{
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_LBUTTON"          , NULL   },
	{ 0x00, 0, "VK_RBUTTON"          , NULL   },
	{ 0x00, 0, "VK_CANCEL"           , NULL   },
	{ 0x00, 0, "VK_MBUTTON"          , NULL   },
	{ 0x00, 0, "VK_XBUTTON1"         , NULL   },
	{ 0x00, 0, "VK_XBUTTON2"         , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x0E, 0, "VK_BACK"             , "BKSP" },
	{ 0x0F, 0, "VK_TAB"              , "TAB"  },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_CLEAR"            , NULL   },
	{ 0x1C, 0, "VK_RETURN"           , "RTRN" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x2A, 0, "VK_SHIFT"            , "LFSH" },
	{ 0x00, 0, "VK_CONTROL"          , NULL   },
	{ 0x38, 0, "VK_MENU"             , "LALT" },
	{ 0x46, 1, "VK_PAUSE"            , "PAUS" },
	{ 0x3A, 0, "VK_CAPITAL"          , "CAPS" },
	{ 0x72, 0, "VK_KANA / VK_HANGUL" , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_JUNJA"            , NULL   },
	{ 0x00, 0, "VK_FINAL"            , NULL   },
	{ 0x71, 0, "VK_HANJA / VK_KANJI" , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x01, 0, "VK_ESCAPE"           , "ESC"  },
	{ 0x00, 0, "VK_CONVERT"          , NULL   },
	{ 0x00, 0, "VK_NONCONVERT"       , NULL   },
	{ 0x00, 0, "VK_ACCEPT"           , NULL   },
	{ 0x00, 0, "VK_MODECHANGE"       , NULL   },
	{ 0x39, 0, "VK_SPACE"            , "SPCE" },
	{ 0x49, 1, "VK_PRIOR"            , "PGUP" },
	{ 0x51, 1, "VK_NEXT"             , "PGDN" },
	{ 0x4F, 1, "VK_END"              , "END"  },
	{ 0x47, 1, "VK_HOME"             , "HOME" },
	{ 0x4B, 1, "VK_LEFT"             , "LEFT" },
	{ 0x48, 1, "VK_UP"               , "UP"   },
	{ 0x4D, 1, "VK_RIGHT"            , "RGHT" },
	{ 0x50, 1, "VK_DOWN"             , "DOWN" },
	{ 0x00, 0, "VK_SELECT"           , NULL   },
	{ 0x37, 1, "VK_PRINT"            , "PRSC" },
	{ 0x37, 1, "VK_EXECUTE"          , NULL   },
	{ 0x37, 1, "VK_SNAPSHOT"         , NULL   },
	{ 0x52, 1, "VK_INSERT"           , "INS"  },
	{ 0x53, 1, "VK_DELETE"           , "DELE" },
	{ 0x63, 0, "VK_HELP"             , NULL   },
	{ 0x0B, 0, "VK_KEY_0"            , "AE10" },
	{ 0x02, 0, "VK_KEY_1"            , "AE01" },
	{ 0x03, 0, "VK_KEY_2"            , "AE02" },
	{ 0x04, 0, "VK_KEY_3"            , "AE03" },
	{ 0x05, 0, "VK_KEY_4"            , "AE04" },
	{ 0x06, 0, "VK_KEY_5"            , "AE05" },
	{ 0x07, 0, "VK_KEY_6"            , "AE06" },
	{ 0x08, 0, "VK_KEY_7"            , "AE07" },
	{ 0x09, 0, "VK_KEY_8"            , "AE08" },
	{ 0x0A, 0, "VK_KEY_9"            , "AE09" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x1E, 0, "VK_KEY_A"            , "AC01" },
	{ 0x30, 0, "VK_KEY_B"            , "AB05" },
	{ 0x2E, 0, "VK_KEY_C"            , "AB03" },
	{ 0x20, 0, "VK_KEY_D"            , "AC03" },
	{ 0x12, 0, "VK_KEY_E"            , "AD03" },
	{ 0x21, 0, "VK_KEY_F"            , "AC04" },
	{ 0x22, 0, "VK_KEY_G"            , "AC05" },
	{ 0x23, 0, "VK_KEY_H"            , "AC06" },
	{ 0x17, 0, "VK_KEY_I"            , "AD08" },
	{ 0x24, 0, "VK_KEY_J"            , "AC07" },
	{ 0x25, 0, "VK_KEY_K"            , "AC08" },
	{ 0x26, 0, "VK_KEY_L"            , "AC09" },
	{ 0x32, 0, "VK_KEY_M"            , "AB07" },
	{ 0x31, 0, "VK_KEY_N"            , "AB06" },
	{ 0x18, 0, "VK_KEY_O"            , "AD09" },
	{ 0x19, 0, "VK_KEY_P"            , "AD10" },
	{ 0x10, 0, "VK_KEY_Q"            , "AD01" },
	{ 0x13, 0, "VK_KEY_R"            , "AD04" },
	{ 0x1F, 0, "VK_KEY_S"            , "AC02" },
	{ 0x14, 0, "VK_KEY_T"            , "AD05" },
	{ 0x16, 0, "VK_KEY_U"            , "AD07" },
	{ 0x2F, 0, "VK_KEY_V"            , "AB04" },
	{ 0x11, 0, "VK_KEY_W"            , "AD02" },
	{ 0x2D, 0, "VK_KEY_X"            , "AB02" },
	{ 0x15, 0, "VK_KEY_Y"            , "AD06" },
	{ 0x2C, 0, "VK_KEY_Z"            , "AB01" },
	{ 0x5B, 1, "VK_LWIN"             , "LWIN" },
	{ 0x5C, 1, "VK_RWIN"             , "RWIN" },
	{ 0x5D, 1, "VK_APPS"             , "COMP" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x5F, 0, "VK_SLEEP"            , NULL   },
	{ 0x52, 0, "VK_NUMPAD0"          , "KP0"  },
	{ 0x4F, 0, "VK_NUMPAD1"          , "KP1"  },
	{ 0x50, 0, "VK_NUMPAD2"          , "KP2"  },
	{ 0x51, 0, "VK_NUMPAD3"          , "KP3"  },
	{ 0x4B, 0, "VK_NUMPAD4"          , "KP4"  },
	{ 0x4C, 0, "VK_NUMPAD5"          , "KP5"  },
	{ 0x4D, 0, "VK_NUMPAD6"          , "KP6"  },
	{ 0x47, 0, "VK_NUMPAD7"          , "KP7"  },
	{ 0x48, 0, "VK_NUMPAD8"          , "KP8"  },
	{ 0x49, 0, "VK_NUMPAD9"          , "KP9"  },
	{ 0x37, 0, "VK_MULTIPLY"         , "KPMU" },
	{ 0x4E, 0, "VK_ADD"              , "KPAD" },
	{ 0x00, 0, "VK_SEPARATOR"        , NULL   },
	{ 0x4A, 0, "VK_SUBTRACT"         , "KPSU" },
	{ 0x53, 0, "VK_DECIMAL"          , "KPDL" },
	{ 0x35, 0, "VK_DIVIDE"           , "KPDV" },
	{ 0x3B, 0, "VK_F1"               , "FK01" },
	{ 0x3C, 0, "VK_F2"               , "FK02" },
	{ 0x3D, 0, "VK_F3"               , "FK03" },
	{ 0x3E, 0, "VK_F4"               , "FK04" },
	{ 0x3F, 0, "VK_F5"               , "FK05" },
	{ 0x40, 0, "VK_F6"               , "FK06" },
	{ 0x41, 0, "VK_F7"               , "FK07" },
	{ 0x42, 0, "VK_F8"               , "FK08" },
	{ 0x43, 0, "VK_F9"               , "FK09" },
	{ 0x44, 0, "VK_F10"              , "FK10" },
	{ 0x57, 0, "VK_F11"              , "FK11" },
	{ 0x58, 0, "VK_F12"              , "FK12" },
	{ 0x64, 0, "VK_F13"              , NULL   },
	{ 0x65, 0, "VK_F14"              , NULL   },
	{ 0x66, 0, "VK_F15"              , NULL   },
	{ 0x67, 0, "VK_F16"              , NULL   },
	{ 0x68, 0, "VK_F17"              , NULL   },
	{ 0x69, 0, "VK_F18"              , NULL   },
	{ 0x6A, 0, "VK_F19"              , NULL   },
	{ 0x6B, 0, "VK_F20"              , NULL   },
	{ 0x6C, 0, "VK_F21"              , NULL   },
	{ 0x6D, 0, "VK_F22"              , NULL   },
	{ 0x6E, 0, "VK_F23"              , NULL   },
	{ 0x6F, 0, "VK_F24"              , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x45, 0, "VK_NUMLOCK"          , "NMLK" },
	{ 0x46, 0, "VK_SCROLL"           , "SCLK" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x2A, 0, "VK_LSHIFT"           , NULL   },
	{ 0x36, 0, "VK_RSHIFT"           , "RTSH" },
	{ 0x1D, 0, "VK_LCONTROL"         , "LCTL" },
	{ 0x1D, 1, "VK_RCONTROL"         , "RCTL" },
	{ 0x38, 0, "VK_LMENU"            , NULL   },
	{ 0x38, 1, "VK_RMENU"            , "RALT" },
	{ 0x00, 0, "VK_BROWSER_BACK"     , NULL   },
	{ 0x00, 0, "VK_BROWSER_FORWARD"  , NULL   },
	{ 0x00, 0, "VK_BROWSER_REFRESH"  , NULL   },
	{ 0x00, 0, "VK_BROWSER_STOP"     , NULL   },
	{ 0x00, 0, "VK_BROWSER_SEARCH"   , NULL   },
	{ 0x00, 0, "VK_BROWSER_FAVORITES", NULL   },
	{ 0x00, 0, "VK_BROWSER_HOME"     , NULL   },
	{ 0x00, 0, "VK_VOLUME_MUTE"      , NULL   },
	{ 0x00, 0, "VK_VOLUME_DOWN"      , NULL   },
	{ 0x00, 0, "VK_VOLUME_UP"        , NULL   },
	{ 0x00, 0, "VK_MEDIA_NEXT_TRACK" , NULL   },
	{ 0x00, 0, "VK_MEDIA_PREV_TRACK" , NULL   },
	{ 0x00, 0, "VK_MEDIA_STOP"       , NULL   },
	{ 0x00, 0, "VK_MEDIA_PLAY_PAUSE" , NULL   },
	{ 0x00, 0, "VK_LAUNCH_MAIL"      , NULL   },
	{ 0x00, 0, "VK_MEDIA_SELECT"     , NULL   },
	{ 0x00, 0, "VK_LAUNCH_APP1"      , NULL   },
	{ 0x00, 0, "VK_LAUNCH_APP2"      , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x27, 0, "VK_OEM_1"            , "AC10" },
	{ 0x0D, 0, "VK_OEM_PLUS"         , "AE12" },
	{ 0x33, 0, "VK_OEM_COMMA"        , "AB08" },
	{ 0x0C, 0, "VK_OEM_MINUS"        , "AE11" },
	{ 0x34, 0, "VK_OEM_PERIOD"       , "AB09" },
	{ 0x35, 0, "VK_OEM_2"            , "AB10" },
	{ 0x29, 0, "VK_OEM_3"            , "TLDE" },
	{ 0x73, 0, "VK_ABNT_C1"          , "AB11" },
	{ 0x7E, 0, "VK_ABNT_C2"          , "I129" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x1A, 0, "VK_OEM_4"            , "AD11" },
	{ 0x2B, 0, "VK_OEM_5"            , "BKSL" },
	{ 0x1B, 0, "VK_OEM_6"            , "AD12" },
	{ 0x28, 0, "VK_OEM_7"            , "AC11" },
	{ 0x1D, 0, "VK_OEM_8"            , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x56, 0, "VK_OEM_102"          , "LSGT" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_PROCESSKEY"       , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_PACKET"           , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_ATTN"             , NULL   },
	{ 0x00, 0, "VK_CRSEL"            , NULL   },
	{ 0x00, 0, "VK_EXSEL"            , NULL   },
	{ 0x00, 0, "VK_EREOF"            , NULL   },
	{ 0x00, 0, "VK_PLAY"             , NULL   },
	{ 0x62, 0, "VK_ZOOM"             , NULL   },
	{ 0x00, 0, "VK_NONAME"           , NULL   },
	{ 0x00, 0, "VK_PA1"              , NULL   },
	{ 0x00, 0, "VK_OEM_CLEAR"        , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	/* end of 256 VK entries */
	{ 0x54, 0, ""                    , "LVL3" },
	{ 0x1C, 1, ""                    , "KPEN" },
};

#endif /* __VKCODES_H */
