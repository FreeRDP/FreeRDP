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

#ifndef FREERDP_LOCALE_KEYBOARD_H
#define FREERDP_LOCALE_KEYBOARD_H

#include <winpr/input.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/scancode.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_remap_table FREERDP_REMAP_TABLE;

	typedef enum
	{
		RDP_KEYBOARD_LAYOUT_TYPE_STANDARD = 1,
		RDP_KEYBOARD_LAYOUT_TYPE_VARIANT = 2,
		RDP_KEYBOARD_LAYOUT_TYPE_IME = 4
	} FREERDP_KEYBOARD_LAYOUT_TYPES;

	typedef struct
	{
		UINT16 id;
		UINT8 primaryId;
		UINT8 subId;
		char locale[512];
		char primaryLanguage[512];
		char primaryLanguageSymbol[512];
		char subLanguage[512];
		char subLanguageSymbol[512];
	} RDP_CODEPAGE;

typedef struct
{
	DWORD code; /* Keyboard layout code */
	char* name; /* Keyboard layout name */
} RDP_KEYBOARD_LAYOUT;

/* Keyboard layout IDs */
typedef enum
{
	KBD_ARABIC_101 = 0x00000401,
	KBD_BULGARIAN = 0x00000402,
	KBD_CHINESE_TRADITIONAL_US = 0x00000404,
	KBD_CZECH = 0x00000405,
	KBD_DANISH = 0x00000406,
	KBD_GERMAN = 0x00000407,
	KBD_GREEK = 0x00000408,
	KBD_US = 0x00000409,
	KBD_SPANISH = 0x0000040A,
	KBD_FINNISH = 0x0000040B,
	KBD_FRENCH = 0x0000040C,
	KBD_HEBREW = 0x0000040D,
	KBD_HUNGARIAN = 0x0000040E,
	KBD_ICELANDIC = 0x0000040F,
	KBD_ITALIAN = 0x00000410,
	KBD_JAPANESE = 0x00000411,
	KBD_KOREAN = 0x00000412,
	KBD_DUTCH = 0x00000413,
	KBD_NORWEGIAN = 0x00000414,
	KBD_POLISH_PROGRAMMERS = 0x00000415,
	KBD_PORTUGUESE_BRAZILIAN_ABNT = 0x00000416,
	KBD_ROMANIAN = 0x00000418,
	KBD_RUSSIAN = 0x00000419,
	KBD_CROATIAN = 0x0000041A,
	KBD_SLOVAK = 0x0000041B,
	KBD_ALBANIAN = 0x0000041C,
	KBD_SWEDISH = 0x0000041D,
	KBD_THAI_KEDMANEE = 0x0000041E,
	KBD_TURKISH_Q = 0x0000041F,
	KBD_URDU = 0x00000420,
	KBD_UKRAINIAN = 0x00000422,
	KBD_BELARUSIAN = 0x00000423,
	KBD_SLOVENIAN = 0x00000424,
	KBD_ESTONIAN = 0x00000425,
	KBD_LATVIAN = 0x00000426,
	KBD_LITHUANIAN_IBM = 0x00000427,
	KBD_FARSI = 0x00000429,
	KBD_VIETNAMESE = 0x0000042A,
	KBD_ARMENIAN_EASTERN = 0x0000042B,
	KBD_AZERI_LATIN = 0x0000042C,
	KBD_FYRO_MACEDONIAN = 0x0000042F,
	KBD_GEORGIAN = 0x00000437,
	KBD_FAEROESE = 0x00000438,
	KBD_DEVANAGARI_INSCRIPT = 0x00000439,
	KBD_MALTESE_47_KEY = 0x0000043A,
	KBD_NORWEGIAN_WITH_SAMI = 0x0000043B,
	KBD_KAZAKH = 0x0000043F,
	KBD_KYRGYZ_CYRILLIC = 0x00000440,
	KBD_TATAR = 0x00000444,
	KBD_BENGALI = 0x00000445,
	KBD_PUNJABI = 0x00000446,
	KBD_GUJARATI = 0x00000447,
	KBD_TAMIL = 0x00000449,
	KBD_TELUGU = 0x0000044A,
	KBD_KANNADA = 0x0000044B,
	KBD_MALAYALAM = 0x0000044C,
	KBD_MARATHI = 0x0000044E,
	KBD_MONGOLIAN_CYRILLIC = 0x00000450,
	KBD_UNITED_KINGDOM_EXTENDED = 0x00000452,
	KBD_SYRIAC = 0x0000045A,
	KBD_NEPALI = 0x00000461,
	KBD_PASHTO = 0x00000463,
	KBD_DIVEHI_PHONETIC = 0x00000465,
	KBD_LUXEMBOURGISH = 0x0000046E,
	KBD_MAORI = 0x00000481,
	KBD_CHINESE_SIMPLIFIED_US = 0x00000804,
	KBD_SWISS_GERMAN = 0x00000807,
	KBD_UNITED_KINGDOM = 0x00000809,
	KBD_LATIN_AMERICAN = 0x0000080A,
	KBD_BELGIAN_FRENCH = 0x0000080C,
	KBD_BELGIAN_PERIOD = 0x00000813,
	KBD_PORTUGUESE = 0x00000816,
	KBD_SERBIAN_LATIN = 0x0000081A,
	KBD_AZERI_CYRILLIC = 0x0000082C,
	KBD_SWEDISH_WITH_SAMI = 0x0000083B,
	KBD_UZBEK_CYRILLIC = 0x00000843,
	KBD_INUKTITUT_LATIN = 0x0000085D,
	KBD_CANADIAN_FRENCH_LEGACY = 0x00000C0C,
	KBD_SERBIAN_CYRILLIC = 0x00000C1A,
	KBD_CANADIAN_FRENCH = 0x00001009,
	KBD_SWISS_FRENCH = 0x0000100C,
	KBD_BOSNIAN = 0x0000141A,
	KBD_IRISH = 0x00001809,
	KBD_BOSNIAN_CYRILLIC = 0x0000201A
} FREERDP_KBD_LAYOUT_ID;

/* Keyboard layout variant IDs */
typedef enum
{
	KBD_ARABIC_102 = 0x00010401,
	KBD_BULGARIAN_LATIN = 0x00010402,
	KBD_CZECH_QWERTY = 0x00010405,
	KBD_GERMAN_IBM = 0x00010407,
	KBD_GREEK_220 = 0x00010408,
	KBD_UNITED_STATES_DVORAK = 0x00010409,
	KBD_SPANISH_VARIATION = 0x0001040A,
	KBD_HUNGARIAN_101_KEY = 0x0001040E,
	KBD_ITALIAN_142 = 0x00010410,
	KBD_POLISH_214 = 0x00010415,
	KBD_PORTUGUESE_BRAZILIAN_ABNT2 = 0x00010416,
	KBD_ROMANIAN_STANDARD = 0x00010418,
	KBD_RUSSIAN_TYPEWRITER = 0x00010419,
	KBD_SLOVAK_QWERTY = 0x0001041B,
	KBD_THAI_PATTACHOTE = 0x0001041E,
	KBD_TURKISH_F = 0x0001041F,
	KBD_LATVIAN_QWERTY = 0x00010426,
	KBD_LITHUANIAN = 0x00010427,
	KBD_ARMENIAN_WESTERN = 0x0001042B,
	KBD_GEORGIAN_QUERTY = 0x00010437,
	KBD_HINDI_TRADITIONAL = 0x00010439,
	KBD_MALTESE_48_KEY = 0x0001043A,
	KBD_SAMI_EXTENDED_NORWAY = 0x0001043B,
	KBD_BENGALI_INSCRIPT = 0x00010445,
	KBD_KHMER = 0x00010453,
	KBD_SYRIAC_PHONETIC = 0x0001045A,
	KBD_DIVEHI_TYPEWRITER = 0x00010465,
	KBD_BELGIAN_COMMA = 0x0001080C,
	KBD_FINNISH_WITH_SAMI = 0x0001083B,
	KBD_CANADIAN_MULTILINGUAL_STANDARD = 0x00011009,
	KBD_GAELIC = 0x00011809,
	KBD_ARABIC_102_AZERTY = 0x00020401,
	KBD_CZECH_PROGRAMMERS = 0x00020405,
	KBD_GREEK_319 = 0x00020408,
	KBD_UNITED_STATES_INTERNATIONAL = 0x00020409,
	KBD_HEBREW_STANDARD = 0x0002040D, /** @since version 3.6.0 */
	KBD_RUSSIAN_PHONETIC = 0x00020419,
	KBD_THAI_KEDMANEE_NON_SHIFTLOCK = 0x0002041E,
	KBD_BANGLA = 0x00020445,
	KBD_SAMI_EXTENDED_FINLAND_SWEDEN = 0x0002083B,
	KBD_GREEK_220_LATIN = 0x00030408,
	KBD_UNITED_STATES_DVORAK_FOR_LEFT_HAND = 0x00030409,
	KBD_THAI_PATTACHOTE_NON_SHIFTLOCK = 0x0003041E,
	KBD_BULGARIAN_PHONETIC = 0x00040402,
	KBD_GREEK_319_LATIN = 0x00040408,
	KBD_UNITED_STATES_DVORAK_FOR_RIGHT_HAND = 0x00040409,
	KBD_UNITED_STATES_DVORAK_PROGRAMMER = 0x19360409,
	KBD_GREEK_LATIN = 0x00050408,
	KBD_PERSIAN = 0x00050429,
	KBD_US_ENGLISH_TABLE_FOR_IBM_ARABIC_238_L = 0x00050409,
	KBD_GREEK_POLYTONIC = 0x00060408,
	KBD_FRENCH_BEPO = WINPR_CXX_COMPAT_CAST(int, 0xa000040c),
	KBD_GERMAN_NEO = WINPR_CXX_COMPAT_CAST(int, 0xB0000407)
} FREERDP_KBD_LAYPUT_VARIANT_ID;

/* Global Input Method Editor (IME) IDs */
typedef enum
{
	KBD_CHINESE_TRADITIONAL_PHONETIC = WINPR_CXX_COMPAT_CAST(int, 0xE0010404),
	KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002 = WINPR_CXX_COMPAT_CAST(int, 0xE0010411),
	KBD_KOREAN_INPUT_SYSTEM_IME_2000 = WINPR_CXX_COMPAT_CAST(int, 0xE0010412),
	KBD_CHINESE_SIMPLIFIED_QUANPIN = WINPR_CXX_COMPAT_CAST(int, 0xE0010804),
	KBD_CHINESE_TRADITIONAL_CHANGJIE = WINPR_CXX_COMPAT_CAST(int, 0xE0020404),
	KBD_CHINESE_SIMPLIFIED_SHUANGPIN = WINPR_CXX_COMPAT_CAST(int, 0xE0020804),
	KBD_CHINESE_TRADITIONAL_QUICK = WINPR_CXX_COMPAT_CAST(int, 0xE0030404),
	KBD_CHINESE_SIMPLIFIED_ZHENGMA = WINPR_CXX_COMPAT_CAST(int, 0xE0030804),
	KBD_CHINESE_TRADITIONAL_BIG5_CODE = WINPR_CXX_COMPAT_CAST(int, 0xE0040404),
	KBD_CHINESE_TRADITIONAL_ARRAY = WINPR_CXX_COMPAT_CAST(int, 0xE0050404),
	KBD_CHINESE_SIMPLIFIED_NEIMA = WINPR_CXX_COMPAT_CAST(int, 0xE0050804),
	KBD_CHINESE_TRADITIONAL_DAYI = WINPR_CXX_COMPAT_CAST(int, 0xE0060404),
	KBD_CHINESE_TRADITIONAL_UNICODE = WINPR_CXX_COMPAT_CAST(int, 0xE0070404),
	KBD_CHINESE_TRADITIONAL_NEW_PHONETIC = WINPR_CXX_COMPAT_CAST(int, 0xE0080404),
	KBD_CHINESE_TRADITIONAL_NEW_CHANGJIE = WINPR_CXX_COMPAT_CAST(int, 0xE0090404),
	KBD_CHINESE_TRADITIONAL_MICROSOFT_PINYIN_IME_3 = WINPR_CXX_COMPAT_CAST(int, 0xE00E0804),
	KBD_CHINESE_TRADITIONAL_ALPHANUMERIC = WINPR_CXX_COMPAT_CAST(int, 0xE00F0404)
} FREERDP_KBD_IME_ID;

/** @brief Deallocation function for a \b RDP_KEYBOARD_LAYOUT array of \b count size
 *
 * @param layouts An array allocated by \ref freerdp_keyboard_get_layouts
 * @param count The number of \b RDP_KEYBOARD_LAYOUT members
 *
 */
FREERDP_API void freerdp_keyboard_layouts_free(RDP_KEYBOARD_LAYOUT* layouts, size_t count);

/** @brief Return an allocated array of available keyboard layouts
 *
 * @param types The types of layout queried. A mask of \b RDP_KEYBOARD_LAYOUT_TYPES
 * @param count A pointer that will be set to the number of elements in the returned array
 *
 * @return An allocated array of keyboard layouts, free with \b freerdp_keyboard_layouts_free
 */
WINPR_ATTR_MALLOC(freerdp_keyboard_layouts_free, 1)
FREERDP_API RDP_KEYBOARD_LAYOUT* freerdp_keyboard_get_layouts(DWORD types, size_t* count);

/** @brief Get a string representation of a keyboard layout.
 *
 *  @param keyboardLayoutId The keyboard layout to get a string for
 *
 *  @return The string representation of the layout or the string \b "unknown" in case there is
 * none.
 */
FREERDP_API const char* freerdp_keyboard_get_layout_name_from_id(DWORD keyboardLayoutId);

/** @brief Get a keyboard layout id from a string
 *
 *  @param name The string to convert to a layout id. Must not be \b NULL
 *
 *  @return The keyboard layout id or \b 0 in case of no mapping
 */
FREERDP_API DWORD freerdp_keyboard_get_layout_id_from_name(const char* name);

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
WINPR_DEPRECATED_VAR("since 3.11.0, implement yourself in client",
	                 FREERDP_API DWORD freerdp_keyboard_init(DWORD keyboardLayoutId));

WINPR_DEPRECATED_VAR("since 3.11.0, implement yourself in client",
	                 FREERDP_API DWORD freerdp_keyboard_init_ex(DWORD keyboardLayoutId,
	                                                            const char* keyboardRemappingList));

WINPR_DEPRECATED_VAR("since 3.11.0, implement yourself in client",
	                 FREERDP_API DWORD
	                     freerdp_keyboard_get_rdp_scancode_from_x11_keycode(DWORD keycode));

WINPR_DEPRECATED_VAR("since 3.11.0, implement yourself in client",
	                 FREERDP_API DWORD freerdp_keyboard_get_x11_keycode_from_rdp_scancode(
	                     DWORD scancode, BOOL extended));
#endif

/** @brief deallocate a \b FREERDP_REMAP_TABLE
 *
 *  @param table The table to deallocate, may be \b NULL
 *
 *  @since version 3.11.0
 */
FREERDP_API void freerdp_keyboard_remap_free(FREERDP_REMAP_TABLE* table);

/** @brief parses a key=value string and creates a RDP scancode remapping table from it.
 *
 *  @param list A string containing the comma separated key=value pairs (e.g.
 * 'key=val,key2=val2,...')
 *
 *  @return An allocated array of values to remap. Must be deallocated by \ref
 * freerdp_keyboard_remap_free
 *
 *  @since version 3.11.0
 */
WINPR_ATTR_MALLOC(freerdp_keyboard_remap_free, 1)
FREERDP_API FREERDP_REMAP_TABLE* freerdp_keyboard_remap_string_to_list(const char* list);

/** @brief does remap a RDP scancode according to the remap table provided.
 *
 *  @param remap_table The remapping table to use
 *  @param rdpScanCode the RDP scancode to remap
 *
 *  @return The remapped RDP scancode (or the rdpScanCode if not remapped) or \b 0 in case of any
 * failures
 *
 *  @since version 3.11.0
 */
FREERDP_API DWORD freerdp_keyboard_remap_key(const FREERDP_REMAP_TABLE* remap_table,
	                                         DWORD rdpScanCode);

/** @brief deallocator for a \b RDP_CODEPAGE array allocated by \ref
 * freerdp_keyboard_get_matching_codepages
 *
 *  @param codepages The codepages to free, may be \b NULL
 */
FREERDP_API void freerdp_codepages_free(RDP_CODEPAGE* codepages);

/** @brief return an allocated array of matching codepages
 *
 *  @param column The column the filter is applied to:
 *    - 0 : RDP_CODEPAGE::locale
 *    - 1 : RDP_CODEPAGE::PrimaryLanguage
 *    - 2 : RDP_CODEPAGE::PrimaryLanguageSymbol
 *    - 3 : RDP_CODEPAGE::Sublanguage
 *    - 4 : RDP_CODEPAGE::SublanguageSymbol
 *  @param filter A filter pattern or \b NULL for no filtering. Simple string match, no expressions
 * supported (e.g. \b strstr match)
 *  @param count A pointer that will be set to the number of codepages found, may be \b NULL
 *
 *  @return An allocated array of \b RDP_CODEPAGE of size \b count or \b NULL if no match. Must be
 * freed by \ref freerdp_codepages_free
 */
WINPR_ATTR_MALLOC(freerdp_codepages_free, 1)
FREERDP_API RDP_CODEPAGE* freerdp_keyboard_get_matching_codepages(DWORD column, const char* filter,
	                                                              size_t* count);

/** @brief get a string representation of a RDP scancode
 *
 *  @param scancode The RDP scancode to get a string for
 *
 *  @return A string describing the RDP scancode or \b NULL if it does not exist
 */
FREERDP_API const char* freerdp_keyboard_scancode_name(DWORD scancode);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LOCALE_KEYBOARD_H */
