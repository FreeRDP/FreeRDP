/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Keyboard Mapping
 *
 * Copyright 2009-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __FREERDP_LOCALE_KEYBOARD_H
#define __FREERDP_LOCALE_KEYBOARD_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define RDP_KEYBOARD_LAYOUT_TYPE_STANDARD   1
#define RDP_KEYBOARD_LAYOUT_TYPE_VARIANT    2
#define RDP_KEYBOARD_LAYOUT_TYPE_IME        4

struct _RDP_KEYBOARD_LAYOUT
{
	uint32 code; /* Keyboard layout code */
	char* name; /* Keyboard layout name */
};
typedef struct _RDP_KEYBOARD_LAYOUT RDP_KEYBOARD_LAYOUT;

struct _RDP_SCANCODE
{
	uint32 code; /* Windows "scan code" */
	boolean extended; /* extended key flag */
};
typedef struct _RDP_SCANCODE RDP_SCANCODE;

struct _VIRTUAL_KEY_CODE
{
	uint32 code; /* Windows Virtual Key Code */
	const char* name; /* Virtual Key Code Name */
};
typedef struct _VIRTUAL_KEY_CODE VIRTUAL_KEY_CODE;

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
#define VK_MEDIA_SELECT		0xB5 /* Windows 2000/XP: Select Media key */
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

/* Keyboard layout IDs */

#define KBD_ARABIC_101				0x00000401
#define KBD_BULGARIAN				0x00000402
#define KBD_CHINESE_TRADITIONAL_US		0x00000404
#define KBD_CZECH				0x00000405
#define KBD_DANISH				0x00000406
#define KBD_GERMAN				0x00000407
#define KBD_GREEK				0x00000408
#define KBD_US					0x00000409
#define KBD_SPANISH				0x0000040A
#define KBD_FINNISH				0x0000040B
#define KBD_FRENCH				0x0000040C
#define KBD_HEBREW				0x0000040D
#define KBD_HUNGARIAN				0x0000040E
#define KBD_ICELANDIC				0x0000040F
#define KBD_ITALIAN				0x00000410
#define KBD_JAPANESE				0x00000411
#define KBD_KOREAN				0x00000412
#define KBD_DUTCH				0x00000413
#define KBD_NORWEGIAN				0x00000414
#define KBD_POLISH_PROGRAMMERS			0x00000415
#define KBD_PORTUGUESE_BRAZILIAN_ABNT		0x00000416
#define KBD_ROMANIAN				0x00000418
#define KBD_RUSSIAN				0x00000419
#define KBD_CROATIAN				0x0000041A
#define KBD_SLOVAK				0x0000041B
#define KBD_ALBANIAN				0x0000041C
#define KBD_SWEDISH				0x0000041D
#define KBD_THAI_KEDMANEE			0x0000041E
#define KBD_TURKISH_Q				0x0000041F
#define KBD_URDU				0x00000420
#define KBD_UKRAINIAN				0x00000422
#define KBD_BELARUSIAN				0x00000423
#define KBD_SLOVENIAN				0x00000424
#define KBD_ESTONIAN				0x00000425
#define KBD_LATVIAN				0x00000426
#define KBD_LITHUANIAN_IBM			0x00000427
#define KBD_FARSI				0x00000429
#define KBD_VIETNAMESE				0x0000042A
#define KBD_ARMENIAN_EASTERN			0x0000042B
#define KBD_AZERI_LATIN				0x0000042C
#define KBD_FYRO_MACEDONIAN			0x0000042F
#define KBD_GEORGIAN				0x00000437
#define KBD_FAEROESE				0x00000438
#define KBD_DEVANAGARI_INSCRIPT			0x00000439
#define KBD_MALTESE_47_KEY			0x0000043A
#define KBD_NORWEGIAN_WITH_SAMI			0x0000043B
#define KBD_KAZAKH				0x0000043F
#define KBD_KYRGYZ_CYRILLIC			0x00000440
#define KBD_TATAR				0x00000444
#define KBD_BENGALI				0x00000445
#define KBD_PUNJABI				0x00000446
#define KBD_GUJARATI				0x00000447
#define KBD_TAMIL				0x00000449
#define KBD_TELUGU				0x0000044A
#define KBD_KANNADA				0x0000044B
#define KBD_MALAYALAM				0x0000044C
#define KBD_MARATHI				0x0000044E
#define KBD_MONGOLIAN_CYRILLIC			0x00000450
#define KBD_UNITED_KINGDOM_EXTENDED		0x00000452
#define KBD_SYRIAC				0x0000045A
#define KBD_NEPALI				0x00000461
#define KBD_PASHTO				0x00000463
#define KBD_DIVEHI_PHONETIC			0x00000465
#define KBD_LUXEMBOURGISH			0x0000046E
#define KBD_MAORI				0x00000481
#define KBD_CHINESE_SIMPLIFIED_US		0x00000804
#define KBD_SWISS_GERMAN			0x00000807
#define KBD_UNITED_KINGDOM			0x00000809
#define KBD_LATIN_AMERICAN			0x0000080A
#define KBD_BELGIAN_FRENCH			0x0000080C
#define KBD_BELGIAN_PERIOD			0x00000813
#define KBD_PORTUGUESE				0x00000816
#define KBD_SERBIAN_LATIN			0x0000081A
#define KBD_AZERI_CYRILLIC			0x0000082C
#define KBD_SWEDISH_WITH_SAMI			0x0000083B
#define KBD_UZBEK_CYRILLIC			0x00000843
#define KBD_INUKTITUT_LATIN			0x0000085D
#define KBD_CANADIAN_FRENCH_LEGACY		0x00000C0C
#define KBD_SERBIAN_CYRILLIC			0x00000C1A
#define KBD_CANADIAN_FRENCH			0x00001009
#define KBD_SWISS_FRENCH			0x0000100C
#define KBD_BOSNIAN				0x0000141A
#define KBD_IRISH				0x00001809
#define KBD_BOSNIAN_CYRILLIC			0x0000201A

/* Keyboard layout variant IDs */

#define KBD_ARABIC_102					0x00010401
#define KBD_BULGARIAN_LATIN				0x00010402
#define KBD_CZECH_QWERTY				0x00010405
#define KBD_GERMAN_IBM					0x00010407
#define KBD_GREEK_220					0x00010408
#define KBD_UNITED_STATES_DVORAK			0x00010409
#define KBD_SPANISH_VARIATION				0x0001040A
#define KBD_HUNGARIAN_101_KEY				0x0001040E
#define KBD_ITALIAN_142					0x00010410
#define KBD_POLISH_214					0x00010415
#define KBD_PORTUGUESE_BRAZILIAN_ABNT2			0x00010416
#define KBD_RUSSIAN_TYPEWRITER				0x00010419
#define KBD_SLOVAK_QWERTY				0x0001041B
#define KBD_THAI_PATTACHOTE				0x0001041E
#define KBD_TURKISH_F					0x0001041F
#define KBD_LATVIAN_QWERTY				0x00010426
#define KBD_LITHUANIAN					0x00010427
#define KBD_ARMENIAN_WESTERN				0x0001042B
#define KBD_HINDI_TRADITIONAL				0x00010439
#define KBD_MALTESE_48_KEY				0x0001043A
#define KBD_SAMI_EXTENDED_NORWAY			0x0001043B
#define KBD_BENGALI_INSCRIPT				0x00010445
#define KBD_SYRIAC_PHONETIC				0x0001045A
#define KBD_DIVEHI_TYPEWRITER				0x00010465
#define KBD_BELGIAN_COMMA				0x0001080C
#define KBD_FINNISH_WITH_SAMI				0x0001083B
#define KBD_CANADIAN_MULTILINGUAL_STANDARD		0x00011009
#define KBD_GAELIC					0x00011809
#define KBD_ARABIC_102_AZERTY				0x00020401
#define KBD_CZECH_PROGRAMMERS				0x00020405
#define KBD_GREEK_319					0x00020408
#define KBD_UNITED_STATES_INTERNATIONAL			0x00020409
#define KBD_THAI_KEDMANEE_NON_SHIFTLOCK			0x0002041E
#define KBD_SAMI_EXTENDED_FINLAND_SWEDEN		0x0002083B
#define KBD_GREEK_220_LATIN				0x00030408
#define KBD_UNITED_STATES_DVORAK_FOR_LEFT_HAND		0x00030409
#define KBD_THAI_PATTACHOTE_NON_SHIFTLOCK		0x0003041E
#define KBD_GREEK_319_LATIN				0x00040408
#define KBD_UNITED_STATES_DVORAK_FOR_RIGHT_HAND		0x00040409
#define KBD_GREEK_LATIN					0x00050408
#define KBD_US_ENGLISH_TABLE_FOR_IBM_ARABIC_238_L	0x00050409
#define KBD_GREEK_POLYTONIC				0x00060408
#define KBD_GERMAN_NEO					0xB0000407

/* Global Input Method Editor (IME) IDs */

#define KBD_CHINESE_TRADITIONAL_PHONETIC			0xE0010404
#define KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002			0xE0010411
#define KBD_KOREAN_INPUT_SYSTEM_IME_2000			0xE0010412
#define KBD_CHINESE_SIMPLIFIED_QUANPIN				0xE0010804
#define KBD_CHINESE_TRADITIONAL_CHANGJIE			0xE0020404
#define KBD_CHINESE_SIMPLIFIED_SHUANGPIN			0xE0020804
#define KBD_CHINESE_TRADITIONAL_QUICK				0xE0030404
#define KBD_CHINESE_SIMPLIFIED_ZHENGMA				0xE0030804
#define KBD_CHINESE_TRADITIONAL_BIG5_CODE			0xE0040404
#define KBD_CHINESE_TRADITIONAL_ARRAY				0xE0050404
#define KBD_CHINESE_SIMPLIFIED_NEIMA				0xE0050804
#define KBD_CHINESE_TRADITIONAL_DAYI				0xE0060404
#define KBD_CHINESE_TRADITIONAL_UNICODE				0xE0070404
#define KBD_CHINESE_TRADITIONAL_NEW_PHONETIC			0xE0080404
#define KBD_CHINESE_TRADITIONAL_NEW_CHANGJIE			0xE0090404
#define KBD_CHINESE_TRADITIONAL_MICROSOFT_PINYIN_IME_3		0xE00E0804
#define KBD_CHINESE_TRADITIONAL_ALPHANUMERIC			0xE00F0404

FREERDP_API uint32 freerdp_keyboard_init(uint32 keyboardLayoutId);
FREERDP_API RDP_KEYBOARD_LAYOUT* freerdp_keyboard_get_layouts(uint32 types);
FREERDP_API const char* freerdp_keyboard_get_layout_name_from_id(uint32 keyboardLayoutId);
FREERDP_API uint32 freerdp_keyboard_get_rdp_scancode_from_x11_keycode(uint32 keycode, boolean* extended);
FREERDP_API uint32 freerdp_keyboard_get_x11_keycode_from_rdp_scancode(uint32 scancode, boolean extended);
FREERDP_API uint32 freerdp_keyboard_get_rdp_scancode_from_virtual_key_code(uint32 vkcode, boolean* extended);
FREERDP_API char* freerdp_keyboard_get_virtual_key_code_name(uint32 vkcode);

#endif /* __FREERDP_LOCALE_KEYBOARD_H */
