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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include "liblocale.h"

#include <freerdp/types.h>
#include <freerdp/scancode.h>
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
	{ KBD_CANADIAN_ENGLISH,		"Canadian English" },
	{ KBD_SWISS_FRENCH,		"Swiss French" },
	{ KBD_BOSNIAN,			"Bosnian" },
	{ KBD_IRISH,			"Irish" },
	{ KBD_BOSNIAN_CYRILLIC,		"Bosnian Cyrillic" }
};

struct _RDP_KEYBOARD_LAYOUT_VARIANT
{
	DWORD code; /* Keyboard layout code */
	DWORD id; /* Keyboard variant ID */
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
	{ KBD_UNITED_STATES_DVORAK_PROGRAMMER,          0x001C, "United States-Programmer Dvorak" },
	{ KBD_GREEK_LATIN,				0x0019, "Greek Latin" },
	{ KBD_US_ENGLISH_TABLE_FOR_IBM_ARABIC_238_L,	0x000B, "US English Table for IBM Arabic 238_L" },
	{ KBD_GREEK_POLYTONIC,				0x001F, "Greek Polytonic" },
	{ KBD_GERMAN_NEO,				0x00C0, "German Neo" }
};

/* Input Method Editor (IME) */

struct _RDP_KEYBOARD_IME
{
	DWORD code; /* Keyboard layout code */
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

void freerdp_keyboard_layouts_free(RDP_KEYBOARD_LAYOUT* layouts)
{
    free(layouts);
}

RDP_KEYBOARD_LAYOUT* freerdp_keyboard_get_layouts(DWORD types)
{
	int num, length, i;
	RDP_KEYBOARD_LAYOUT* layouts;

	num = 0;
	layouts = (RDP_KEYBOARD_LAYOUT*) malloc((num + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_STANDARD) != 0)
	{
		length = ARRAYSIZE(RDP_KEYBOARD_LAYOUT_TABLE);
		layouts = (RDP_KEYBOARD_LAYOUT*) realloc(layouts, (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_LAYOUT_TABLE[i].code;
			layouts[num].name = _strdup(RDP_KEYBOARD_LAYOUT_TABLE[i].name);
		}
	}
	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_VARIANT) != 0)
	{
		length = ARRAYSIZE(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE);
		layouts = (RDP_KEYBOARD_LAYOUT*) realloc(layouts, (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].code;
			layouts[num].name = _strdup(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].name);
		}
	}
	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_IME) != 0)
	{
		length = ARRAYSIZE(RDP_KEYBOARD_IME_TABLE);
		layouts = (RDP_KEYBOARD_LAYOUT*) realloc(layouts, (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_IME_TABLE[i].code;
			layouts[num].name = _strdup(RDP_KEYBOARD_IME_TABLE[i].name);
		}
	}

	ZeroMemory(&layouts[num], sizeof(RDP_KEYBOARD_LAYOUT));

	return layouts;
}

const char* freerdp_keyboard_get_layout_name_from_id(DWORD keyboardLayoutID)
{
	int i;

	for (i = 0; i < ARRAYSIZE(RDP_KEYBOARD_LAYOUT_TABLE); i++)
	{
		if (RDP_KEYBOARD_LAYOUT_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_LAYOUT_TABLE[i].name;
	}

	for (i = 0; i < ARRAYSIZE(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE); i++)
	{
		if (RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].name;
	}

	for (i = 0; i < ARRAYSIZE(RDP_KEYBOARD_IME_TABLE); i++)
	{
		if (RDP_KEYBOARD_IME_TABLE[i].code == keyboardLayoutID)
			return RDP_KEYBOARD_IME_TABLE[i].name;
	}

	return "unknown";
}

DWORD freerdp_keyboard_get_layout_id_from_name(const char* name)
{
	int i;

	for (i = 0; i < ARRAYSIZE(RDP_KEYBOARD_LAYOUT_TABLE); i++)
	{
		if (strcmp(RDP_KEYBOARD_LAYOUT_TABLE[i].name, name) == 0)
			return RDP_KEYBOARD_LAYOUT_TABLE[i].code;
	}

	for (i = 0; i < ARRAYSIZE(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE); i++)
	{
		if (strcmp(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].name, name) == 0)
			return RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].code;
	}

	for (i = 0; i < ARRAYSIZE(RDP_KEYBOARD_IME_TABLE); i++)
	{
		if (strcmp(RDP_KEYBOARD_IME_TABLE[i].name, name) == 0)
			return RDP_KEYBOARD_IME_TABLE[i].code;
	}

	return 0;
}
