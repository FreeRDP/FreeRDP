/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Keyboard Layouts
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "liblocale.h"

#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/locale/keyboard.h>

/*
 * In Windows XP, this information is available in the system registry at
 * HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet001/Control/Keyboard Layouts/
 */

static const RDP_KEYBOARD_LAYOUT RDP_KEYBOARD_LAYOUT_TABLE[] =
{
	{ KBD_ARABIC_101,		"Arabic (101)" },
	{ KBD_BULGARIAN,		"Bulgarian" },
	{ KBD_CHINESE_TRADITIONAL_US,	"Chinese (Traditional) - US Keyboard" },
	{ KBD_CZECH,			"Czech" },
	{ KBD_DANISH,			"Danish" },
	{ KBD_GERMAN,			"German" },
	{ KBD_GREEK,			"Greek" },
	{ KBD_US,			"US" },
	{ KBD_SPANISH,			"Spanish" },
	{ KBD_FINNISH,			"Finnish" },
	{ KBD_FRENCH,			"French" },
	{ KBD_HEBREW,			"Hebrew" },
	{ KBD_HUNGARIAN,		"Hungarian" },
	{ KBD_ICELANDIC,		"Icelandic" },
	{ KBD_ITALIAN,			"Italian" },
	{ KBD_JAPANESE,			"Japanese" },
	{ KBD_KOREAN,			"Korean" },
	{ KBD_DUTCH,			"Dutch" },
	{ KBD_NORWEGIAN,		"Norwegian" },
	{ KBD_POLISH_PROGRAMMERS,	"Polish (Programmers)" },
	{ KBD_PORTUGUESE_BRAZILIAN_ABNT,"Portuguese (Brazilian ABNT)" },
	{ KBD_ROMANIAN,			"Romanian" },
	{ KBD_RUSSIAN,			"Russian" },
	{ KBD_CROATIAN,			"Croatian" },
	{ KBD_SLOVAK,			"Slovak" },
	{ KBD_ALBANIAN,			"Albanian" },
	{ KBD_SWEDISH,			"Swedish" },
	{ KBD_THAI_KEDMANEE,		"Thai Kedmanee" },
	{ KBD_TURKISH_Q,		"Turkish Q" },
	{ KBD_URDU,			"Urdu" },
	{ KBD_UKRAINIAN,		"Ukrainian" },
	{ KBD_BELARUSIAN,		"Belarusian" },
	{ KBD_SLOVENIAN,		"Slovenian" },
	{ KBD_ESTONIAN,			"Estonian" },
	{ KBD_LATVIAN,			"Latvian" },
	{ KBD_LITHUANIAN_IBM,		"Lithuanian IBM" },
	{ KBD_FARSI,			"Farsi" },
	{ KBD_VIETNAMESE,		"Vietnamese" },
	{ KBD_ARMENIAN_EASTERN,		"Armenian Eastern" },
	{ KBD_AZERI_LATIN,		"Azeri Latin" },
	{ KBD_FYRO_MACEDONIAN,		"FYRO Macedonian" },
	{ KBD_GEORGIAN,			"Georgian" },
	{ KBD_FAEROESE,			"Faeroese" },
	{ KBD_DEVANAGARI_INSCRIPT,	"Devanagari - INSCRIPT" },
	{ KBD_MALTESE_47_KEY,		"Maltese 47-key" },
	{ KBD_NORWEGIAN_WITH_SAMI,	"Norwegian with Sami" },
	{ KBD_KAZAKH,			"Kazakh" },
	{ KBD_KYRGYZ_CYRILLIC,		"Kyrgyz Cyrillic" },
	{ KBD_TATAR,			"Tatar" },
	{ KBD_BENGALI,			"Bengali" },
	{ KBD_PUNJABI,			"Punjabi" },
	{ KBD_GUJARATI,			"Gujarati" },
	{ KBD_TAMIL,			"Tamil" },
	{ KBD_TELUGU,			"Telugu" },
	{ KBD_KANNADA,			"Kannada" },
	{ KBD_MALAYALAM,		"Malayalam" },
	{ KBD_MARATHI,			"Marathi" },
	{ KBD_MONGOLIAN_CYRILLIC,	"Mongolian Cyrillic" },
	{ KBD_UNITED_KINGDOM_EXTENDED,	"United Kingdom Extended" },
	{ KBD_SYRIAC,			"Syriac" },
	{ KBD_NEPALI,			"Nepali" },
	{ KBD_PASHTO,			"Pashto" },
	{ KBD_DIVEHI_PHONETIC,		"Divehi Phonetic" },
	{ KBD_LUXEMBOURGISH,		"Luxembourgish" },
	{ KBD_MAORI,			"Maori" },
	{ KBD_CHINESE_SIMPLIFIED_US,	"Chinese (Simplified) - US Keyboard" },
	{ KBD_SWISS_GERMAN,		"Swiss German" },
	{ KBD_UNITED_KINGDOM,		"United Kingdom" },
	{ KBD_LATIN_AMERICAN,		"Latin American" },
	{ KBD_BELGIAN_FRENCH,		"Belgian French" },
	{ KBD_BELGIAN_PERIOD,		"Belgian (Period)" },
	{ KBD_PORTUGUESE,		"Portuguese" },
	{ KBD_SERBIAN_LATIN,		"Serbian (Latin)" },
	{ KBD_AZERI_CYRILLIC,		"Azeri Cyrillic" },
	{ KBD_SWEDISH_WITH_SAMI,	"Swedish with Sami" },
	{ KBD_UZBEK_CYRILLIC,		"Uzbek Cyrillic" },
	{ KBD_INUKTITUT_LATIN,		"Inuktitut Latin" },
	{ KBD_CANADIAN_FRENCH_LEGACY,	"Canadian French (legacy)" },
	{ KBD_SERBIAN_CYRILLIC,		"Serbian (Cyrillic)" },
	{ KBD_CANADIAN_FRENCH,		"Canadian French" },
	{ KBD_SWISS_FRENCH,		"Swiss French" },
	{ KBD_BOSNIAN,			"Bosnian" },
	{ KBD_IRISH,			"Irish" },
	{ KBD_BOSNIAN_CYRILLIC,		"Bosnian Cyrillic" }
};

struct _RDP_KEYBOARD_LAYOUT_VARIANT
{
	uint32 code; /* Keyboard layout code */
	uint32 id; /* Keyboard variant ID */
	const char* name; /* Keyboard layout variant name */
};
typedef struct _RDP_KEYBOARD_LAYOUT_VARIANT RDP_KEYBOARD_LAYOUT_VARIANT;

static const RDP_KEYBOARD_LAYOUT_VARIANT RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[] =
{
	{ KBD_ARABIC_102,				0x0028, "Arabic (102)" },
	{ KBD_BULGARIAN_LATIN,				0x0004, "Bulgarian (Latin)" },
	{ KBD_CZECH_QWERTY,				0x0005, "Czech (QWERTY)" },
	{ KBD_GERMAN_IBM,				0x0012, "German (IBM)" },
	{ KBD_GREEK_220,				0x0016, "Greek (220)" },
	{ KBD_UNITED_STATES_DVORAK,			0x0002, "United States-Dvorak" },
	{ KBD_SPANISH_VARIATION,			0x0086, "Spanish Variation" },
	{ KBD_HUNGARIAN_101_KEY,			0x0006, "Hungarian 101-key" },
	{ KBD_ITALIAN_142,				0x0003, "Italian (142)" },
	{ KBD_POLISH_214,				0x0007, "Polish (214)" },
	{ KBD_PORTUGUESE_BRAZILIAN_ABNT2,		0x001D, "Portuguese (Brazilian ABNT2)" },
	{ KBD_RUSSIAN_TYPEWRITER,			0x0008, "Russian (Typewriter)" },
	{ KBD_SLOVAK_QWERTY,				0x0013, "Slovak (QWERTY)" },
	{ KBD_THAI_PATTACHOTE,				0x0021, "Thai Pattachote" },
	{ KBD_TURKISH_F,				0x0014, "Turkish F" },
	{ KBD_LATVIAN_QWERTY,				0x0015, "Latvian (QWERTY)" },
	{ KBD_LITHUANIAN,				0x0027, "Lithuanian" },
	{ KBD_ARMENIAN_WESTERN,				0x0025, "Armenian Western" },
	{ KBD_HINDI_TRADITIONAL,			0x000C, "Hindi Traditional" },
	{ KBD_MALTESE_48_KEY,				0x002B, "Maltese 48-key" },
	{ KBD_SAMI_EXTENDED_NORWAY,			0x002C, "Sami Extended Norway" },
	{ KBD_BENGALI_INSCRIPT,				0x002A, "Bengali (Inscript)" },
	{ KBD_SYRIAC_PHONETIC,				0x000E, "Syriac Phonetic" },
	{ KBD_DIVEHI_TYPEWRITER,			0x000D, "Divehi Typewriter" },
	{ KBD_BELGIAN_COMMA,				0x001E, "Belgian (Comma)" },
	{ KBD_FINNISH_WITH_SAMI,			0x002D, "Finnish with Sami" },
	{ KBD_CANADIAN_MULTILINGUAL_STANDARD,		0x0020, "Canadian Multilingual Standard" },
	{ KBD_GAELIC,					0x0026, "Gaelic" },
	{ KBD_ARABIC_102_AZERTY,			0x0029, "Arabic (102) AZERTY" },
	{ KBD_CZECH_PROGRAMMERS,			0x000A, "Czech Programmers" },
	{ KBD_GREEK_319,				0x0018, "Greek (319)" },
	{ KBD_UNITED_STATES_INTERNATIONAL,		0x0001, "United States-International" },
	{ KBD_THAI_KEDMANEE_NON_SHIFTLOCK,		0x0022, "Thai Kedmanee (non-ShiftLock)" },
	{ KBD_SAMI_EXTENDED_FINLAND_SWEDEN,		0x002E, "Sami Extended Finland-Sweden" },
	{ KBD_GREEK_220_LATIN,				0x0017, "Greek (220) Latin" },
	{ KBD_UNITED_STATES_DVORAK_FOR_LEFT_HAND,	0x001A, "United States-Dvorak for left hand" },
	{ KBD_THAI_PATTACHOTE_NON_SHIFTLOCK,		0x0023, "Thai Pattachote (non-ShiftLock)" },
	{ KBD_GREEK_319_LATIN,				0x0011, "Greek (319) Latin" },
	{ KBD_UNITED_STATES_DVORAK_FOR_RIGHT_HAND,	0x001B, "United States-Dvorak for right hand" },
	{ KBD_GREEK_LATIN,				0x0019, "Greek Latin" },
	{ KBD_US_ENGLISH_TABLE_FOR_IBM_ARABIC_238_L,	0x000B, "US English Table for IBM Arabic 238_L" },
	{ KBD_GREEK_POLYTONIC,				0x001F, "Greek Polytonic" },
	{ KBD_GERMAN_NEO,				0x00C0, "German Neo" }
};

/* Input Method Editor (IME) */

struct _RDP_KEYBOARD_IME
{
	uint32 code; /* Keyboard layout code */
	const char* file; /* IME file */
	const char* name; /* Keyboard layout name */
};
typedef struct _RDP_KEYBOARD_IME RDP_KEYBOARD_IME;

/* Global Input Method Editors (IME) */

static const RDP_KEYBOARD_IME RDP_KEYBOARD_IME_TABLE[] =
{
	{ KBD_CHINESE_TRADITIONAL_PHONETIC,			"phon.ime", "Chinese (Traditional) - Phonetic" },
	{ KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002,			"imjp81.ime", "Japanese Input System (MS-IME2002)" },
	{ KBD_KOREAN_INPUT_SYSTEM_IME_2000,			"imekr61.ime", "Korean Input System (IME 2000)" },
	{ KBD_CHINESE_SIMPLIFIED_QUANPIN,			"winpy.ime", "Chinese (Simplified) - QuanPin" },
	{ KBD_CHINESE_TRADITIONAL_CHANGJIE,			"chajei.ime", "Chinese (Traditional) - ChangJie" },
	{ KBD_CHINESE_SIMPLIFIED_SHUANGPIN,			"winsp.ime", "Chinese (Simplified) - ShuangPin" },
	{ KBD_CHINESE_TRADITIONAL_QUICK,			"quick.ime", "Chinese (Traditional) - Quick" },
	{ KBD_CHINESE_SIMPLIFIED_ZHENGMA,			"winzm.ime", "Chinese (Simplified) - ZhengMa" },
	{ KBD_CHINESE_TRADITIONAL_BIG5_CODE,			"winime.ime", "Chinese (Traditional) - Big5 Code" },
	{ KBD_CHINESE_TRADITIONAL_ARRAY,			"winar30.ime", "Chinese (Traditional) - Array" },
	{ KBD_CHINESE_SIMPLIFIED_NEIMA,				"wingb.ime", "Chinese (Simplified) - NeiMa" },
	{ KBD_CHINESE_TRADITIONAL_DAYI,				"dayi.ime", "Chinese (Traditional) - DaYi" },
	{ KBD_CHINESE_TRADITIONAL_UNICODE,			"unicdime.ime", "Chinese (Traditional) - Unicode" },
	{ KBD_CHINESE_TRADITIONAL_NEW_PHONETIC,			"TINTLGNT.IME", "Chinese (Traditional) - New Phonetic" },
	{ KBD_CHINESE_TRADITIONAL_NEW_CHANGJIE,			"CINTLGNT.IME", "Chinese (Traditional) - New ChangJie" },
	{ KBD_CHINESE_TRADITIONAL_MICROSOFT_PINYIN_IME_3,	"pintlgnt.ime", "Chinese (Traditional) - Microsoft Pinyin IME 3.0" },
	{ KBD_CHINESE_TRADITIONAL_ALPHANUMERIC,			"romanime.ime", "Chinese (Traditional) - Alphanumeric" }
};

const VIRTUAL_KEY_CODE VIRTUAL_KEY_CODE_TABLE[256] =
{
	{ 0, "" },
	{ VK_LBUTTON, "VK_LBUTTON" },
	{ VK_RBUTTON, "VK_RBUTTON" },
	{ VK_CANCEL, "VK_CANCEL" },
	{ VK_MBUTTON, "VK_MBUTTON" },
	{ VK_XBUTTON1, "VK_XBUTTON1" },
	{ VK_XBUTTON2, "VK_XBUTTON2" },
	{ 0, "" },
	{ VK_BACK, "VK_BACK" },
	{ VK_TAB, "VK_TAB" },
	{ 0, "" },
	{ 0, "" },
	{ VK_CLEAR, "VK_CLEAR" },
	{ VK_RETURN, "VK_RETURN" },
	{ 0, "" },
	{ 0, "" },
	{ VK_SHIFT, "VK_SHIFT" },
	{ VK_CONTROL, "VK_CONTROL" },
	{ VK_MENU, "VK_MENU" },
	{ VK_PAUSE, "VK_PAUSE" },
	{ VK_CAPITAL, "VK_CAPITAL" },
	{ VK_KANA, "VK_KANA" }, /* also VK_HANGUL */
	{ 0, "" },
	{ VK_JUNJA, "VK_JUNJA" },
	{ VK_FINAL, "VK_FINAL" },
	{ VK_KANJI, "VK_KANJI" }, /* also VK_HANJA */
	{ 0, "" },
	{ VK_ESCAPE, "VK_ESCAPE" },
	{ VK_CONVERT, "VK_CONVERT" },
	{ VK_NONCONVERT, "VK_NONCONVERT" },
	{ VK_ACCEPT, "VK_ACCEPT" },
	{ VK_MODECHANGE, "VK_MODECHANGE" },
	{ VK_SPACE, "VK_SPACE" },
	{ VK_PRIOR, "VK_PRIOR" },
	{ VK_NEXT, "VK_NEXT" },
	{ VK_END, "VK_END" },
	{ VK_HOME, "VK_HOME" },
	{ VK_LEFT, "VK_LEFT" },
	{ VK_UP, "VK_UP" },
	{ VK_RIGHT, "VK_RIGHT" },
	{ VK_DOWN, "VK_DOWN" },
	{ VK_SELECT, "VK_SELECT" },
	{ VK_PRINT, "VK_PRINT" },
	{ VK_EXECUTE, "VK_EXECUTE" },
	{ VK_SNAPSHOT, "VK_SNAPSHOT" },
	{ VK_INSERT, "VK_INSERT" },
	{ VK_DELETE, "VK_DELETE" },
	{ VK_HELP, "VK_HELP" },
	{ VK_KEY_0, "VK_KEY_0" },
	{ VK_KEY_1, "VK_KEY_1" },
	{ VK_KEY_2, "VK_KEY_2" },
	{ VK_KEY_3, "VK_KEY_3" },
	{ VK_KEY_4, "VK_KEY_4" },
	{ VK_KEY_5, "VK_KEY_5" },
	{ VK_KEY_6, "VK_KEY_6" },
	{ VK_KEY_7, "VK_KEY_7" },
	{ VK_KEY_8, "VK_KEY_8" },
	{ VK_KEY_9, "VK_KEY_9" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_KEY_A, "VK_KEY_A" },
	{ VK_KEY_B, "VK_KEY_B" },
	{ VK_KEY_C, "VK_KEY_C" },
	{ VK_KEY_D, "VK_KEY_D" },
	{ VK_KEY_E, "VK_KEY_E" },
	{ VK_KEY_F, "VK_KEY_F" },
	{ VK_KEY_G, "VK_KEY_G" },
	{ VK_KEY_H, "VK_KEY_H" },
	{ VK_KEY_I, "VK_KEY_I" },
	{ VK_KEY_J, "VK_KEY_J" },
	{ VK_KEY_K, "VK_KEY_K" },
	{ VK_KEY_L, "VK_KEY_L" },
	{ VK_KEY_M, "VK_KEY_M" },
	{ VK_KEY_N, "VK_KEY_N" },
	{ VK_KEY_O, "VK_KEY_O" },
	{ VK_KEY_P, "VK_KEY_P" },
	{ VK_KEY_Q, "VK_KEY_Q" },
	{ VK_KEY_R, "VK_KEY_R" },
	{ VK_KEY_S, "VK_KEY_S" },
	{ VK_KEY_T, "VK_KEY_T" },
	{ VK_KEY_U, "VK_KEY_U" },
	{ VK_KEY_V, "VK_KEY_V" },
	{ VK_KEY_W, "VK_KEY_W" },
	{ VK_KEY_X, "VK_KEY_X" },
	{ VK_KEY_Y, "VK_KEY_Y" },
	{ VK_KEY_Z, "VK_KEY_Z" },
	{ VK_LWIN, "VK_LWIN" },
	{ VK_RWIN, "VK_RWIN" },
	{ VK_APPS, "VK_APPS" },
	{ 0, "" },
	{ VK_SLEEP, "VK_SLEEP" },
	{ VK_NUMPAD0, "VK_NUMPAD0" },
	{ VK_NUMPAD1, "VK_NUMPAD1" },
	{ VK_NUMPAD2, "VK_NUMPAD2" },
	{ VK_NUMPAD3, "VK_NUMPAD3" },
	{ VK_NUMPAD4, "VK_NUMPAD4" },
	{ VK_NUMPAD5, "VK_NUMPAD5" },
	{ VK_NUMPAD6, "VK_NUMPAD6" },
	{ VK_NUMPAD7, "VK_NUMPAD7" },
	{ VK_NUMPAD8, "VK_NUMPAD8" },
	{ VK_NUMPAD9, "VK_NUMPAD9" },
	{ VK_MULTIPLY, "VK_MULTIPLY" },
	{ VK_ADD, "VK_ADD" },
	{ VK_SEPARATOR, "VK_SEPARATOR" },
	{ VK_SUBTRACT, "VK_SUBTRACT" },
	{ VK_DECIMAL, "VK_DECIMAL" },
	{ VK_DIVIDE, "VK_DIVIDE" },
	{ VK_F1, "VK_F1" },
	{ VK_F2, "VK_F2" },
	{ VK_F3, "VK_F3" },
	{ VK_F4, "VK_F4" },
	{ VK_F5, "VK_F5" },
	{ VK_F6, "VK_F6" },
	{ VK_F7, "VK_F7" },
	{ VK_F8, "VK_F8" },
	{ VK_F9, "VK_F9" },
	{ VK_F10, "VK_F10" },
	{ VK_F11, "VK_F11" },
	{ VK_F12, "VK_F12" },
	{ VK_F13, "VK_F13" },
	{ VK_F14, "VK_F14" },
	{ VK_F15, "VK_F15" },
	{ VK_F16, "VK_F16" },
	{ VK_F17, "VK_F17" },
	{ VK_F18, "VK_F18" },
	{ VK_F19, "VK_F19" },
	{ VK_F20, "VK_F20" },
	{ VK_F21, "VK_F21" },
	{ VK_F22, "VK_F22" },
	{ VK_F23, "VK_F23" },
	{ VK_F24, "VK_F24" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_NUMLOCK, "VK_NUMLOCK" },
	{ VK_SCROLL, "VK_SCROLL" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_LSHIFT, "VK_LSHIFT" },
	{ VK_RSHIFT, "VK_RSHIFT" },
	{ VK_LCONTROL, "VK_LCONTROL" },
	{ VK_RCONTROL, "VK_RCONTROL" },
	{ VK_LMENU, "VK_LMENU" },
	{ VK_RMENU, "VK_RMENU" },
	{ VK_BROWSER_BACK, "VK_BROWSER_BACK" },
	{ VK_BROWSER_FORWARD, "VK_BROWSER_FORWARD" },
	{ VK_BROWSER_REFRESH, "VK_BROWSER_REFRESH" },
	{ VK_BROWSER_STOP, "VK_BROWSER_STOP" },
	{ VK_BROWSER_SEARCH, "VK_BROWSER_SEARCH" },
	{ VK_BROWSER_FAVORITES, "VK_BROWSER_FAVORITES" },
	{ VK_BROWSER_HOME, "VK_BROWSER_HOME" },
	{ VK_VOLUME_MUTE, "VK_VOLUME_MUTE" },
	{ VK_VOLUME_DOWN, "VK_VOLUME_DOWN" },
	{ VK_VOLUME_UP, "VK_VOLUME_UP" },
	{ VK_MEDIA_NEXT_TRACK, "VK_MEDIA_NEXT_TRACK" },
	{ VK_MEDIA_PREV_TRACK, "VK_MEDIA_PREV_TRACK" },
	{ VK_MEDIA_STOP, "VK_MEDIA_STOP" },
	{ VK_MEDIA_PLAY_PAUSE, "VK_MEDIA_PLAY_PAUSE" },
	{ VK_LAUNCH_MAIL, "VK_LAUNCH_MAIL" },
	{ VK_MEDIA_SELECT, "VK_MEDIA_SELECT" },
	{ VK_LAUNCH_APP1, "VK_LAUNCH_APP1" },
	{ VK_LAUNCH_APP2, "VK_LAUNCH_APP2" },
	{ 0, "" },
	{ 0, "" },
	{ VK_OEM_1, "VK_OEM_1" },
	{ VK_OEM_PLUS, "VK_OEM_PLUS" },
	{ VK_OEM_COMMA, "VK_OEM_COMMA" },
	{ VK_OEM_MINUS, "VK_OEM_MINUS" },
	{ VK_OEM_PERIOD, "VK_OEM_PERIOD" },
	{ VK_OEM_2, "VK_OEM_2" },
	{ VK_OEM_3, "VK_OEM_3" },
	{ VK_ABNT_C1, "VK_ABNT_C1" },
	{ VK_ABNT_C2, "VK_ABNT_C2" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_OEM_4, "VK_OEM_4" },
	{ VK_OEM_5, "VK_OEM_5" },
	{ VK_OEM_6, "VK_OEM_6" },
	{ VK_OEM_7, "VK_OEM_7" },
	{ VK_OEM_8, "VK_OEM_8" },
	{ 0, "" },
	{ 0, "" },
	{ VK_OEM_102, "VK_OEM_102" },
	{ 0, "" },
	{ 0, "" },
	{ VK_PROCESSKEY, "VK_PROCESSKEY" },
	{ 0, "" },
	{ VK_PACKET, "VK_PACKET" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ 0, "" },
	{ VK_ATTN, "VK_ATTN" },
	{ VK_CRSEL, "VK_CRSEL" },
	{ VK_EXSEL, "VK_EXSEL" },
	{ VK_EREOF, "VK_EREOF" },
	{ VK_PLAY, "VK_PLAY" },
	{ VK_ZOOM, "VK_ZOOM" },
	{ VK_NONAME, "VK_NONAME" },
	{ VK_PA1, "VK_PA1" },
	{ VK_OEM_CLEAR, "VK_OEM_CLEAR" },
	{ 0, "" }
};

/* the index in this table is the virtual key code */

const uint32 VIRTUAL_KEY_CODE_TO_RDP_SCANCODE_TABLE[256] =
{
	0x00,
	0x00, /* VK_LBUTTON */
	0x00, /* VK_RBUTTON */
	0x00, /* VK_CANCEL */
	0x00, /* VK_MBUTTON */
	0x00, /* VK_XBUTTON1 */
	0x00, /* VK_XBUTTON2 */
	0x00,
	0x0E, /* VK_BACK */
	0x0F, /* VK_TAB */
	0x00,
	0x00,
	0x00, /* VK_CLEAR */
	0x1C, /* VK_RETURN */
	0x00,
	0x00,
	0x2A, /* VK_SHIFT */
	0x00, /* VK_CONTROL */
	0x38, /* VK_MENU */
	0x46, /* VK_PAUSE */
	0x3A, /* VK_CAPITAL */
	0x72, /* VK_KANA / VK_HANGUL */
	0x00,
	0x00, /* VK_JUNJA */
	0x00, /* VK_FINAL */
	0x71, /* VK_HANJA / VK_KANJI */
	0x00,
	0x01, /* VK_ESCAPE */
	0x00, /* VK_CONVERT */
	0x00, /* VK_NONCONVERT */
	0x00, /* VK_ACCEPT */
	0x00, /* VK_MODECHANGE */
	0x39, /* VK_SPACE */
	0x49, /* VK_PRIOR */
	0x51, /* VK_NEXT */
	0x4F, /* VK_END */
	0x47, /* VK_HOME */
	0x4B, /* VK_LEFT */
	0x48, /* VK_UP */
	0x4D, /* VK_RIGHT */
	0x50, /* VK_DOWN */
	0x00, /* VK_SELECT */
	0x37, /* VK_PRINT */
	0x37, /* VK_EXECUTE */
	0x37, /* VK_SNAPSHOT */
	0x52, /* VK_INSERT */
	0x53, /* VK_DELETE */
	0x63, /* VK_HELP */
	0x0B, /* VK_KEY_0 */
	0x02, /* VK_KEY_1 */
	0x03, /* VK_KEY_2 */
	0x04, /* VK_KEY_3 */
	0x05, /* VK_KEY_4 */
	0x06, /* VK_KEY_5 */
	0x07, /* VK_KEY_6 */
	0x08, /* VK_KEY_7 */
	0x09, /* VK_KEY_8 */
	0x0A, /* VK_KEY_9 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x1E, /* VK_KEY_A */
	0x30, /* VK_KEY_B */
	0x2E, /* VK_KEY_C */
	0x20, /* VK_KEY_D */
	0x12, /* VK_KEY_E */
	0x21, /* VK_KEY_F */
	0x22, /* VK_KEY_G */
	0x23, /* VK_KEY_H */
	0x17, /* VK_KEY_I */
	0x24, /* VK_KEY_J */
	0x25, /* VK_KEY_K */
	0x26, /* VK_KEY_L */
	0x32, /* VK_KEY_M */
	0x31, /* VK_KEY_N */
	0x18, /* VK_KEY_O */
	0x19, /* VK_KEY_P */
	0x10, /* VK_KEY_Q */
	0x13, /* VK_KEY_R */
	0x1F, /* VK_KEY_S */
	0x14, /* VK_KEY_T */
	0x16, /* VK_KEY_U */
	0x2F, /* VK_KEY_V */
	0x11, /* VK_KEY_W */
	0x2D, /* VK_KEY_X */
	0x15, /* VK_KEY_Y */
	0x2C, /* VK_KEY_Z */
	0x5B, /* VK_LWIN */
	0x5C, /* VK_RWIN */
	0x5D, /* VK_APPS */
	0x00,
	0x5F, /* VK_SLEEP */
	0x52, /* VK_NUMPAD0 */
	0x4F, /* VK_NUMPAD1 */
	0x50, /* VK_NUMPAD2 */
	0x51, /* VK_NUMPAD3 */
	0x4B, /* VK_NUMPAD4 */
	0x4C, /* VK_NUMPAD5 */
	0x4D, /* VK_NUMPAD6 */
	0x47, /* VK_NUMPAD7 */
	0x48, /* VK_NUMPAD8 */
	0x49, /* VK_NUMPAD9 */
	0x37, /* VK_MULTIPLY */
	0x4E, /* VK_ADD */
	0x00, /* VK_SEPARATOR */
	0x4A, /* VK_SUBTRACT */
	0x53, /* VK_DECIMAL */
	0x35, /* VK_DIVIDE */
	0x3B, /* VK_F1 */
	0x3C, /* VK_F2 */
	0x3D, /* VK_F3 */
	0x3E, /* VK_F4 */
	0x3F, /* VK_F5 */
	0x40, /* VK_F6 */
	0x41, /* VK_F7 */
	0x42, /* VK_F8 */
	0x43, /* VK_F9 */
	0x44, /* VK_F10 */
	0x57, /* VK_F11 */
	0x58, /* VK_F12 */
	0x64, /* VK_F13 */
	0x65, /* VK_F14 */
	0x66, /* VK_F15 */
	0x67, /* VK_F16 */
	0x68, /* VK_F17 */
	0x69, /* VK_F18 */
	0x6A, /* VK_F19 */
	0x6B, /* VK_F20 */
	0x6C, /* VK_F21 */
	0x6D, /* VK_F22 */
	0x6E, /* VK_F23 */
	0x6F, /* VK_F24 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x45, /* VK_NUMLOCK */
	0x46, /* VK_SCROLL */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x2A, /* VK_LSHIFT */
	0x36, /* VK_RSHIFT */
	0x1D, /* VK_LCONTROL */
	0x1D, /* VK_RCONTROL */
	0x38, /* VK_LMENU */
	0x38, /* VK_RMENU */
	0x00, /* VK_BROWSER_BACK */
	0x00, /* VK_BROWSER_FORWARD */
	0x00, /* VK_BROWSER_REFRESH */
	0x00, /* VK_BROWSER_STOP */
	0x00, /* VK_BROWSER_SEARCH */
	0x00, /* VK_BROWSER_FAVORITES */
	0x00, /* VK_BROWSER_HOME */
	0x00, /* VK_VOLUME_MUTE */
	0x00, /* VK_VOLUME_DOWN */
	0x00, /* VK_VOLUME_UP */
	0x00, /* VK_MEDIA_NEXT_TRACK */
	0x00, /* VK_MEDIA_PREV_TRACK */
	0x00, /* VK_MEDIA_STOP */
	0x00, /* VK_MEDIA_PLAY_PAUSE */
	0x00, /* VK_LAUNCH_MAIL */
	0x00, /* VK_MEDIA_SELECT */
	0x00, /* VK_LAUNCH_APP1 */
	0x00, /* VK_LAUNCH_APP2 */
	0x00,
	0x00,
	0x27, /* VK_OEM_1 */
	0x0D, /* VK_OEM_PLUS */
	0x33, /* VK_OEM_COMMA */
	0x0C, /* VK_OEM_MINUS */
	0x34, /* VK_OEM_PERIOD */
	0x35, /* VK_OEM_2 */
	0x29, /* VK_OEM_3 */
	0x73, /* VK_ABNT_C1 */
	0x7E, /* VK_ABNT_C2 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x1A, /* VK_OEM_4 */
	0x2B, /* VK_OEM_5 */
	0x1B, /* VK_OEM_6 */
	0x28, /* VK_OEM_7 */
	0x1D, /* VK_OEM_8 */
	0x00,
	0x00,
	0x56, /* VK_OEM_102 */
	0x00,
	0x00,
	0x00, /* VK_PROCESSKEY */
	0x00,
	0x00, /* VK_PACKET */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00, /* VK_ATTN */
	0x00, /* VK_CRSEL */
	0x00, /* VK_EXSEL */
	0x00, /* VK_EREOF */
	0x00, /* VK_PLAY */
	0x62, /* VK_ZOOM */
	0x00, /* VK_NONAME */
	0x00, /* VK_PA1 */
	0x00, /* VK_OEM_CLEAR */
	0x00
};

/* the index in this table is the virtual key code */

const RDP_SCANCODE VIRTUAL_KEY_CODE_TO_DEFAULT_RDP_SCANCODE_TABLE[256] =
{
	{ 0x00, 0 },
	{ 0x00, 0 }, /* VK_LBUTTON */
	{ 0x00, 0 }, /* VK_RBUTTON */
	{ 0x00, 0 }, /* VK_CANCEL */
	{ 0x00, 0 }, /* VK_MBUTTON */
	{ 0x00, 0 }, /* VK_XBUTTON1 */
	{ 0x00, 0 }, /* VK_XBUTTON2 */
	{ 0x00, 0 },
	{ 0x0E, 0 }, /* VK_BACK */
	{ 0x0F, 0 }, /* VK_TAB */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 }, /* VK_CLEAR */
	{ 0x1C, 0 }, /* VK_RETURN */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x2A, 0 }, /* VK_SHIFT */
	{ 0x00, 0 }, /* VK_CONTROL */
	{ 0x38, 0 }, /* VK_MENU */
	{ 0x46, 1 }, /* VK_PAUSE */
	{ 0x3A, 0 }, /* VK_CAPITAL */
	{ 0x72, 0 }, /* VK_KANA / VK_HANGUL */
	{ 0x00, 0 },
	{ 0x00, 0 }, /* VK_JUNJA */
	{ 0x00, 0 }, /* VK_FINAL */
	{ 0x71, 0 }, /* VK_HANJA / VK_KANJI */
	{ 0x00, 0 },
	{ 0x01, 0 }, /* VK_ESCAPE */
	{ 0x00, 0 }, /* VK_CONVERT */
	{ 0x00, 0 }, /* VK_NONCONVERT */
	{ 0x00, 0 }, /* VK_ACCEPT */
	{ 0x00, 0 }, /* VK_MODECHANGE */
	{ 0x39, 0 }, /* VK_SPACE */
	{ 0x49, 1 }, /* VK_PRIOR */
	{ 0x51, 1 }, /* VK_NEXT */
	{ 0x4F, 1 }, /* VK_END */
	{ 0x47, 1 }, /* VK_HOME */
	{ 0x4B, 1 }, /* VK_LEFT */
	{ 0x48, 1 }, /* VK_UP */
	{ 0x4D, 1 }, /* VK_RIGHT */
	{ 0x50, 1 }, /* VK_DOWN */
	{ 0x00, 0 }, /* VK_SELECT */
	{ 0x37, 1 }, /* VK_PRINT */
	{ 0x37, 1 }, /* VK_EXECUTE */
	{ 0x37, 1 }, /* VK_SNAPSHOT */
	{ 0x52, 1 }, /* VK_INSERT */
	{ 0x53, 1 }, /* VK_DELETE */
	{ 0x63, 0 }, /* VK_HELP */
	{ 0x0B, 0 }, /* VK_KEY_0 */
	{ 0x02, 0 }, /* VK_KEY_1 */
	{ 0x03, 0 }, /* VK_KEY_2 */
	{ 0x04, 0 }, /* VK_KEY_3 */
	{ 0x05, 0 }, /* VK_KEY_4 */
	{ 0x06, 0 }, /* VK_KEY_5 */
	{ 0x07, 0 }, /* VK_KEY_6 */
	{ 0x08, 0 }, /* VK_KEY_7 */
	{ 0x09, 0 }, /* VK_KEY_8 */
	{ 0x0A, 0 }, /* VK_KEY_9 */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x1E, 0 }, /* VK_KEY_A */
	{ 0x30, 0 }, /* VK_KEY_B */
	{ 0x2E, 0 }, /* VK_KEY_C */
	{ 0x20, 0 }, /* VK_KEY_D */
	{ 0x12, 0 }, /* VK_KEY_E */
	{ 0x21, 0 }, /* VK_KEY_F */
	{ 0x22, 0 }, /* VK_KEY_G */
	{ 0x23, 0 }, /* VK_KEY_H */
	{ 0x17, 0 }, /* VK_KEY_I */
	{ 0x24, 0 }, /* VK_KEY_J */
	{ 0x25, 0 }, /* VK_KEY_K */
	{ 0x26, 0 }, /* VK_KEY_L */
	{ 0x32, 0 }, /* VK_KEY_M */
	{ 0x31, 0 }, /* VK_KEY_N */
	{ 0x18, 0 }, /* VK_KEY_O */
	{ 0x19, 0 }, /* VK_KEY_P */
	{ 0x10, 0 }, /* VK_KEY_Q */
	{ 0x13, 0 }, /* VK_KEY_R */
	{ 0x1F, 0 }, /* VK_KEY_S */
	{ 0x14, 0 }, /* VK_KEY_T */
	{ 0x16, 0 }, /* VK_KEY_U */
	{ 0x2F, 0 }, /* VK_KEY_V */
	{ 0x11, 0 }, /* VK_KEY_W */
	{ 0x2D, 0 }, /* VK_KEY_X */
	{ 0x15, 0 }, /* VK_KEY_Y */
	{ 0x2C, 0 }, /* VK_KEY_Z */
	{ 0x5B, 1 }, /* VK_LWIN */
	{ 0x5C, 1 }, /* VK_RWIN */
	{ 0x5D, 1 }, /* VK_APPS */
	{ 0x00, 0 },
	{ 0x5F, 0 }, /* VK_SLEEP */
	{ 0x52, 0 }, /* VK_NUMPAD0 */
	{ 0x4F, 0 }, /* VK_NUMPAD1 */
	{ 0x50, 0 }, /* VK_NUMPAD2 */
	{ 0x51, 0 }, /* VK_NUMPAD3 */
	{ 0x4B, 0 }, /* VK_NUMPAD4 */
	{ 0x4C, 0 }, /* VK_NUMPAD5 */
	{ 0x4D, 0 }, /* VK_NUMPAD6 */
	{ 0x47, 0 }, /* VK_NUMPAD7 */
	{ 0x48, 0 }, /* VK_NUMPAD8 */
	{ 0x49, 0 }, /* VK_NUMPAD9 */
	{ 0x37, 0 }, /* VK_MULTIPLY */
	{ 0x4E, 0 }, /* VK_ADD */
	{ 0x00, 0 }, /* VK_SEPARATOR */
	{ 0x4A, 0 }, /* VK_SUBTRACT */
	{ 0x53, 0 }, /* VK_DECIMAL */
	{ 0x35, 1 }, /* VK_DIVIDE */
	{ 0x3B, 0 }, /* VK_F1 */
	{ 0x3C, 0 }, /* VK_F2 */
	{ 0x3D, 0 }, /* VK_F3 */
	{ 0x3E, 0 }, /* VK_F4 */
	{ 0x3F, 0 }, /* VK_F5 */
	{ 0x40, 0 }, /* VK_F6 */
	{ 0x41, 0 }, /* VK_F7 */
	{ 0x42, 0 }, /* VK_F8 */
	{ 0x43, 0 }, /* VK_F9 */
	{ 0x44, 0 }, /* VK_F10 */
	{ 0x57, 0 }, /* VK_F11 */
	{ 0x58, 0 }, /* VK_F12 */
	{ 0x64, 0 }, /* VK_F13 */
	{ 0x65, 0 }, /* VK_F14 */
	{ 0x66, 0 }, /* VK_F15 */
	{ 0x67, 0 }, /* VK_F16 */
	{ 0x68, 0 }, /* VK_F17 */
	{ 0x69, 0 }, /* VK_F18 */
	{ 0x6A, 0 }, /* VK_F19 */
	{ 0x6B, 0 }, /* VK_F20 */
	{ 0x6C, 0 }, /* VK_F21 */
	{ 0x6D, 0 }, /* VK_F22 */
	{ 0x6E, 0 }, /* VK_F23 */
	{ 0x6F, 0 }, /* VK_F24 */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x45, 0 }, /* VK_NUMLOCK */
	{ 0x46, 0 }, /* VK_SCROLL */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x2A, 0 }, /* VK_LSHIFT */
	{ 0x36, 0 }, /* VK_RSHIFT */
	{ 0x1D, 0 }, /* VK_LCONTROL */
	{ 0x1D, 1 }, /* VK_RCONTROL */
	{ 0x38, 0 }, /* VK_LMENU */
	{ 0x38, 1 }, /* VK_RMENU */
	{ 0x00, 0 }, /* VK_BROWSER_BACK */
	{ 0x00, 0 }, /* VK_BROWSER_FORWARD */
	{ 0x00, 0 }, /* VK_BROWSER_REFRESH */
	{ 0x00, 0 }, /* VK_BROWSER_STOP */
	{ 0x00, 0 }, /* VK_BROWSER_SEARCH */
	{ 0x00, 0 }, /* VK_BROWSER_FAVORITES */
	{ 0x00, 0 }, /* VK_BROWSER_HOME */
	{ 0x00, 0 }, /* VK_VOLUME_MUTE */
	{ 0x00, 0 }, /* VK_VOLUME_DOWN */
	{ 0x00, 0 }, /* VK_VOLUME_UP */
	{ 0x00, 0 }, /* VK_MEDIA_NEXT_TRACK */
	{ 0x00, 0 }, /* VK_MEDIA_PREV_TRACK */
	{ 0x00, 0 }, /* VK_MEDIA_STOP */
	{ 0x00, 0 }, /* VK_MEDIA_PLAY_PAUSE */
	{ 0x00, 0 }, /* VK_LAUNCH_MAIL */
	{ 0x00, 0 }, /* VK_MEDIA_SELECT */
	{ 0x00, 0 }, /* VK_LAUNCH_APP1 */
	{ 0x00, 0 }, /* VK_LAUNCH_APP2 */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x27, 0 }, /* VK_OEM_1 */
	{ 0x0D, 0 }, /* VK_OEM_PLUS */
	{ 0x33, 0 }, /* VK_OEM_COMMA */
	{ 0x0C, 0 }, /* VK_OEM_MINUS */
	{ 0x34, 0 }, /* VK_OEM_PERIOD */
	{ 0x35, 0 }, /* VK_OEM_2 */
	{ 0x29, 0 }, /* VK_OEM_3 */
	{ 0x73, 0 }, /* VK_ABNT_C1 */
	{ 0x7E, 0 }, /* VK_ABNT_C2 */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x1A, 0 }, /* VK_OEM_4 */
	{ 0x2B, 0 }, /* VK_OEM_5 */
	{ 0x1B, 0 }, /* VK_OEM_6 */
	{ 0x28, 0 }, /* VK_OEM_7 */
	{ 0x1D, 0 }, /* VK_OEM_8 */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x56, 0 }, /* VK_OEM_102 */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 }, /* VK_PROCESSKEY */
	{ 0x00, 0 },
	{ 0x00, 0 }, /* VK_PACKET */
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 },
	{ 0x00, 0 }, /* VK_ATTN */
	{ 0x00, 0 }, /* VK_CRSEL */
	{ 0x00, 0 }, /* VK_EXSEL */
	{ 0x00, 0 }, /* VK_EREOF */
	{ 0x00, 0 }, /* VK_PLAY */
	{ 0x62, 0 }, /* VK_ZOOM */
	{ 0x00, 0 }, /* VK_NONAME */
	{ 0x00, 0 }, /* VK_PA1 */
	{ 0x00, 0 }, /* VK_OEM_CLEAR */
	{ 0x00, 0 }
};

RDP_KEYBOARD_LAYOUT* freerdp_keyboard_get_layouts(uint32 types)
{
	int num, length, i;
	RDP_KEYBOARD_LAYOUT* layouts;

	num = 0;
	layouts = (RDP_KEYBOARD_LAYOUT*) xmalloc((num + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_STANDARD) != 0)
	{
		length = sizeof(RDP_KEYBOARD_LAYOUT_TABLE) / sizeof(RDP_KEYBOARD_LAYOUT);
		layouts = (RDP_KEYBOARD_LAYOUT*) xrealloc(layouts, (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_LAYOUT_TABLE[i].code;
			layouts[num].name = xstrdup(RDP_KEYBOARD_LAYOUT_TABLE[i].name);
		}
	}
	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_VARIANT) != 0)
	{
		length = sizeof(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE) / sizeof(RDP_KEYBOARD_LAYOUT_VARIANT);
		layouts = (RDP_KEYBOARD_LAYOUT*) xrealloc(layouts, (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].code;
			layouts[num].name = xstrdup(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].name);
		}
	}
	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_IME) != 0)
	{
		length = sizeof(RDP_KEYBOARD_IME_TABLE) / sizeof(RDP_KEYBOARD_IME);
		layouts = (RDP_KEYBOARD_LAYOUT*) realloc(layouts, (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_IME_TABLE[i].code;
			layouts[num].name = xstrdup(RDP_KEYBOARD_IME_TABLE[i].name);
		}
	}

	memset(&layouts[num], 0, sizeof(RDP_KEYBOARD_LAYOUT));

	return layouts;
}

const char* freerdp_keyboard_get_layout_name_from_id(uint32 keyboardLayoutID)
{
	int i;

	for (i = 0; i < sizeof(RDP_KEYBOARD_LAYOUT_TABLE) / sizeof(RDP_KEYBOARD_LAYOUT); i++)
	{
		if (RDP_KEYBOARD_LAYOUT_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_LAYOUT_TABLE[i].name;
	}

	for (i = 0; i < sizeof(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE) / sizeof(RDP_KEYBOARD_LAYOUT_VARIANT); i++)
	{
		if (RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].name;
	}

	for (i = 0; i < sizeof(RDP_KEYBOARD_IME_TABLE) / sizeof(RDP_KEYBOARD_IME); i++)
	{
		if (RDP_KEYBOARD_IME_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_IME_TABLE[i].name;
	}

	return "unknown";
}
