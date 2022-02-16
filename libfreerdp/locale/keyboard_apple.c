/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Apple Core Foundation Keyboard Mapping
 *
 * Copyright 2021 Thincast Technologies GmbH
 * Copyright 2021 Martin Fleisz <martin.fleisz@thincast.com>
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

#include <freerdp/config.h>

#include <Carbon/Carbon.h>
#include <string.h>

#include "liblocale.h"

#include <freerdp/locale/locale.h>
#include <freerdp/locale/keyboard.h>

#include "keyboard_apple.h"

struct KEYBOARD_LAYOUT_MAPPING_
{
	const char* inputSourceId; /* Apple input source id (com.apple.keylayout or inputmethod) */
	DWORD code;                /* mapped rdp keyboard layout id */
};
typedef struct KEYBOARD_LAYOUT_MAPPING_ KEYBOARD_LAYOUT_MAPPING;

static const KEYBOARD_LAYOUT_MAPPING KEYBOARD_MAPPING_TABLE[] = {
	{ "com.apple.inputmethod.Kotoeri.Japanese", JAPANESE },
	{ "com.apple.inputmethod.Kotoeri.Japanese.FullWidthRoman", JAPANESE },
	{ "com.apple.inputmethod.Kotoeri.Japanese.HalfWidthKana", JAPANESE },
	{ "com.apple.inputmethod.Kotoeri.Japanese.Katakana", JAPANESE },
	{ "com.apple.inputmethod.Kotoeri.Katakana", JAPANESE },
	{ "com.apple.inputmethod.Kotoeri.Roman", JAPANESE },
	{ "com.apple.inputmethod.kotoeri.Ainu", JAPANESE },
	{ "com.apple.keylayout.2SetHangul", KOREAN },
	{ "com.apple.keylayout.390Hangul", KOREAN },
	{ "com.apple.keylayout.3SetHangul", KOREAN },
	{ "com.apple.keylayout.AfghanDari", KBD_PERSIAN },
	{ "com.apple.keylayout.AfghanPashto", PASHTO },
	{ "com.apple.keylayout.AfghanUzbek", UZBEK_LATIN },
	{ "com.apple.keylayout.Arabic", ARABIC_EGYPT },
	{ "com.apple.keylayout.Arabic-QWERTY", ARABIC_EGYPT },
	{ "com.apple.keylayout.ArabicPC", ARABIC_EGYPT },
	{ "com.apple.keylayout.Armenian-HMQWERTY", ARMENIAN },
	{ "com.apple.keylayout.Armenian-WesternQWERTY", ARMENIAN },
	{ "com.apple.keylayout.Australian", ENGLISH_AUSTRALIAN },
	{ "com.apple.keylayout.Austrian", GERMAN_STANDARD },
	{ "com.apple.keylayout.Azeri", AZERI_LATIN },
	{ "com.apple.keylayout.Bangla", KBD_BANGLA },
	{ "com.apple.keylayout.Bangla-QWERTY", KBD_BANGLA },
	{ "com.apple.keylayout.Belgian", DUTCH_BELGIAN },
	{ "com.apple.keylayout.Brazilian", PORTUGUESE_BRAZILIAN },
	{ "com.apple.keylayout.British", ENGLISH_UNITED_KINGDOM },
	{ "com.apple.keylayout.British-PC", ENGLISH_UNITED_KINGDOM },
	{ "com.apple.keylayout.Bulgarian", BULGARIAN },
	{ "com.apple.keylayout.Bulgarian-Phonetic", KBD_BULGARIAN_PHONETIC },
	{ "com.apple.keylayout.Byelorussian", BELARUSIAN },
	{ "com.apple.keylayout.Canadian", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.Canadian-CSA", KBD_CANADIAN_MULTILINGUAL_STANDARD },
	{ "com.apple.keylayout.CangjieKeyboard", CHINESE_TAIWAN },
	{ "com.apple.keylayout.Cherokee-Nation", CHEROKEE },
	{ "com.apple.keylayout.Cherokee-QWERTY", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.Colemak", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.Croatian", CROATIAN },
	{ "com.apple.keylayout.Croatian-PC", CROATIAN },
	{ "com.apple.keylayout.Czech", CZECH },
	{ "com.apple.keylayout.Czech-QWERTY", KBD_CZECH_QWERTY },
	{ "com.apple.keylayout.DVORAK-QWERTYCMD", KBD_UNITED_STATES_DVORAK },
	{ "com.apple.keylayout.Danish", DANISH },
	{ "com.apple.keylayout.Devanagari", HINDI },
	{ "com.apple.keylayout.Devanagari-QWERTY", HINDI },
	{ "com.apple.keylayout.Dutch", KBD_UNITED_STATES_INTERNATIONAL },
	{ "com.apple.keylayout.Dvorak", KBD_UNITED_STATES_DVORAK },
	{ "com.apple.keylayout.Dvorak-Left", KBD_UNITED_STATES_DVORAK_FOR_LEFT_HAND },
	{ "com.apple.keylayout.Dvorak-Right", KBD_UNITED_STATES_DVORAK_FOR_RIGHT_HAND },
	{ "com.apple.keylayout.Estonian", ESTONIAN },
	{ "com.apple.keylayout.Faroese", FAEROESE },
	{ "com.apple.keylayout.Finnish", FINNISH },
	{ "com.apple.keylayout.FinnishExtended", KBD_SAMI_EXTENDED_FINLAND_SWEDEN },
	{ "com.apple.keylayout.FinnishSami-PC", KBD_FINNISH_WITH_SAMI },
	{ "com.apple.keylayout.French", KBD_BELGIAN_FRENCH },
	{ "com.apple.keylayout.French-PC", FRENCH_STANDARD },
	{ "com.apple.keylayout.French-numerical", KBD_BELGIAN_FRENCH },
	{ "com.apple.keylayout.GJCRomaja", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.Georgian-QWERTY", KBD_GEORGIAN_QUERTY },
	{ "com.apple.keylayout.German", GERMAN_STANDARD },
	{ "com.apple.keylayout.Greek", GREEK },
	{ "com.apple.keylayout.GreekPolytonic", KBD_GREEK_POLYTONIC },
	{ "com.apple.keylayout.Gujarati", GUJARATI },
	{ "com.apple.keylayout.Gujarati-QWERTY", GUJARATI },
	{ "com.apple.keylayout.Gurmukhi", PUNJABI },
	{ "com.apple.keylayout.Gurmukhi-QWERTY", PUNJABI },
	{ "com.apple.keylayout.HNCRomaja", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.Hawaiian", HAWAIIAN },
	{ "com.apple.keylayout.Hebrew", HEBREW },
	{ "com.apple.keylayout.Hebrew-PC", HEBREW },
	{ "com.apple.keylayout.Hebrew-QWERTY", HEBREW },
	{ "com.apple.keylayout.Hungarian", HUNGARIAN },
	{ "com.apple.keylayout.Hungarian-QWERTY", HUNGARIAN },
	{ "com.apple.keylayout.Icelandic", ICELANDIC },
	{ "com.apple.keylayout.Inuktitut-Nunavut", INUKTITUT },
	{ "com.apple.keylayout.Inuktitut-Nutaaq", INUKTITUT },
	{ "com.apple.keylayout.Inuktitut-QWERTY", INUKTITUT },
	{ "com.apple.keylayout.InuttitutNunavik", INUKTITUT },
	{ "com.apple.keylayout.Irish", ENGLISH_IRELAND },
	{ "com.apple.keylayout.IrishExtended", KBD_IRISH },
	{ "com.apple.keylayout.Italian", ITALIAN_STANDARD },
	{ "com.apple.keylayout.Italian-Pro", ITALIAN_STANDARD },
	{ "com.apple.keylayout.Jawi-QWERTY", ARABIC_SAUDI_ARABIA },
	{ "com.apple.keylayout.Kannada", KANNADA },
	{ "com.apple.keylayout.Kannada-QWERTY", KANNADA },
	{ "com.apple.keylayout.Kazakh", KAZAKH },
	{ "com.apple.keylayout.Khmer", KBD_KHMER },
	{ "com.apple.keylayout.Latvian", LATVIAN },
	{ "com.apple.keylayout.Lithuanian", LITHUANIAN },
	{ "com.apple.keylayout.Macedonian", MACEDONIAN },
	{ "com.apple.keylayout.Malayalam", MALAYALAM },
	{ "com.apple.keylayout.Malayalam-QWERTY", MALAYALAM },
	{ "com.apple.keylayout.Maltese", MALTESE },
	{ "com.apple.keylayout.Maori", MAORI },
	{ "com.apple.keylayout.Myanmar-QWERTY", MYANMAR },
	{ "com.apple.keylayout.Nepali", NEPALI },
	{ "com.apple.keylayout.NorthernSami", SAMI_NORTHERN_NORWAY },
	{ "com.apple.keylayout.Norwegian", NORWEGIAN_BOKMAL },
	{ "com.apple.keylayout.NorwegianExtended", NORWEGIAN_BOKMAL },
	{ "com.apple.keylayout.NorwegianSami-PC", NORWEGIAN_BOKMAL },
	{ "com.apple.keylayout.Oriya", ORIYA },
	{ "com.apple.keylayout.Persian", KBD_PERSIAN },
	{ "com.apple.keylayout.Persian-ISIRI2901", KBD_PERSIAN },
	{ "com.apple.keylayout.Polish", KBD_POLISH_214 },
	{ "com.apple.keylayout.PolishPro", KBD_UNITED_STATES_INTERNATIONAL },
	{ "com.apple.keylayout.Portuguese", PORTUGUESE_STANDARD },
	{ "com.apple.keylayout.Romanian", KBD_ROMANIAN },
	{ "com.apple.keylayout.Romanian-Standard", KBD_ROMANIAN_STANDARD },
	{ "com.apple.keylayout.Russian", RUSSIAN },
	{ "com.apple.keylayout.Russian-Phonetic", KBD_RUSSIAN_PHONETIC },
	{ "com.apple.keylayout.RussianWin", RUSSIAN },
	{ "com.apple.keylayout.Sami-PC", KBD_SAMI_EXTENDED_FINLAND_SWEDEN },
	{ "com.apple.keylayout.Serbian", KBD_SERBIAN_CYRILLIC },
	{ "com.apple.keylayout.Serbian-Latin", KBD_SERBIAN_LATIN },
	{ "com.apple.keylayout.Sinhala", SINHALA },
	{ "com.apple.keylayout.Sinhala-QWERTY", SINHALA },
	{ "com.apple.keylayout.Slovak", SLOVAK },
	{ "com.apple.keylayout.Slovak-QWERTY", KBD_SLOVAK_QWERTY },
	{ "com.apple.keylayout.Slovenian", SLOVENIAN },
	{ "com.apple.keylayout.Spanish", SPANISH_TRADITIONAL_SORT },
	{ "com.apple.keylayout.Spanish-ISO", KBD_SPANISH },
	{ "com.apple.keylayout.Swedish", SWEDISH },
	{ "com.apple.keylayout.Swedish-Pro", SWEDISH },
	{ "com.apple.keylayout.SwedishSami-PC", SWEDISH },
	{ "com.apple.keylayout.SwissFrench", FRENCH_SWISS },
	{ "com.apple.keylayout.SwissGerman", GERMAN_SWISS },
	{ "com.apple.keylayout.Telugu", TELUGU },
	{ "com.apple.keylayout.Telugu-QWERTY", TELUGU },
	{ "com.apple.keylayout.Thai", THAI },
	{ "com.apple.keylayout.Thai-PattaChote", KBD_THAI_PATTACHOTE },
	{ "com.apple.keylayout.Tibetan-QWERTY", TIBETAN_PRC },
	{ "com.apple.keylayout.Tibetan-Wylie", TIBETAN_PRC },
	{ "com.apple.keylayout.TibetanOtaniUS", TIBETAN_PRC },
	{ "com.apple.keylayout.Turkish", KBD_TURKISH_F },
	{ "com.apple.keylayout.Turkish-QWERTY", TURKISH },
	{ "com.apple.keylayout.Turkish-QWERTY-PC", TURKISH },
	{ "com.apple.keylayout.US", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.USExtended", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.USInternational-PC", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.Ukrainian", UKRAINIAN },
	{ "com.apple.keylayout.Ukrainian-PC", UKRAINIAN },
	{ "com.apple.keylayout.UnicodeHexInput", ENGLISH_UNITED_STATES },
	{ "com.apple.keylayout.Urdu", URDU },
	{ "com.apple.keylayout.Uyghur", UIGHUR },
	{ "com.apple.keylayout.Vietnamese", VIETNAMESE },
	{ "com.apple.keylayout.Welsh", WELSH }
};

int freerdp_detect_keyboard_layout_from_cf(DWORD* keyboardLayoutId)
{
	int i;
	CFIndex length;
	char* inputSourceId = NULL;
	CFStringRef inputSourceIdRef;
	TISInputSourceRef inputSrc = TISCopyCurrentKeyboardLayoutInputSource();
	if (!inputSrc)
	{
		DEBUG_KBD("Failed to get current keyboard layout input source!");
		return 0;
	}

	/* get current input source id */
	inputSourceIdRef = (CFStringRef)TISGetInputSourceProperty(inputSrc, kTISPropertyInputSourceID);
	if (!inputSourceIdRef)
	{
		DEBUG_KBD("Failed to get input source id!");
		goto done;
	}

	/* convert it to a C-string */
	length = CFStringGetLength(inputSourceIdRef);
	length = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
	inputSourceId = (char*)malloc(length);
	if (!inputSourceId)
	{
		DEBUG_KBD("Failed to allocate string buffer!");
		goto done;
	}

	if (!CFStringGetCString(inputSourceIdRef, inputSourceId, length, kCFStringEncodingUTF8))
	{
		DEBUG_KBD("Failed to convert CFString to C-string!");
		goto done;
	}

	/* Search for the id in the mapping table */
	for (i = 0; i < ARRAYSIZE(KEYBOARD_MAPPING_TABLE); ++i)
	{
		if (strcmp(inputSourceId, KEYBOARD_MAPPING_TABLE[i].inputSourceId) == 0)
		{
			*keyboardLayoutId = KEYBOARD_MAPPING_TABLE[i].code;
			break;
		}
	}

done:
	free(inputSourceId);
	CFRelease(inputSrc);
	if (*keyboardLayoutId > 0)
		return *keyboardLayoutId;

	return 0;
}
