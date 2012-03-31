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
#include <freerdp/keyboard_scancode.h>
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
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,    /* VK_LBUTTON */
	RDP_SCANCODE_UNKNOWN,    /* VK_RBUTTON */
	RDP_SCANCODE_UNKNOWN,    /* VK_CANCEL */
	RDP_SCANCODE_UNKNOWN,    /* VK_MBUTTON */
	RDP_SCANCODE_UNKNOWN,    /* VK_XBUTTON1 */
	RDP_SCANCODE_UNKNOWN,    /* VK_XBUTTON2 */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_BACKSPACE,  /* VK_BACK */
	RDP_SCANCODE_TAB,        /* VK_TAB */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,    /* VK_CLEAR */
	RDP_SCANCODE_RETURN,     /* VK_RETURN */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_LSHIFT,     /* VK_SHIFT */
	RDP_SCANCODE_UNKNOWN,    /* VK_CONTROL */
	RDP_SCANCODE_LMENU,      /* VK_MENU */
	RDP_SCANCODE_PAUSE,      /* VK_PAUSE */
	RDP_SCANCODE_CAPSLOCK,   /* VK_CAPITAL */
	RDP_SCANCODE_UNKNOWN,    /* VK_KANA / VK_HANGUL */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,    /* VK_JUNJA */
	RDP_SCANCODE_UNKNOWN,    /* VK_FINAL */
	RDP_SCANCODE_UNKNOWN,    /* VK_HANJA / VK_KANJI */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_ESCAPE,     /* VK_ESCAPE */
	RDP_SCANCODE_UNKNOWN,    /* VK_CONVERT */
	RDP_SCANCODE_UNKNOWN,    /* VK_NONCONVERT */
	RDP_SCANCODE_UNKNOWN,    /* VK_ACCEPT */
	RDP_SCANCODE_UNKNOWN,    /* VK_MODECHANGE */
	RDP_SCANCODE_SPACE,      /* VK_SPACE */
	RDP_SCANCODE_PRIOR,      /* VK_PRIOR */
	RDP_SCANCODE_NEXT,       /* VK_NEXT */
	RDP_SCANCODE_END,        /* VK_END */
	RDP_SCANCODE_HOME,       /* VK_HOME */
	RDP_SCANCODE_LEFT,       /* VK_LEFT */
	RDP_SCANCODE_UP,         /* VK_UP */
	RDP_SCANCODE_RIGHT,      /* VK_RIGHT */
	RDP_SCANCODE_DOWN,       /* VK_DOWN */
	RDP_SCANCODE_UNKNOWN,    /* VK_SELECT */
	RDP_SCANCODE_PRINTSCREEN,/* VK_PRINT */
	RDP_SCANCODE_PRINTSCREEN,/* VK_EXECUTE */
	RDP_SCANCODE_PRINTSCREEN,/* VK_SNAPSHOT */
	RDP_SCANCODE_INSERT,     /* VK_INSERT */
	RDP_SCANCODE_DELETE,     /* VK_DELETE */
	RDP_SCANCODE_HELP,       /* VK_HELP */
	RDP_SCANCODE_KEY_0,      /* VK_KEY_0 */
	RDP_SCANCODE_KEY_1,      /* VK_KEY_1 */
	RDP_SCANCODE_KEY_2,      /* VK_KEY_2 */
	RDP_SCANCODE_KEY_3,      /* VK_KEY_3 */
	RDP_SCANCODE_KEY_4,      /* VK_KEY_4 */
	RDP_SCANCODE_KEY_5,      /* VK_KEY_5 */
	RDP_SCANCODE_KEY_6,      /* VK_KEY_6 */
	RDP_SCANCODE_KEY_7,      /* VK_KEY_7 */
	RDP_SCANCODE_KEY_8,      /* VK_KEY_8 */
	RDP_SCANCODE_KEY_9,      /* VK_KEY_9 */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_KEY_A,      /* VK_KEY_A */
	RDP_SCANCODE_KEY_B,      /* VK_KEY_B */
	RDP_SCANCODE_KEY_C,      /* VK_KEY_C */
	RDP_SCANCODE_KEY_D,      /* VK_KEY_D */
	RDP_SCANCODE_KEY_E,      /* VK_KEY_E */
	RDP_SCANCODE_KEY_F,      /* VK_KEY_F */
	RDP_SCANCODE_KEY_G,      /* VK_KEY_G */
	RDP_SCANCODE_KEY_H,      /* VK_KEY_H */
	RDP_SCANCODE_KEY_I,      /* VK_KEY_I */
	RDP_SCANCODE_KEY_J,      /* VK_KEY_J */
	RDP_SCANCODE_KEY_K,      /* VK_KEY_K */
	RDP_SCANCODE_KEY_L,      /* VK_KEY_L */
	RDP_SCANCODE_KEY_M,      /* VK_KEY_M */
	RDP_SCANCODE_KEY_N,      /* VK_KEY_N */
	RDP_SCANCODE_KEY_O,      /* VK_KEY_O */
	RDP_SCANCODE_KEY_P,      /* VK_KEY_P */
	RDP_SCANCODE_KEY_Q,      /* VK_KEY_Q */
	RDP_SCANCODE_KEY_R,      /* VK_KEY_R */
	RDP_SCANCODE_KEY_S,      /* VK_KEY_S */
	RDP_SCANCODE_KEY_T,      /* VK_KEY_T */
	RDP_SCANCODE_KEY_U,      /* VK_KEY_U */
	RDP_SCANCODE_KEY_V,      /* VK_KEY_V */
	RDP_SCANCODE_KEY_W,      /* VK_KEY_W */
	RDP_SCANCODE_KEY_X,      /* VK_KEY_X */
	RDP_SCANCODE_KEY_Y,      /* VK_KEY_Y */
	RDP_SCANCODE_KEY_Z,      /* VK_KEY_Z */
	RDP_SCANCODE_LWIN,       /* VK_LWIN */
	RDP_SCANCODE_RWIN,       /* VK_RWIN */
	RDP_SCANCODE_APPS,       /* VK_APPS */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_SLEEP,      /* VK_SLEEP */
	RDP_SCANCODE_NUMPAD0,    /* VK_NUMPAD0 */
	RDP_SCANCODE_NUMPAD1,    /* VK_NUMPAD1 */
	RDP_SCANCODE_NUMPAD2,    /* VK_NUMPAD2 */
	RDP_SCANCODE_NUMPAD3,    /* VK_NUMPAD3 */
	RDP_SCANCODE_NUMPAD4,    /* VK_NUMPAD4 */
	RDP_SCANCODE_NUMPAD5,    /* VK_NUMPAD5 */
	RDP_SCANCODE_NUMPAD6,    /* VK_NUMPAD6 */
	RDP_SCANCODE_NUMPAD7,    /* VK_NUMPAD7 */
	RDP_SCANCODE_NUMPAD8,    /* VK_NUMPAD8 */
	RDP_SCANCODE_NUMPAD9,    /* VK_NUMPAD9 */
	RDP_SCANCODE_MULTIPLY,   /* VK_MULTIPLY */
	RDP_SCANCODE_ADD,        /* VK_ADD */
	RDP_SCANCODE_UNKNOWN,    /* VK_SEPARATOR */
	RDP_SCANCODE_SUBTRACT,   /* VK_SUBTRACT */
	RDP_SCANCODE_DECIMAL,    /* VK_DECIMAL */
	RDP_SCANCODE_DIVIDE,     /* VK_DIVIDE */
	RDP_SCANCODE_F1,         /* VK_F1 */
	RDP_SCANCODE_F2,         /* VK_F2 */
	RDP_SCANCODE_F3,         /* VK_F3 */
	RDP_SCANCODE_F4,         /* VK_F4 */
	RDP_SCANCODE_F5,         /* VK_F5 */
	RDP_SCANCODE_F6,         /* VK_F6 */
	RDP_SCANCODE_F7,         /* VK_F7 */
	RDP_SCANCODE_F8,         /* VK_F8 */
	RDP_SCANCODE_F9,         /* VK_F9 */
	RDP_SCANCODE_F10,        /* VK_F10 */
	RDP_SCANCODE_F11,        /* VK_F11 */
	RDP_SCANCODE_F12,        /* VK_F12 */
	RDP_SCANCODE_F13,        /* VK_F13 */
	RDP_SCANCODE_F14,        /* VK_F14 */
	RDP_SCANCODE_F15,        /* VK_F15 */
	RDP_SCANCODE_F16,        /* VK_F16 */
	RDP_SCANCODE_F17,        /* VK_F17 */
	RDP_SCANCODE_F18,        /* VK_F18 */
	RDP_SCANCODE_F19,        /* VK_F19 */
	RDP_SCANCODE_F20,        /* VK_F20 */
	RDP_SCANCODE_F21,        /* VK_F21 */
	RDP_SCANCODE_F22,        /* VK_F22 */
	RDP_SCANCODE_F23,        /* VK_F23 */
	RDP_SCANCODE_F24,        /* VK_F24 */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_NUMLOCK,    /* VK_NUMLOCK */
	RDP_SCANCODE_SCROLLLOCK, /* VK_SCROLL */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_LSHIFT,     /* VK_LSHIFT */
	RDP_SCANCODE_RSHIFT,     /* VK_RSHIFT */
	RDP_SCANCODE_LCONTROL,   /* VK_LCONTROL */
	RDP_SCANCODE_RCONTROL,   /* VK_RCONTROL */
	RDP_SCANCODE_LMENU,      /* VK_LMENU */
	RDP_SCANCODE_RMENU,      /* VK_RMENU */
	RDP_SCANCODE_UNKNOWN,    /* VK_BROWSER_BACK */
	RDP_SCANCODE_UNKNOWN,    /* VK_BROWSER_FORWARD */
	RDP_SCANCODE_UNKNOWN,    /* VK_BROWSER_REFRESH */
	RDP_SCANCODE_UNKNOWN,    /* VK_BROWSER_STOP */
	RDP_SCANCODE_UNKNOWN,    /* VK_BROWSER_SEARCH */
	RDP_SCANCODE_UNKNOWN,    /* VK_BROWSER_FAVORITES */
	RDP_SCANCODE_UNKNOWN,    /* VK_BROWSER_HOME */
	RDP_SCANCODE_UNKNOWN,    /* VK_VOLUME_MUTE */
	RDP_SCANCODE_UNKNOWN,    /* VK_VOLUME_DOWN */
	RDP_SCANCODE_UNKNOWN,    /* VK_VOLUME_UP */
	RDP_SCANCODE_UNKNOWN,    /* VK_MEDIA_NEXT_TRACK */
	RDP_SCANCODE_UNKNOWN,    /* VK_MEDIA_PREV_TRACK */
	RDP_SCANCODE_UNKNOWN,    /* VK_MEDIA_STOP */
	RDP_SCANCODE_UNKNOWN,    /* VK_MEDIA_PLAY_PAUSE */
	RDP_SCANCODE_UNKNOWN,    /* VK_LAUNCH_MAIL */
	RDP_SCANCODE_UNKNOWN,    /* VK_MEDIA_SELECT */
	RDP_SCANCODE_UNKNOWN,    /* VK_LAUNCH_APP1 */
	RDP_SCANCODE_UNKNOWN,    /* VK_LAUNCH_APP2 */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_OEM_1,      /* VK_OEM_1 */
	RDP_SCANCODE_OEM_PLUS,   /* VK_OEM_PLUS */
	RDP_SCANCODE_OEM_COMMA,  /* VK_OEM_COMMA */
	RDP_SCANCODE_OEM_MINUS,  /* VK_OEM_MINUS */
	RDP_SCANCODE_OEM_PERIOD, /* VK_OEM_PERIOD */
	RDP_SCANCODE_OEM_2,      /* VK_OEM_2 */
	RDP_SCANCODE_OEM_3,      /* VK_OEM_3 */
	RDP_SCANCODE_ABNT_C1,    /* VK_ABNT_C1 */
	RDP_SCANCODE_ABNT_C2,    /* VK_ABNT_C2 */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_OEM_4,      /* VK_OEM_4 */
	RDP_SCANCODE_OEM_5,      /* VK_OEM_5 */
	RDP_SCANCODE_OEM_6,      /* VK_OEM_6 */
	RDP_SCANCODE_OEM_7,      /* VK_OEM_7 */
	RDP_SCANCODE_LCONTROL,   /* VK_OEM_8 */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_OEM_102,    /* VK_OEM_102 */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,    /* VK_PROCESSKEY */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,    /* VK_PACKET */
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,
	RDP_SCANCODE_UNKNOWN,    /* VK_ATTN */
	RDP_SCANCODE_UNKNOWN,    /* VK_CRSEL */
	RDP_SCANCODE_UNKNOWN,    /* VK_EXSEL */
	RDP_SCANCODE_UNKNOWN,    /* VK_EREOF */
	RDP_SCANCODE_UNKNOWN,    /* VK_PLAY */
	RDP_SCANCODE_ZOOM,       /* VK_ZOOM */
	RDP_SCANCODE_UNKNOWN,    /* VK_NONAME */
	RDP_SCANCODE_UNKNOWN,    /* VK_PA1 */
	RDP_SCANCODE_UNKNOWN,    /* VK_OEM_CLEAR */
	RDP_SCANCODE_UNKNOWN
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
