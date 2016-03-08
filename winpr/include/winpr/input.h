/**
 * WinPR: Windows Portable Runtime
 * Input Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_INPUT_H
#define WINPR_INPUT_H


#include <winpr/winpr.h>
#include <winpr/wtypes.h>

/**
 * Key Flags
 */

#define KBDEXT		(USHORT) 0x0100
#define KBDMULTIVK	(USHORT) 0x0200
#define KBDSPECIAL	(USHORT) 0x0400
#define KBDNUMPAD	(USHORT) 0x0800
#define KBDUNICODE	(USHORT) 0x1000
#define KBDINJECTEDVK	(USHORT) 0x2000
#define KBDMAPPEDVK	(USHORT) 0x4000
#define KBDBREAK	(USHORT) 0x8000

/*
 * Virtual Key Codes (Windows):
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731/
 * http://msdn.microsoft.com/en-us/library/ms927178.aspx
 */

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

/* 0x1A is undefined, use it for missing Hiragana/Katakana Toggle */

#define VK_HKTG 	0x1A /* Hiragana/Katakana toggle */
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

#define VK_POWER	0x5E /* Power key */

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
#define VK_MEDIA_SELECT		0xB5 /* Windows 2000/XP: Select Media key */
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

#define VK_OEM_AX	0xE1 /* AX key on Japanese AX keyboard */

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

#define VK_OEM_RESET			0xE9
#define VK_OEM_JUMP			0xEA
#define VK_OEM_PA1			0xEB
#define VK_OEM_PA2			0xEC
#define VK_OEM_PA3			0xED
#define VK_OEM_WSCTRL			0xEE
#define VK_OEM_CUSEL			0xEF
#define VK_OEM_ATTN			0xF0
#define VK_OEM_FINISH			0xF1
#define VK_OEM_COPY			0xF2
#define VK_OEM_AUTO			0xF3
#define VK_OEM_ENLW			0xF4
#define VK_OEM_BACKTAB			0xF5

#define VK_ATTN				0xF6 /* Attn key */
#define VK_CRSEL			0xF7 /* CrSel key */
#define VK_EXSEL			0xF8 /* ExSel key */
#define VK_EREOF			0xF9 /* Erase EOF key */
#define VK_PLAY				0xFA /* Play key */
#define VK_ZOOM				0xFB /* Zoom key */
#define VK_NONAME			0xFC /* Reserved */
#define VK_PA1				0xFD /* PA1 key */
#define VK_OEM_CLEAR			0xFE /* Clear key */

#define VK_NONE				0xFF /* no key */

/**
 * For East Asian Input Method Editors (IMEs)
 * the following additional virtual keyboard definitions must be observed.
 */

#define VK_DBE_ALPHANUMERIC		0xF0 /* Changes the mode to alphanumeric. */
#define VK_DBE_KATAKANA			0xF1 /* Changes the mode to Katakana. */
#define VK_DBE_HIRAGANA			0xF2 /* Changes the mode to Hiragana. */
#define VK_DBE_SBCSCHAR			0xF3 /* Changes the mode to single-byte characters. */
#define VK_DBE_DBCSCHAR			0xF4 /* Changes the mode to double-byte characters. */
#define VK_DBE_ROMAN			0xF5 /* Changes the mode to Roman characters. */
#define VK_DBE_NOROMAN			0xF6 /* Changes the mode to non-Roman characters. */
#define VK_DBE_ENTERWORDREGISTERMODE	0xF7 /* Activates the word registration dialog box. */
#define VK_DBE_ENTERIMECONFIGMODE	0xF8 /* Activates a dialog box for setting up an IME environment. */
#define VK_DBE_FLUSHSTRING		0xF9 /* Deletes the undetermined string without determining it. */
#define VK_DBE_CODEINPUT		0xFA /* Changes the mode to code input. */
#define VK_DBE_NOCODEINPUT		0xFB /* Changes the mode to no-code input. */

/*
 * Virtual Scan Codes
 */

/**
 * Keyboard Type 4
 */

#define KBD4_T00		VK_NONE
#define KBD4_T01		VK_ESCAPE
#define KBD4_T02		VK_KEY_1
#define KBD4_T03		VK_KEY_2
#define KBD4_T04		VK_KEY_3
#define KBD4_T05		VK_KEY_4
#define KBD4_T06		VK_KEY_5
#define KBD4_T07		VK_KEY_6
#define KBD4_T08		VK_KEY_7
#define KBD4_T09		VK_KEY_8
#define KBD4_T0A		VK_KEY_9
#define KBD4_T0B		VK_KEY_0
#define KBD4_T0C		VK_OEM_MINUS
#define KBD4_T0D		VK_OEM_PLUS		/* NE */
#define KBD4_T0E		VK_BACK
#define KBD4_T0F		VK_TAB
#define KBD4_T10		VK_KEY_Q
#define KBD4_T11		VK_KEY_W
#define KBD4_T12		VK_KEY_E
#define KBD4_T13		VK_KEY_R
#define KBD4_T14		VK_KEY_T
#define KBD4_T15		VK_KEY_Y
#define KBD4_T16		VK_KEY_U
#define KBD4_T17		VK_KEY_I
#define KBD4_T18		VK_KEY_O
#define KBD4_T19		VK_KEY_P
#define KBD4_T1A		VK_OEM_4		/* NE */
#define KBD4_T1B		VK_OEM_6		/* NE */
#define KBD4_T1C		VK_RETURN
#define KBD4_T1D		VK_LCONTROL
#define KBD4_T1E		VK_KEY_A
#define KBD4_T1F		VK_KEY_S
#define KBD4_T20		VK_KEY_D
#define KBD4_T21		VK_KEY_F
#define KBD4_T22		VK_KEY_G
#define KBD4_T23		VK_KEY_H
#define KBD4_T24		VK_KEY_J
#define KBD4_T25		VK_KEY_K
#define KBD4_T26		VK_KEY_L
#define KBD4_T27		VK_OEM_1		/* NE */
#define KBD4_T28		VK_OEM_7		/* NE */
#define KBD4_T29		VK_OEM_3		/* NE */
#define KBD4_T2A		VK_LSHIFT
#define KBD4_T2B		VK_OEM_5
#define KBD4_T2C		VK_KEY_Z
#define KBD4_T2D		VK_KEY_X
#define KBD4_T2E		VK_KEY_C
#define KBD4_T2F		VK_KEY_V
#define KBD4_T30		VK_KEY_B
#define KBD4_T31		VK_KEY_N
#define KBD4_T32		VK_KEY_M
#define KBD4_T33		VK_OEM_COMMA
#define KBD4_T34		VK_OEM_PERIOD
#define KBD4_T35		VK_OEM_2
#define KBD4_T36		VK_RSHIFT
#define KBD4_T37		VK_MULTIPLY
#define KBD4_T38		VK_LMENU
#define KBD4_T39		VK_SPACE
#define KBD4_T3A		VK_CAPITAL
#define KBD4_T3B		VK_F1
#define KBD4_T3C		VK_F2
#define KBD4_T3D		VK_F3
#define KBD4_T3E		VK_F4
#define KBD4_T3F		VK_F5
#define KBD4_T40		VK_F6
#define KBD4_T41		VK_F7
#define KBD4_T42		VK_F8
#define KBD4_T43		VK_F9
#define KBD4_T44		VK_F10
#define KBD4_T45		VK_NUMLOCK
#define KBD4_T46		VK_SCROLL
#define KBD4_T47		VK_NUMPAD7		/* VK_HOME */
#define KBD4_T48		VK_NUMPAD8		/* VK_UP */
#define KBD4_T49		VK_NUMPAD9		/* VK_PRIOR */
#define KBD4_T4A		VK_SUBTRACT
#define KBD4_T4B		VK_NUMPAD4		/* VK_LEFT */
#define KBD4_T4C		VK_NUMPAD5		/* VK_CLEAR */
#define KBD4_T4D		VK_NUMPAD6		/* VK_RIGHT */
#define KBD4_T4E		VK_ADD
#define KBD4_T4F		VK_NUMPAD1		/* VK_END */
#define KBD4_T50		VK_NUMPAD2		/* VK_DOWN */
#define KBD4_T51		VK_NUMPAD3		/* VK_NEXT */
#define KBD4_T52		VK_NUMPAD0		/* VK_INSERT */
#define KBD4_T53		VK_DECIMAL		/* VK_DELETE */
#define KBD4_T54		VK_SNAPSHOT
#define KBD4_T55		VK_NONE
#define KBD4_T56		VK_OEM_102		/* NE */
#define KBD4_T57		VK_F11			/* NE */
#define KBD4_T58		VK_F12			/* NE */
#define KBD4_T59		VK_CLEAR
#define KBD4_T5A		VK_OEM_WSCTRL
#define KBD4_T5B		VK_OEM_FINISH
#define KBD4_T5C		VK_OEM_JUMP
#define KBD4_T5D		VK_EREOF
#define KBD4_T5E		VK_OEM_BACKTAB
#define KBD4_T5F		VK_OEM_AUTO
#define KBD4_T60		VK_NONE
#define KBD4_T61		VK_NONE
#define KBD4_T62		VK_ZOOM
#define KBD4_T63		VK_HELP
#define KBD4_T64		VK_F13
#define KBD4_T65		VK_F14
#define KBD4_T66		VK_F15
#define KBD4_T67		VK_F16
#define KBD4_T68		VK_F17
#define KBD4_T69		VK_F18
#define KBD4_T6A		VK_F19
#define KBD4_T6B		VK_F20
#define KBD4_T6C		VK_F21
#define KBD4_T6D		VK_F22
#define KBD4_T6E		VK_F23
#define KBD4_T6F		VK_OEM_PA3
#define KBD4_T70		VK_NONE
#define KBD4_T71		VK_OEM_RESET
#define KBD4_T72		VK_NONE
#define KBD4_T73		VK_ABNT_C1
#define KBD4_T74		VK_NONE
#define KBD4_T75		VK_NONE
#define KBD4_T76		VK_F24
#define KBD4_T77		VK_NONE
#define KBD4_T78		VK_NONE
#define KBD4_T79		VK_NONE
#define KBD4_T7A		VK_NONE
#define KBD4_T7B		VK_OEM_PA1
#define KBD4_T7C		VK_TAB
#define KBD4_T7D		VK_NONE
#define KBD4_T7E		VK_ABNT_C2
#define KBD4_T7F		VK_OEM_PA2

#define KBD4_X10		VK_MEDIA_PREV_TRACK
#define KBD4_X19		VK_MEDIA_NEXT_TRACK
#define KBD4_X1C		VK_RETURN
#define KBD4_X1D		VK_RCONTROL
#define KBD4_X20		VK_VOLUME_MUTE
#define KBD4_X21		VK_LAUNCH_APP2
#define KBD4_X22		VK_MEDIA_PLAY_PAUSE
#define KBD4_X24		VK_MEDIA_STOP
#define KBD4_X2E		VK_VOLUME_DOWN
#define KBD4_X30		VK_VOLUME_UP
#define KBD4_X32		VK_BROWSER_HOME
#define KBD4_X35		VK_DIVIDE
#define KBD4_X37		VK_SNAPSHOT
#define KBD4_X38		VK_RMENU
#define KBD4_X46		VK_PAUSE		/* VK_CANCEL */
#define KBD4_X47		VK_HOME
#define KBD4_X48		VK_UP
#define KBD4_X49		VK_PRIOR
#define KBD4_X4B		VK_LEFT
#define KBD4_X4D		VK_RIGHT
#define KBD4_X4F		VK_END
#define KBD4_X50		VK_DOWN
#define KBD4_X51		VK_NEXT			/* NE */
#define KBD4_X52		VK_INSERT
#define KBD4_X53		VK_DELETE
#define KBD4_X5B		VK_LWIN
#define KBD4_X5C		VK_RWIN
#define KBD4_X5D		VK_APPS
#define KBD4_X5E		VK_POWER
#define KBD4_X5F		VK_SLEEP
#define KBD4_X65		VK_BROWSER_SEARCH
#define KBD4_X66		VK_BROWSER_FAVORITES
#define KBD4_X67		VK_BROWSER_REFRESH
#define KBD4_X68		VK_BROWSER_STOP
#define KBD4_X69		VK_BROWSER_FORWARD
#define KBD4_X6A		VK_BROWSER_BACK
#define KBD4_X6B		VK_LAUNCH_APP1
#define KBD4_X6C		VK_LAUNCH_MAIL
#define KBD4_X6D		VK_LAUNCH_MEDIA_SELECT

#define KBD4_Y1D		VK_PAUSE

/**
 * Keyboard Type 7
 */

#define KBD7_T00		VK_NONE
#define KBD7_T01		VK_ESCAPE
#define KBD7_T02		VK_KEY_1
#define KBD7_T03		VK_KEY_2
#define KBD7_T04		VK_KEY_3
#define KBD7_T05		VK_KEY_4
#define KBD7_T06		VK_KEY_5
#define KBD7_T07		VK_KEY_6
#define KBD7_T08		VK_KEY_7
#define KBD7_T09		VK_KEY_8
#define KBD7_T0A		VK_KEY_9
#define KBD7_T0B		VK_KEY_0
#define KBD7_T0C		VK_OEM_MINUS
#define KBD7_T0D		VK_OEM_PLUS
#define KBD7_T0E		VK_BACK
#define KBD7_T0F		VK_TAB
#define KBD7_T10		VK_KEY_Q
#define KBD7_T11		VK_KEY_W
#define KBD7_T12		VK_KEY_E
#define KBD7_T13		VK_KEY_R
#define KBD7_T14		VK_KEY_T
#define KBD7_T15		VK_KEY_Y
#define KBD7_T16		VK_KEY_U
#define KBD7_T17		VK_KEY_I
#define KBD7_T18		VK_KEY_O
#define KBD7_T19		VK_KEY_P
#define KBD7_T1A		VK_OEM_4		/* NE */
#define KBD7_T1B		VK_OEM_6		/* NE */
#define KBD7_T1C		VK_RETURN
#define KBD7_T1D		VK_LCONTROL
#define KBD7_T1E		VK_KEY_A
#define KBD7_T1F		VK_KEY_S
#define KBD7_T20		VK_KEY_D
#define KBD7_T21		VK_KEY_F
#define KBD7_T22		VK_KEY_G
#define KBD7_T23		VK_KEY_H
#define KBD7_T24		VK_KEY_J
#define KBD7_T25		VK_KEY_K
#define KBD7_T26		VK_KEY_L
#define KBD7_T27		VK_OEM_1
#define KBD7_T28		VK_OEM_7
#define KBD7_T29		VK_OEM_3		/* NE */
#define KBD7_T2A		VK_LSHIFT
#define KBD7_T2B		VK_OEM_5		/* NE */
#define KBD7_T2C		VK_KEY_Z
#define KBD7_T2D		VK_KEY_X
#define KBD7_T2E		VK_KEY_C
#define KBD7_T2F		VK_KEY_V
#define KBD7_T30		VK_KEY_B
#define KBD7_T31		VK_KEY_N
#define KBD7_T32		VK_KEY_M
#define KBD7_T33		VK_OEM_COMMA
#define KBD7_T34		VK_OEM_PERIOD
#define KBD7_T35		VK_OEM_2
#define KBD7_T36		VK_RSHIFT
#define KBD7_T37		VK_MULTIPLY
#define KBD7_T38		VK_LMENU
#define KBD7_T39		VK_SPACE
#define KBD7_T3A		VK_CAPITAL
#define KBD7_T3B		VK_F1
#define KBD7_T3C		VK_F2
#define KBD7_T3D		VK_F3
#define KBD7_T3E		VK_F4
#define KBD7_T3F		VK_F5
#define KBD7_T40		VK_F6
#define KBD7_T41		VK_F7
#define KBD7_T42		VK_F8
#define KBD7_T43		VK_F9
#define KBD7_T44		VK_F10
#define KBD7_T45		VK_NUMLOCK
#define KBD7_T46		VK_SCROLL
#define KBD7_T47		VK_NUMPAD7		/* VK_HOME */
#define KBD7_T48		VK_NUMPAD8		/* VK_UP */
#define KBD7_T49		VK_NUMPAD9		/* VK_PRIOR */
#define KBD7_T4A		VK_SUBTRACT
#define KBD7_T4B		VK_NUMPAD4		/* VK_LEFT */
#define KBD7_T4C		VK_NUMPAD5		/* VK_CLEAR */
#define KBD7_T4D		VK_NUMPAD6		/* VK_RIGHT */
#define KBD7_T4E		VK_ADD
#define KBD7_T4F		VK_NUMPAD1		/* VK_END */
#define KBD7_T50		VK_NUMPAD2		/* VK_DOWN */
#define KBD7_T51		VK_NUMPAD3		/* VK_NEXT */
#define KBD7_T52		VK_NUMPAD0		/* VK_INSERT */
#define KBD7_T53		VK_DECIMAL		/* VK_DELETE */
#define KBD7_T54		VK_SNAPSHOT
#define KBD7_T55		VK_NONE
#define KBD7_T56		VK_OEM_102
#define KBD7_T57		VK_F11
#define KBD7_T58		VK_F12
#define KBD7_T59		VK_CLEAR
#define KBD7_T5A		VK_NONAME		/* NE */
#define KBD7_T5B		VK_NONAME		/* NE */
#define KBD7_T5C		VK_NONAME		/* NE */
#define KBD7_T5D		VK_EREOF
#define KBD7_T5E		VK_NONE			/* NE */
#define KBD7_T5F		VK_NONAME		/* NE */
#define KBD7_T60		VK_NONE
#define KBD7_T61		VK_NONE			/* NE */
#define KBD7_T62		VK_NONE			/* NE */
#define KBD7_T63		VK_NONE
#define KBD7_T64		VK_F13
#define KBD7_T65		VK_F14
#define KBD7_T66		VK_F15
#define KBD7_T67		VK_F16
#define KBD7_T68		VK_F17
#define KBD7_T69		VK_F18
#define KBD7_T6A		VK_F19
#define KBD7_T6B		VK_F20
#define KBD7_T6C		VK_F21
#define KBD7_T6D		VK_F22
#define KBD7_T6E		VK_F23
#define KBD7_T6F		VK_NONE			/* NE */
#define KBD7_T70		VK_HKTG			/* NE */
#define KBD7_T71		VK_NONE			/* NE */
#define KBD7_T72		VK_NONE
#define KBD7_T73		VK_ABNT_C1
#define KBD7_T74		VK_NONE
#define KBD7_T75		VK_NONE
#define KBD7_T76		VK_F24
#define KBD7_T77		VK_NONE
#define KBD7_T78		VK_NONE
#define KBD7_T79		VK_CONVERT		/* NE */
#define KBD7_T7A		VK_NONE
#define KBD7_T7B		VK_NONCONVERT		/* NE */
#define KBD7_T7C		VK_TAB
#define KBD7_T7D		VK_OEM_8
#define KBD7_T7E		VK_ABNT_C2
#define KBD7_T7F		VK_OEM_PA2

#define KBD7_X10		VK_MEDIA_PREV_TRACK
#define KBD7_X19		VK_MEDIA_NEXT_TRACK
#define KBD7_X1C		VK_RETURN
#define KBD7_X1D		VK_RCONTROL
#define KBD7_X20		VK_VOLUME_MUTE
#define KBD7_X21		VK_LAUNCH_APP2
#define KBD7_X22		VK_MEDIA_PLAY_PAUSE
#define KBD7_X24		VK_MEDIA_STOP
#define KBD7_X2E		VK_VOLUME_DOWN
#define KBD7_X30		VK_VOLUME_UP
#define KBD7_X32		VK_BROWSER_HOME
#define KBD7_X33		VK_NONE
#define KBD7_X35		VK_DIVIDE
#define KBD7_X37		VK_SNAPSHOT
#define KBD7_X38		VK_RMENU
#define KBD7_X42		VK_NONE
#define KBD7_X43		VK_NONE
#define KBD7_X44		VK_NONE
#define KBD7_X46		VK_CANCEL
#define KBD7_X47		VK_HOME
#define KBD7_X48		VK_UP
#define KBD7_X49		VK_PRIOR
#define KBD7_X4B		VK_LEFT
#define KBD7_X4D		VK_RIGHT
#define KBD7_X4F		VK_END
#define KBD7_X50		VK_DOWN
#define KBD7_X51		VK_NEXT
#define KBD7_X52		VK_INSERT
#define KBD7_X53		VK_DELETE
#define KBD7_X5B		VK_LWIN
#define KBD7_X5C		VK_RWIN
#define KBD7_X5D		VK_APPS
#define KBD7_X5E		VK_POWER
#define KBD7_X5F		VK_SLEEP
#define KBD7_X65		VK_BROWSER_SEARCH
#define KBD7_X66		VK_BROWSER_FAVORITES
#define KBD7_X67		VK_BROWSER_REFRESH
#define KBD7_X68		VK_BROWSER_STOP
#define KBD7_X69		VK_BROWSER_FORWARD
#define KBD7_X6A		VK_BROWSER_BACK
#define KBD7_X6B		VK_LAUNCH_APP1
#define KBD7_X6C		VK_LAUNCH_MAIL
#define KBD7_X6D		VK_LAUNCH_MEDIA_SELECT
#define KBD7_XF1		VK_NONE			/* NE */
#define KBD7_XF2		VK_NONE			/* NE */

#define KBD7_Y1D		VK_PAUSE

/**
 * X11 Keycodes
 */

/**
 * Mac OS X
 */

#define APPLE_VK_ANSI_A			0x00
#define APPLE_VK_ANSI_S			0x01
#define APPLE_VK_ANSI_D			0x02
#define APPLE_VK_ANSI_F			0x03
#define APPLE_VK_ANSI_H			0x04
#define APPLE_VK_ANSI_G			0x05
#define APPLE_VK_ANSI_Z			0x06
#define APPLE_VK_ANSI_X			0x07
#define APPLE_VK_ANSI_C			0x08
#define APPLE_VK_ANSI_V			0x09
#define APPLE_VK_ISO_Section		0x0A
#define APPLE_VK_ANSI_B			0x0B
#define APPLE_VK_ANSI_Q			0x0C
#define APPLE_VK_ANSI_W			0x0D
#define APPLE_VK_ANSI_E			0x0E
#define APPLE_VK_ANSI_R			0x0F
#define APPLE_VK_ANSI_Y			0x10
#define APPLE_VK_ANSI_T			0x11
#define APPLE_VK_ANSI_1			0x12
#define APPLE_VK_ANSI_2			0x13
#define APPLE_VK_ANSI_3			0x14
#define APPLE_VK_ANSI_4			0x15
#define APPLE_VK_ANSI_6			0x16
#define APPLE_VK_ANSI_5			0x17
#define APPLE_VK_ANSI_Equal		0x18
#define APPLE_VK_ANSI_9			0x19
#define APPLE_VK_ANSI_7			0x1A
#define APPLE_VK_ANSI_Minus		0x1B
#define APPLE_VK_ANSI_8			0x1C
#define APPLE_VK_ANSI_0			0x1D
#define APPLE_VK_ANSI_RightBracket	0x1E
#define APPLE_VK_ANSI_O			0x1F
#define APPLE_VK_ANSI_U			0x20
#define APPLE_VK_ANSI_LeftBracket	0x21
#define APPLE_VK_ANSI_I			0x22
#define APPLE_VK_ANSI_P			0x23
#define APPLE_VK_Return			0x24
#define APPLE_VK_ANSI_L			0x25
#define APPLE_VK_ANSI_J			0x26
#define APPLE_VK_ANSI_Quote		0x27
#define APPLE_VK_ANSI_K			0x28
#define APPLE_VK_ANSI_Semicolon		0x29
#define APPLE_VK_ANSI_Backslash		0x2A
#define APPLE_VK_ANSI_Comma		0x2B
#define APPLE_VK_ANSI_Slash		0x2C
#define APPLE_VK_ANSI_N			0x2D
#define APPLE_VK_ANSI_M			0x2E
#define APPLE_VK_ANSI_Period		0x2F
#define APPLE_VK_Tab			0x30
#define APPLE_VK_Space			0x31
#define APPLE_VK_ANSI_Grave		0x32
#define APPLE_VK_Delete			0x33
#define APPLE_VK_0x34			0x34
#define APPLE_VK_Escape			0x35
#define APPLE_VK_0x36			0x36
#define APPLE_VK_Command		0x37
#define APPLE_VK_Shift			0x38
#define APPLE_VK_CapsLock		0x39
#define APPLE_VK_Option			0x3A
#define APPLE_VK_Control		0x3B
#define APPLE_VK_RightShift		0x3C
#define APPLE_VK_RightOption		0x3D
#define APPLE_VK_RightControl		0x3E
#define APPLE_VK_Function		0x3F
#define APPLE_VK_F17			0x40
#define APPLE_VK_ANSI_KeypadDecimal	0x41
#define APPLE_VK_0x42			0x42
#define APPLE_VK_ANSI_KeypadMultiply	0x43
#define APPLE_VK_0x44			0x44
#define APPLE_VK_ANSI_KeypadPlus	0x45
#define APPLE_VK_0x46			0x46
#define APPLE_VK_ANSI_KeypadClear	0x47
#define APPLE_VK_VolumeUp		0x48
#define APPLE_VK_VolumeDown		0x49
#define APPLE_VK_Mute			0x4A
#define APPLE_VK_ANSI_KeypadDivide	0x4B
#define APPLE_VK_ANSI_KeypadEnter	0x4C
#define APPLE_VK_0x4D			0x4D
#define APPLE_VK_ANSI_KeypadMinus	0x4E
#define APPLE_VK_F18			0x4F
#define APPLE_VK_F19			0x50
#define APPLE_VK_ANSI_KeypadEquals	0x51
#define APPLE_VK_ANSI_Keypad0		0x52
#define APPLE_VK_ANSI_Keypad1		0x53
#define APPLE_VK_ANSI_Keypad2		0x54
#define APPLE_VK_ANSI_Keypad3		0x55
#define APPLE_VK_ANSI_Keypad4		0x56
#define APPLE_VK_ANSI_Keypad5		0x57
#define APPLE_VK_ANSI_Keypad6		0x58
#define APPLE_VK_ANSI_Keypad7		0x59
#define APPLE_VK_F20			0x5A
#define APPLE_VK_ANSI_Keypad8		0x5B
#define APPLE_VK_ANSI_Keypad9		0x5C
#define APPLE_VK_JIS_Yen		0x5D
#define APPLE_VK_JIS_Underscore		0x5E
#define APPLE_VK_JIS_KeypadComma	0x5F
#define APPLE_VK_F5			0x60
#define APPLE_VK_F6			0x61
#define APPLE_VK_F7			0x62
#define APPLE_VK_F3			0x63
#define APPLE_VK_F8			0x64
#define APPLE_VK_F9			0x65
#define APPLE_VK_JIS_Eisu		0x66
#define APPLE_VK_F11			0x67
#define APPLE_VK_JIS_Kana		0x68
#define APPLE_VK_F13			0x69
#define APPLE_VK_F16			0x6A
#define APPLE_VK_F14			0x6B
#define APPLE_VK_F10			0x6D
#define APPLE_VK_0x6C			0x6C
#define APPLE_VK_0x6E			0x6E
#define APPLE_VK_F12			0x6F
#define APPLE_VK_0x70			0x70
#define APPLE_VK_F15			0x71
#define APPLE_VK_Help			0x72
#define APPLE_VK_Home			0x73
#define APPLE_VK_PageUp			0x74
#define APPLE_VK_ForwardDelete		0x75
#define APPLE_VK_F4			0x76
#define APPLE_VK_End			0x77
#define APPLE_VK_F2			0x78
#define APPLE_VK_PageDown		0x79
#define APPLE_VK_F1			0x7A
#define APPLE_VK_LeftArrow		0x7B
#define APPLE_VK_RightArrow		0x7C
#define APPLE_VK_DownArrow		0x7D
#define APPLE_VK_UpArrow		0x7E

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Functions
 */

WINPR_API char* GetVirtualKeyName(DWORD vkcode);
WINPR_API DWORD GetVirtualKeyCodeFromName(const char* vkname);
WINPR_API DWORD GetVirtualKeyCodeFromXkbKeyName(const char* xkbname);

WINPR_API DWORD GetVirtualKeyCodeFromVirtualScanCode(DWORD scancode, DWORD dwKeyboardType);
WINPR_API DWORD GetVirtualScanCodeFromVirtualKeyCode(DWORD vkcode, DWORD dwKeyboardType);

#define KEYCODE_TYPE_APPLE		0x00000001
#define KEYCODE_TYPE_EVDEV		0x00000002

WINPR_API DWORD GetVirtualKeyCodeFromKeycode(DWORD keycode, DWORD dwFlags);
WINPR_API DWORD GetKeycodeFromVirtualKeyCode(DWORD keycode, DWORD dwFlags);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_INPUT_H */
