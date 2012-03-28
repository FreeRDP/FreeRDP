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


/* the index in this table is the virtual key code */

const RDP_SCANCODE VIRTUAL_KEY_CODE_TO_DEFAULT_RDP_SCANCODE_TABLE[256] =
{
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false), /* VK_LBUTTON */
	mk_rdp_scancode(0x00, false), /* VK_RBUTTON */
	mk_rdp_scancode(0x00, false), /* VK_CANCEL */
	mk_rdp_scancode(0x00, false), /* VK_MBUTTON */
	mk_rdp_scancode(0x00, false), /* VK_XBUTTON1 */
	mk_rdp_scancode(0x00, false), /* VK_XBUTTON2 */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x0E, false), /* VK_BACK */
	mk_rdp_scancode(0x0F, false), /* VK_TAB */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false), /* VK_CLEAR */
	mk_rdp_scancode(0x1C, false), /* VK_RETURN */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x2A, false), /* VK_SHIFT */
	mk_rdp_scancode(0x00, false), /* VK_CONTROL */
	mk_rdp_scancode(0x38, false), /* VK_MENU */
	mk_rdp_scancode(0x46, true),  /* VK_PAUSE */
	mk_rdp_scancode(0x3A, false), /* VK_CAPITAL */
	mk_rdp_scancode(0x72, false), /* VK_KANA / VK_HANGUL */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false), /* VK_JUNJA */
	mk_rdp_scancode(0x00, false), /* VK_FINAL */
	mk_rdp_scancode(0x71, false), /* VK_HANJA / VK_KANJI */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x01, false), /* VK_ESCAPE */
	mk_rdp_scancode(0x00, false), /* VK_CONVERT */
	mk_rdp_scancode(0x00, false), /* VK_NONCONVERT */
	mk_rdp_scancode(0x00, false), /* VK_ACCEPT */
	mk_rdp_scancode(0x00, false), /* VK_MODECHANGE */
	mk_rdp_scancode(0x39, false), /* VK_SPACE */
	mk_rdp_scancode(0x49, true),  /* VK_PRIOR */
	mk_rdp_scancode(0x51, true),  /* VK_NEXT */
	mk_rdp_scancode(0x4F, true),  /* VK_END */
	mk_rdp_scancode(0x47, true),  /* VK_HOME */
	mk_rdp_scancode(0x4B, true),  /* VK_LEFT */
	mk_rdp_scancode(0x48, true),  /* VK_UP */
	mk_rdp_scancode(0x4D, true),  /* VK_RIGHT */
	mk_rdp_scancode(0x50, true),  /* VK_DOWN */
	mk_rdp_scancode(0x00, false), /* VK_SELECT */
	mk_rdp_scancode(0x37, true),  /* VK_PRINT */
	mk_rdp_scancode(0x37, true),  /* VK_EXECUTE */
	mk_rdp_scancode(0x37, true),  /* VK_SNAPSHOT */
	mk_rdp_scancode(0x52, true),  /* VK_INSERT */
	mk_rdp_scancode(0x53, true),  /* VK_DELETE */
	mk_rdp_scancode(0x63, false), /* VK_HELP */
	mk_rdp_scancode(0x0B, false), /* VK_KEY_0 */
	mk_rdp_scancode(0x02, false), /* VK_KEY_1 */
	mk_rdp_scancode(0x03, false), /* VK_KEY_2 */
	mk_rdp_scancode(0x04, false), /* VK_KEY_3 */
	mk_rdp_scancode(0x05, false), /* VK_KEY_4 */
	mk_rdp_scancode(0x06, false), /* VK_KEY_5 */
	mk_rdp_scancode(0x07, false), /* VK_KEY_6 */
	mk_rdp_scancode(0x08, false), /* VK_KEY_7 */
	mk_rdp_scancode(0x09, false), /* VK_KEY_8 */
	mk_rdp_scancode(0x0A, false), /* VK_KEY_9 */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x1E, false), /* VK_KEY_A */
	mk_rdp_scancode(0x30, false), /* VK_KEY_B */
	mk_rdp_scancode(0x2E, false), /* VK_KEY_C */
	mk_rdp_scancode(0x20, false), /* VK_KEY_D */
	mk_rdp_scancode(0x12, false), /* VK_KEY_E */
	mk_rdp_scancode(0x21, false), /* VK_KEY_F */
	mk_rdp_scancode(0x22, false), /* VK_KEY_G */
	mk_rdp_scancode(0x23, false), /* VK_KEY_H */
	mk_rdp_scancode(0x17, false), /* VK_KEY_I */
	mk_rdp_scancode(0x24, false), /* VK_KEY_J */
	mk_rdp_scancode(0x25, false), /* VK_KEY_K */
	mk_rdp_scancode(0x26, false), /* VK_KEY_L */
	mk_rdp_scancode(0x32, false), /* VK_KEY_M */
	mk_rdp_scancode(0x31, false), /* VK_KEY_N */
	mk_rdp_scancode(0x18, false), /* VK_KEY_O */
	mk_rdp_scancode(0x19, false), /* VK_KEY_P */
	mk_rdp_scancode(0x10, false), /* VK_KEY_Q */
	mk_rdp_scancode(0x13, false), /* VK_KEY_R */
	mk_rdp_scancode(0x1F, false), /* VK_KEY_S */
	mk_rdp_scancode(0x14, false), /* VK_KEY_T */
	mk_rdp_scancode(0x16, false), /* VK_KEY_U */
	mk_rdp_scancode(0x2F, false), /* VK_KEY_V */
	mk_rdp_scancode(0x11, false), /* VK_KEY_W */
	mk_rdp_scancode(0x2D, false), /* VK_KEY_X */
	mk_rdp_scancode(0x15, false), /* VK_KEY_Y */
	mk_rdp_scancode(0x2C, false), /* VK_KEY_Z */
	mk_rdp_scancode(0x5B, true),  /* VK_LWIN */
	mk_rdp_scancode(0x5C, true),  /* VK_RWIN */
	mk_rdp_scancode(0x5D, true),  /* VK_APPS */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x5F, false), /* VK_SLEEP */
	mk_rdp_scancode(0x52, false), /* VK_NUMPAD0 */
	mk_rdp_scancode(0x4F, false), /* VK_NUMPAD1 */
	mk_rdp_scancode(0x50, false), /* VK_NUMPAD2 */
	mk_rdp_scancode(0x51, false), /* VK_NUMPAD3 */
	mk_rdp_scancode(0x4B, false), /* VK_NUMPAD4 */
	mk_rdp_scancode(0x4C, false), /* VK_NUMPAD5 */
	mk_rdp_scancode(0x4D, false), /* VK_NUMPAD6 */
	mk_rdp_scancode(0x47, false), /* VK_NUMPAD7 */
	mk_rdp_scancode(0x48, false), /* VK_NUMPAD8 */
	mk_rdp_scancode(0x49, false), /* VK_NUMPAD9 */
	mk_rdp_scancode(0x37, false), /* VK_MULTIPLY */
	mk_rdp_scancode(0x4E, false), /* VK_ADD */
	mk_rdp_scancode(0x00, false), /* VK_SEPARATOR */
	mk_rdp_scancode(0x4A, false), /* VK_SUBTRACT */
	mk_rdp_scancode(0x53, false), /* VK_DECIMAL */
	mk_rdp_scancode(0x35, true),  /* VK_DIVIDE */
	mk_rdp_scancode(0x3B, false), /* VK_F1 */
	mk_rdp_scancode(0x3C, false), /* VK_F2 */
	mk_rdp_scancode(0x3D, false), /* VK_F3 */
	mk_rdp_scancode(0x3E, false), /* VK_F4 */
	mk_rdp_scancode(0x3F, false), /* VK_F5 */
	mk_rdp_scancode(0x40, false), /* VK_F6 */
	mk_rdp_scancode(0x41, false), /* VK_F7 */
	mk_rdp_scancode(0x42, false), /* VK_F8 */
	mk_rdp_scancode(0x43, false), /* VK_F9 */
	mk_rdp_scancode(0x44, false), /* VK_F10 */
	mk_rdp_scancode(0x57, false), /* VK_F11 */
	mk_rdp_scancode(0x58, false), /* VK_F12 */
	mk_rdp_scancode(0x64, false), /* VK_F13 */
	mk_rdp_scancode(0x65, false), /* VK_F14 */
	mk_rdp_scancode(0x66, false), /* VK_F15 */
	mk_rdp_scancode(0x67, false), /* VK_F16 */
	mk_rdp_scancode(0x68, false), /* VK_F17 */
	mk_rdp_scancode(0x69, false), /* VK_F18 */
	mk_rdp_scancode(0x6A, false), /* VK_F19 */
	mk_rdp_scancode(0x6B, false), /* VK_F20 */
	mk_rdp_scancode(0x6C, false), /* VK_F21 */
	mk_rdp_scancode(0x6D, false), /* VK_F22 */
	mk_rdp_scancode(0x6E, false), /* VK_F23 */
	mk_rdp_scancode(0x6F, false), /* VK_F24 */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x45, false), /* VK_NUMLOCK */
	mk_rdp_scancode(0x46, false), /* VK_SCROLL */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x2A, false), /* VK_LSHIFT */
	mk_rdp_scancode(0x36, false), /* VK_RSHIFT */
	mk_rdp_scancode(0x1D, false), /* VK_LCONTROL */
	mk_rdp_scancode(0x1D, true),  /* VK_RCONTROL */
	mk_rdp_scancode(0x38, false), /* VK_LMENU */
	mk_rdp_scancode(0x38, true),  /* VK_RMENU */
	mk_rdp_scancode(0x00, false), /* VK_BROWSER_BACK */
	mk_rdp_scancode(0x00, false), /* VK_BROWSER_FORWARD */
	mk_rdp_scancode(0x00, false), /* VK_BROWSER_REFRESH */
	mk_rdp_scancode(0x00, false), /* VK_BROWSER_STOP */
	mk_rdp_scancode(0x00, false), /* VK_BROWSER_SEARCH */
	mk_rdp_scancode(0x00, false), /* VK_BROWSER_FAVORITES */
	mk_rdp_scancode(0x00, false), /* VK_BROWSER_HOME */
	mk_rdp_scancode(0x00, false), /* VK_VOLUME_MUTE */
	mk_rdp_scancode(0x00, false), /* VK_VOLUME_DOWN */
	mk_rdp_scancode(0x00, false), /* VK_VOLUME_UP */
	mk_rdp_scancode(0x00, false), /* VK_MEDIA_NEXT_TRACK */
	mk_rdp_scancode(0x00, false), /* VK_MEDIA_PREV_TRACK */
	mk_rdp_scancode(0x00, false), /* VK_MEDIA_STOP */
	mk_rdp_scancode(0x00, false), /* VK_MEDIA_PLAY_PAUSE */
	mk_rdp_scancode(0x00, false), /* VK_LAUNCH_MAIL */
	mk_rdp_scancode(0x00, false), /* VK_MEDIA_SELECT */
	mk_rdp_scancode(0x00, false), /* VK_LAUNCH_APP1 */
	mk_rdp_scancode(0x00, false), /* VK_LAUNCH_APP2 */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x27, false), /* VK_OEM_1 */
	mk_rdp_scancode(0x0D, false), /* VK_OEM_PLUS */
	mk_rdp_scancode(0x33, false), /* VK_OEM_COMMA */
	mk_rdp_scancode(0x0C, false), /* VK_OEM_MINUS */
	mk_rdp_scancode(0x34, false), /* VK_OEM_PERIOD */
	mk_rdp_scancode(0x35, false), /* VK_OEM_2 */
	mk_rdp_scancode(0x29, false), /* VK_OEM_3 */
	mk_rdp_scancode(0x73, false), /* VK_ABNT_C1 */
	mk_rdp_scancode(0x7E, false), /* VK_ABNT_C2 */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x1A, false), /* VK_OEM_4 */
	mk_rdp_scancode(0x2B, false), /* VK_OEM_5 */
	mk_rdp_scancode(0x1B, false), /* VK_OEM_6 */
	mk_rdp_scancode(0x28, false), /* VK_OEM_7 */
	mk_rdp_scancode(0x1D, false), /* VK_OEM_8 */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x56, false), /* VK_OEM_102 */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false), /* VK_PROCESSKEY */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false), /* VK_PACKET */
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false),
	mk_rdp_scancode(0x00, false), /* VK_ATTN */
	mk_rdp_scancode(0x00, false), /* VK_CRSEL */
	mk_rdp_scancode(0x00, false), /* VK_EXSEL */
	mk_rdp_scancode(0x00, false), /* VK_EREOF */
	mk_rdp_scancode(0x00, false), /* VK_PLAY */
	mk_rdp_scancode(0x62, false), /* VK_ZOOM */
	mk_rdp_scancode(0x00, false), /* VK_NONAME */
	mk_rdp_scancode(0x00, false), /* VK_PA1 */
	mk_rdp_scancode(0x00, false), /* VK_OEM_CLEAR */
	mk_rdp_scancode(0x00, false)
};

RDP_KEYBOARD_LAYOUT* freerdp_keyboard_get_layouts(uint32 types)
{
	int num, length, i;
	RDP_KEYBOARD_LAYOUT* layouts;

	num = 0;
	layouts = (RDP_KEYBOARD_LAYOUT*) xmalloc((num + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_STANDARD) != 0)
	{
		length = ARRAY_SIZE(RDP_KEYBOARD_LAYOUT_TABLE);
		layouts = (RDP_KEYBOARD_LAYOUT*) xrealloc(layouts, (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_LAYOUT_TABLE[i].code;
			layouts[num].name = xstrdup(RDP_KEYBOARD_LAYOUT_TABLE[i].name);
		}
	}
	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_VARIANT) != 0)
	{
		length = ARRAY_SIZE(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE);
		layouts = (RDP_KEYBOARD_LAYOUT*) xrealloc(layouts, (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].code;
			layouts[num].name = xstrdup(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].name);
		}
	}
	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_IME) != 0)
	{
		length = ARRAY_SIZE(RDP_KEYBOARD_IME_TABLE);
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

	for (i = 0; i < ARRAY_SIZE(RDP_KEYBOARD_LAYOUT_TABLE); i++)
	{
		if (RDP_KEYBOARD_LAYOUT_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_LAYOUT_TABLE[i].name;
	}

	for (i = 0; i < ARRAY_SIZE(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE); i++)
	{
		if (RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].name;
	}

	for (i = 0; i < ARRAY_SIZE(RDP_KEYBOARD_IME_TABLE); i++)
	{
		if (RDP_KEYBOARD_IME_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_IME_TABLE[i].name;
	}

	return "unknown";
}
