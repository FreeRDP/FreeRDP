/**
 * FreeRDP: A Remote Desktop Protocol Client
 * XKB-based Keyboard Mapping to Microsoft Keyboard System
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libkbd.h"

#include <freerdp/kbd/locales.h>

typedef struct
{
	/* Two or three letter language code */
	char language[4];

	/* Two or three letter country code (Sometimes with Cyrl_ prefix) */
	char country[10];

	/* 32-bit unsigned integer corresponding to the locale */
	unsigned int code;

} locale;

/*
 * Refer to MSDN article "Locale Identifier Constants and Strings":
 * http://msdn.microsoft.com/en-us/library/ms776260.aspx
 */

static const locale locales[] =
{
	{  "af", "ZA", AFRIKAANS }, /* Afrikaans (South Africa) */
	{  "sq", "AL", ALBANIAN }, /* Albanian (Albania) */
	{ "gsw", "FR", ALSATIAN }, /* Windows Vista and later: Alsatian (France) */
	{  "am", "ET", AMHARIC }, /* Windows Vista and later: Amharic (Ethiopia) */
	{  "ar", "DZ", ARABIC_ALGERIA }, /* Arabic (Algeria) */
	{  "ar", "BH", ARABIC_BAHRAIN }, /* Arabic (Bahrain) */
	{  "ar", "EG", ARABIC_EGYPT }, /* Arabic (Egypt) */
	{  "ar", "IQ", ARABIC_IRAQ }, /* Arabic (Iraq) */
	{  "ar", "JO", ARABIC_JORDAN }, /* Arabic (Jordan) */
	{  "ar", "KW", ARABIC_KUWAIT }, /* Arabic (Kuwait) */
	{  "ar", "LB", ARABIC_LEBANON }, /* Arabic (Lebanon) */
	{  "ar", "LY", ARABIC_LIBYA }, /* Arabic (Libya) */
	{  "ar", "MA", ARABIC_MOROCCO }, /* Arabic (Morocco) */
	{  "ar", "OM", ARABIC_OMAN }, /* Arabic (Oman) */
	{  "ar", "QA", ARABIC_QATAR }, /* Arabic (Qatar) */
	{  "ar", "SA", ARABIC_SAUDI_ARABIA }, /* Arabic (Saudi Arabia) */
	{  "ar", "SY", ARABIC_SYRIA }, /* Arabic (Syria) */
	{  "ar", "TN", ARABIC_TUNISIA }, /* Arabic (Tunisia) */
	{  "ar", "AE", ARABIC_UAE }, /* Arabic (U.A.E.) */
	{  "ar", "YE", ARABIC_YEMEN }, /* Arabic (Yemen) */
	{  "az", "AZ", AZERI_LATIN }, /* Azeri (Latin) */
	{  "az", "Cyrl_AZ", AZERI_CYRILLIC }, /* Azeri (Cyrillic) */
	{  "hy", "AM", ARMENIAN }, /* Windows 2000 and later: Armenian (Armenia) */
	{  "as", "IN", ASSAMESE }, /* Windows Vista and later: Assamese (India) */
	{  "ba", "RU", BASHKIR }, /* Windows Vista and later: Bashkir (Russia) */
	{  "eu", "ES", BASQUE }, /* Basque (Basque) */
	{  "be", "BY", BELARUSIAN }, /* Belarusian (Belarus) */
	{  "bn", "IN", BENGALI_INDIA }, /* Windows XP SP2 and later: Bengali (India) */
	{  "br", "FR", BRETON }, /* Breton (France) */
	{  "bs", "BA", BOSNIAN_LATIN }, /* Bosnian (Latin) */
	{  "bg", "BG", BULGARIAN }, /* Bulgarian (Bulgaria) */
	{  "ca", "ES", CATALAN }, /* Catalan (Catalan) */
	{  "zh", "HK", CHINESE_HONG_KONG }, /* Chinese (Hong Kong SAR, PRC) */
	{  "zh", "MO", CHINESE_MACAU }, /* Windows 98/Me, Windows XP and later: Chinese (Macao SAR) */
	{  "zh", "CN", CHINESE_PRC }, /* Chinese (PRC) */
	{  "zh", "SG", CHINESE_SINGAPORE }, /* Chinese (Singapore) */
	{  "zh", "TW", CHINESE_TAIWAN }, /* Chinese (Taiwan) */
	{  "hr", "BA", CROATIAN_BOSNIA_HERZEGOVINA }, /* Windows XP SP2 and later: Croatian (Bosnia and Herzegovina, Latin) */
	{  "hr", "HR", CROATIAN }, /* Croatian (Croatia) */
	{  "cs", "CZ", CZECH }, /* Czech (Czech Republic) */
	{  "da", "DK", DANISH }, /* Danish (Denmark) */
	{ "prs", "AF", DARI }, /* Windows XP and later: Dari (Afghanistan) */
	{  "dv", "MV", DIVEHI }, /* Windows XP and later: Divehi (Maldives) */
	{  "nl", "BE", DUTCH_BELGIAN }, /* Dutch (Belgium) */
	{  "nl", "NL", DUTCH_STANDARD }, /* Dutch (Netherlands) */
	{  "en", "AU", ENGLISH_AUSTRALIAN }, /* English (Australia) */
	{  "en", "BZ", ENGLISH_BELIZE }, /* English (Belize) */
	{  "en", "CA", ENGLISH_CANADIAN }, /* English (Canada) */
	{  "en", "CB", ENGLISH_CARIBBEAN }, /* English (Carribean) */
	{  "en", "IN", ENGLISH_INDIA }, /* Windows Vista and later: English (India) */
	{  "en", "IE", ENGLISH_IRELAND }, /* English (Ireland) */
	{  "en", "JM", ENGLISH_JAMAICA }, /* English (Jamaica) */
	{  "en", "MY", ENGLISH_MALAYSIA }, /* Windows Vista and later: English (Malaysia) */
	{  "en", "NZ", ENGLISH_NEW_ZEALAND }, /* English (New Zealand) */
	{  "en", "PH", ENGLISH_PHILIPPINES }, /* Windows 98/Me, Windows 2000 and later: English (Philippines) */
	{  "en", "SG", ENGLISH_SINGAPORE }, /* Windows Vista and later: English (Singapore) */
	{  "en", "ZA", ENGLISH_SOUTH_AFRICA }, /* English (South Africa) */
	{  "en", "TT", ENGLISH_TRINIDAD }, /* English (Trinidad and Tobago) */
	{  "en", "GB", ENGLISH_UNITED_KINGDOM }, /* English (United Kingdom) */
	{  "en", "US", ENGLISH_UNITED_STATES }, /* English (United States) */
	{  "en", "ZW", ENGLISH_ZIMBABWE }, /* Windows 98/Me, Windows 2000 and later: English (Zimbabwe) */
	{  "et", "EE", ESTONIAN }, /* Estonian (Estonia) */
	{  "fo", "FO", FAEROESE }, /* Faroese (Faroe Islands) */
	{ "fil", "PH", FILIPINO }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Filipino (Philippines) */
	{  "fi", "FI", FINNISH }, /* Finnish (Finland) */
	{  "fr", "BE", FRENCH_BELGIAN }, /* French (Belgium) */
	{  "fr", "CA", FRENCH_CANADIAN }, /* French (Canada) */
	{  "fr", "FR", FRENCH_STANDARD }, /* French (France) */
	{  "fr", "LU", FRENCH_LUXEMBOURG }, /* French (Luxembourg) */
	{  "fr", "MC", FRENCH_MONACO }, /* French (Monaco) */
	{  "fr", "CH", FRENCH_SWISS }, /* French (Switzerland) */
	{  "fy", "NL", FRISIAN }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Frisian (Netherlands) */
	{  "gl", "ES", GALICIAN }, /* Windows XP and later: Galician (Spain) */
	{  "ka", "GE", GEORGIAN }, /* Windows 2000 and later: Georgian (Georgia) */
	{  "de", "AT", GERMAN_AUSTRIAN }, /* German (Austria) */
	{  "de", "DE", GERMAN_STANDARD }, /* German (Germany) */
	{  "de", "LI", GERMAN_LIECHTENSTEIN }, /* German (Liechtenstein) */
	{  "de", "LU", GERMAN_LUXEMBOURG }, /* German (Luxembourg) */
	{  "de", "CH", GERMAN_SWISS }, /* German (Switzerland) */
	{  "el", "GR", GREEK }, /* Greek (Greece) */
	{  "kl", "GL", GREENLANDIC }, /* Windows Vista and later: Greenlandic (Greenland) */
	{  "gu", "IN", GUJARATI }, /* Windows XP and later: Gujarati (India) */
	{  "he", "IL", HEBREW }, /* Hebrew (Israel) */
	{  "hi", "IN", HINDI }, /* Windows 2000 and later: Hindi (India) */
	{  "hu", "HU", HUNGARIAN }, /* Hungarian (Hungary) */
	{  "is", "IS", ICELANDIC }, /* Icelandic (Iceland) */
	{  "ig", "NG", IGBO }, /* Igbo (Nigeria) */
	{  "id", "ID", INDONESIAN }, /* Indonesian (Indonesia) */
	{  "ga", "IE", IRISH }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Irish (Ireland) */
	{  "it", "IT", ITALIAN_STANDARD }, /* Italian (Italy) */
	{  "it", "CH", ITALIAN_SWISS }, /* Italian (Switzerland) */
	{  "ja", "JP", JAPANESE }, /* Japanese (Japan) */
	{  "kn", "IN", KANNADA }, /* Windows XP and later: Kannada (India) */
	{  "kk", "KZ", KAZAKH }, /* Windows 2000 and later: Kazakh (Kazakhstan) */
	{  "kh", "KH", KHMER }, /* Windows Vista and later: Khmer (Cambodia) */
	{ "qut", "GT", KICHE }, /* Windows Vista and later: K'iche (Guatemala) */
	{  "rw", "RW", KINYARWANDA }, /* Windows Vista and later: Kinyarwanda (Rwanda) */
	{ "kok", "IN", KONKANI }, /* Windows 2000 and later: Konkani (India) */
	{  "ko", "KR", KOREAN }, /* Korean (Korea) */
	{  "ky", "KG", KYRGYZ }, /* Windows XP and later: Kyrgyz (Kyrgyzstan) */
	{  "lo", "LA", LAO }, /* Windows Vista and later: Lao (Lao PDR) */
	{  "lv", "LV", LATVIAN }, /* Latvian (Latvia) */
	{  "lt", "LT", LITHUANIAN }, /* Lithuanian (Lithuania) */
	{ "dsb", "DE", LOWER_SORBIAN }, /* Windows Vista and later: Lower Sorbian (Germany) */
	{  "lb", "LU", LUXEMBOURGISH }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Luxembourgish (Luxembourg) */
	{  "mk", "MK", MACEDONIAN }, /* Windows 2000 and later: Macedonian (Macedonia, FYROM) */
	{  "ms", "BN", MALAY_BRUNEI_DARUSSALAM }, /* Windows 2000 and later: Malay (Brunei Darussalam) */
	{  "ms", "MY", MALAY_MALAYSIA }, /* Windows 2000 and later: Malay (Malaysia) */
	{  "ml", "IN", MALAYALAM }, /* Windows XP SP2 and later: Malayalam (India) */
	{  "mt", "MT", MALTESE }, /* Windows XP SP2 and later: Maltese (Malta) */
	{  "mi", "NZ", MAORI }, /* Windows XP SP2 and later: Maori (New Zealand) */
	{ "arn", "CL", MAPUDUNGUN }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Mapudungun (Chile) */
	{  "mr", "IN", MARATHI }, /* Windows 2000 and later: Marathi (India) */
	{ "moh", "CA", MOHAWK }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Mohawk (Canada) */
	{  "mn", "MN", MONGOLIAN }, /* Mongolian */
	{  "ne", "NP", NEPALI }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Nepali (Nepal) */
	{  "nb", "NO", NORWEGIAN_BOKMAL }, /* Norwegian (Bokmal, Norway) */
	{  "nn", "NO", NORWEGIAN_NYNORSK }, /* Norwegian (Nynorsk, Norway) */
	{  "oc", "FR", OCCITAN }, /* Occitan (France) */
	{  "or", "IN", ORIYA }, /* Oriya (India) */
	{  "ps", "AF", PASHTO }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Pashto (Afghanistan) */
	{  "fa", "IR", FARSI }, /* Persian (Iran) */
	{  "pl", "PL", POLISH }, /* Polish (Poland) */
	{  "pt", "BR", PORTUGUESE_BRAZILIAN }, /* Portuguese (Brazil) */
	{  "pt", "PT", PORTUGUESE_STANDARD }, /* Portuguese (Portugal) */
	{  "pa", "IN", PUNJABI }, /* Windows XP and later: Punjabi (India) */
	{ "quz", "BO", QUECHUA_BOLIVIA }, /* Windows XP SP2 and later: Quechua (Bolivia) */
	{ "quz", "EC", QUECHUA_ECUADOR }, /* Windows XP SP2 and later: Quechua (Ecuador) */
	{ "quz", "PE", QUECHUA_PERU }, /* Windows XP SP2 and later: Quechua (Peru) */
	{  "ro", "RO", ROMANIAN }, /* Romanian (Romania) */
	{  "rm", "CH", ROMANSH }, /* Windows XP SP2 and later (downloadable); Windows Vista and later: Romansh (Switzerland) */
	{  "ru", "RU", RUSSIAN }, /* Russian (Russia) */
	{ "smn", "FI", SAMI_INARI }, /* Windows XP SP2 and later: Sami (Inari, Finland) */
	{ "smj", "NO", SAMI_LULE_NORWAY }, /* Windows XP SP2 and later: Sami (Lule, Norway) */
	{ "smj", "SE", SAMI_LULE_SWEDEN }, /* Windows XP SP2 and later: Sami (Lule, Sweden) */
	{  "se", "FI", SAMI_NORTHERN_FINLAND }, /* Windows XP SP2 and later: Sami (Northern, Finland) */
	{  "se", "NO", SAMI_NORTHERN_NORWAY }, /* Windows XP SP2 and later: Sami (Northern, Norway) */
	{  "se", "SE", SAMI_NORTHERN_SWEDEN }, /* Windows XP SP2 and later: Sami (Northern, Sweden) */
	{ "sms", "FI", SAMI_SKOLT }, /* Windows XP SP2 and later: Sami (Skolt, Finland) */
	{ "sma", "NO", SAMI_SOUTHERN_NORWAY }, /* Windows XP SP2 and later: Sami (Southern, Norway) */
	{ "sma", "SE", SAMI_SOUTHERN_SWEDEN }, /* Windows XP SP2 and later: Sami (Southern, Sweden) */
	{  "sa", "IN", SANSKRIT }, /* Windows 2000 and later: Sanskrit (India) */
	{  "sr", "SP", SERBIAN_LATIN }, /* Serbian (Latin) */
	{  "sr", "SIH", SERBIAN_LATIN_BOSNIA_HERZEGOVINA }, /* Serbian (Latin) (Bosnia and Herzegovina) */
	{  "sr", "Cyrl_SP", SERBIAN_CYRILLIC }, /* Serbian (Cyrillic) */
	{  "sr", "Cyrl_SIH", SERBIAN_CYRILLIC_BOSNIA_HERZEGOVINA }, /* Serbian (Cyrillic) (Bosnia and Herzegovina) */
	{  "ns", "ZA", SESOTHO_SA_LEBOA }, /* Windows XP SP2 and later: Sesotho sa Leboa/Northern Sotho (South Africa) */
	{  "tn", "ZA", TSWANA }, /* Windows XP SP2 and later: Setswana/Tswana (South Africa) */
	{  "si", "LK", SINHALA }, /* Windows Vista and later: Sinhala (Sri Lanka) */
	{  "sk", "SK", SLOVAK }, /* Slovak (Slovakia) */
	{  "sl", "SI", SLOVENIAN }, /* Slovenian (Slovenia) */
	{  "es", "AR", SPANISH_ARGENTINA }, /* Spanish (Argentina) */
	{  "es", "BO", SPANISH_BOLIVIA }, /* Spanish (Bolivia) */
	{  "es", "CL", SPANISH_CHILE }, /* Spanish (Chile) */
	{  "es", "CO", SPANISH_COLOMBIA }, /* Spanish (Colombia) */
	{  "es", "CR", SPANISH_COSTA_RICA }, /* Spanish (Costa Rica) */
	{  "es", "DO", SPANISH_DOMINICAN_REPUBLIC }, /* Spanish (Dominican Republic) */
	{  "es", "EC", SPANISH_ECUADOR }, /* Spanish (Ecuador) */
	{  "es", "SV", SPANISH_EL_SALVADOR }, /* Spanish (El Salvador) */
	{  "es", "GT", SPANISH_GUATEMALA }, /* Spanish (Guatemala) */
	{  "es", "HN", SPANISH_HONDURAS }, /* Spanish (Honduras) */
	{  "es", "MX", SPANISH_MEXICAN }, /* Spanish (Mexico) */
	{  "es", "NI", SPANISH_NICARAGUA }, /* Spanish (Nicaragua) */
	{  "es", "PA", SPANISH_PANAMA }, /* Spanish (Panama) */
	{  "es", "PY", SPANISH_PARAGUAY }, /* Spanish (Paraguay) */
	{  "es", "PE", SPANISH_PERU }, /* Spanish (Peru) */
	{  "es", "PR", SPANISH_PUERTO_RICO }, /* Spanish (Puerto Rico) */
	{  "es", "ES", SPANISH_MODERN_SORT }, /* Spanish (Spain) */
	{  "es", "ES", SPANISH_TRADITIONAL_SORT }, /* Spanish (Spain, Traditional Sort) */
	{  "es", "US", SPANISH_UNITED_STATES }, /* Windows Vista and later: Spanish (United States) */
	{  "es", "UY", SPANISH_URUGUAY }, /* Spanish (Uruguay) */
	{  "es", "VE", SPANISH_VENEZUELA }, /* Spanish (Venezuela) */
	{  "sw", "KE", SWAHILI }, /* Windows 2000 and later: Swahili (Kenya) */
	{  "sv", "FI", SWEDISH_FINLAND }, /* Swedish (Finland) */
	{  "sv", "SE", SWEDISH }, /* Swedish (Sweden) */
	{ "syr", "SY", SYRIAC }, /* Windows XP and later: Syriac (Syria) */
	{  "ta", "IN", TAMIL }, /* Windows 2000 and later: Tamil (India) */
	{  "tt", "RU", TATAR }, /* Windows XP and later: Tatar (Russia) */
	{  "te", "IN", TELUGU }, /* Windows XP and later: Telugu (India) */
	{  "th", "TH", THAI }, /* Thai (Thailand) */
	{  "bo", "BT", TIBETAN_BHUTAN }, /* Windows Vista and later: Tibetan (Bhutan) */
	{  "bo", "CN", TIBETAN_PRC }, /* Windows Vista and later: Tibetan (PRC) */
	{  "tr", "TR", TURKISH }, /* Turkish (Turkey) */
	{  "tk", "TM", TURKMEN }, /* Windows Vista and later: Turkmen (Turkmenistan) */
	{  "ug", "CN", UIGHUR }, /* Windows Vista and later: Uighur (PRC) */
	{  "uk", "UA", UKRAINIAN }, /* Ukrainian (Ukraine) */
	{ "wen", "DE", UPPER_SORBIAN }, /* Windows Vista and later: Upper Sorbian (Germany) */
	{  "tr", "IN", URDU_INDIA }, /* Urdu (India) */
	{  "ur", "PK", URDU }, /* Windows 98/Me, Windows 2000 and later: Urdu (Pakistan) */
	{  "uz", "UZ", UZBEK_LATIN }, /* Uzbek (Latin) */
	{  "uz", "Cyrl_UZ", UZBEK_CYRILLIC }, /* Uzbek (Cyrillic) */
	{  "vi", "VN", VIETNAMESE }, /* Windows 98/Me, Windows NT 4.0 and later: Vietnamese (Vietnam) */
	{  "cy", "GB", WELSH }, /* Windows XP SP2 and later: Welsh (United Kingdom) */
	{  "wo", "SN", WOLOF }, /* Windows Vista and later: Wolof (Senegal) */
	{  "xh", "ZA", XHOSA }, /* Windows XP SP2 and later: Xhosa/isiXhosa (South Africa) */
	{ "sah", "RU", YAKUT }, /* Windows Vista and later: Yakut (Russia) */
	{  "ii", "CN", YI }, /* Windows Vista and later: Yi (PRC) */
	{  "yo", "NG", YORUBA }, /* Windows Vista and later: Yoruba (Nigeria) */
	{  "zu", "ZA", ZULU } /* Windows XP SP2 and later: Zulu/isiZulu (South Africa) */
};


typedef struct
{
	/* Locale ID */
	unsigned int locale;

	/* Array of associated keyboard layouts */
	unsigned int keyboardLayouts[5];

} localeAndKeyboardLayout;

/* TODO: Use KBD_* defines instead of hardcoded values */

static const localeAndKeyboardLayout defaultKeyboardLayouts[] =
{
	{ AFRIKAANS,				{ 0x00000409, 0x00000409, 0x0, 0x0, 0x0 } },
	{ ALBANIAN,				{ 0x0000041c, 0x00000409, 0x0, 0x0, 0x0 } },
	{ ARABIC_SAUDI_ARABIA,			{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_IRAQ,				{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_EGYPT,				{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_LIBYA,				{ 0x0000040c, 0x00020401, 0x0, 0x0, 0x0 } },
	{ ARABIC_ALGERIA,			{ 0x0000040c, 0x00020401, 0x0, 0x0, 0x0 } },
	{ ARABIC_MOROCCO,			{ 0x0000040c, 0x00020401, 0x0, 0x0, 0x0 } },
	{ ARABIC_TUNISIA,			{ 0x0000040c, 0x00020401, 0x0, 0x0, 0x0 } },
	{ ARABIC_OMAN,				{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_YEMEN,				{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_SYRIA,				{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_JORDAN,			{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_LEBANON,			{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_KUWAIT,			{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_UAE,				{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_BAHRAIN,			{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARABIC_QATAR,				{ 0x00000409, 0x00000401, 0x0, 0x0, 0x0 } },
	{ ARMENIAN,				{ 0x0000042b, 0x00000409, 0x00000419, 0x0, 0x0 } },
	{ AZERI_LATIN,				{ 0x0000042c, 0x0000082c, 0x00000419, 0x0, 0x0 } },
	{ AZERI_CYRILLIC,			{ 0x0000082c, 0x0000042c, 0x00000419, 0x0, 0x0 } },
	{ BASQUE,				{ 0x0000040a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ BELARUSIAN,				{ 0x00000423, 0x00000409, 0x00000419, 0x0, 0x0 } },
	{ BENGALI_INDIA,			{ 0x00000445, 0x00000409, 0x0, 0x0, 0x0 } },
	{ BOSNIAN_LATIN,			{ 0x0000141A, 0x00000409, 0x0, 0x0, 0x0 } },
	{ BULGARIAN,				{ 0x00000402, 0x00000409, 0x0, 0x0, 0x0 } },
	{ CATALAN,				{ 0x0000040a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ CHINESE_TAIWAN,			{ 0x00000404, 0xe0080404, 0xE0010404, 0x0, 0x0 } },
	{ CHINESE_PRC,				{ 0x00000804, 0xe00e0804, 0xe0010804, 0xe0030804, 0xe0040804 } },
	{ CHINESE_HONG_KONG,			{ 0x00000409, 0xe0080404, 0x0, 0x0, 0x0 } },
	{ CHINESE_SINGAPORE,			{ 0x00000409, 0xe00e0804, 0xe0010804, 0xe0030804, 0xe0040804 } },
	{ CHINESE_MACAU,			{ 0x00000409, 0xe00e0804, 0xe0020404, 0xe0080404 } },
	{ CROATIAN,				{ 0x0000041a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ CROATIAN_BOSNIA_HERZEGOVINA,		{ 0x0000041a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ CZECH,				{ 0x00000405, 0x00000409, 0x0, 0x0, 0x0 } },
	{ DANISH,				{ 0x00000406, 0x00000409, 0x0, 0x0, 0x0 } },
	{ DIVEHI,				{ 0x00000409, 0x00000465, 0x0, 0x0, 0x0 } },
	{ DUTCH_STANDARD,			{ 0x00020409, 0x00000413, 0x00000409, 0x0, 0x0 } },
	{ DUTCH_BELGIAN,			{ 0x00000813, 0x00000409, 0x0, 0x0, 0x0 } },
	{ ENGLISH_UNITED_STATES,		{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_UNITED_KINGDOM,		{ 0x00000809, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_AUSTRALIAN,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_CANADIAN,			{ 0x00000409, 0x00011009, 0x00001009, 0x0, 0x0 } },
	{ ENGLISH_NEW_ZEALAND,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_IRELAND,			{ 0x00001809, 0x00011809, 0x0, 0x0, 0x0 } },
	{ ENGLISH_SOUTH_AFRICA,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_JAMAICA,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_CARIBBEAN,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_BELIZE,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_TRINIDAD,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_ZIMBABWE,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ENGLISH_PHILIPPINES,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ ESTONIAN,				{ 0x00000425, 0x0, 0x0, 0x0, 0x0 } },
	{ FAEROESE,				{ 0x00000406, 0x00000409, 0x0, 0x0, 0x0 } },
	{ FARSI,				{ 0x00000409, 0x00000429, 0x00000401, 0x0, 0x0 } },
	{ FINNISH,				{ 0x0000040b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ FRENCH_STANDARD,			{ 0x0000040c, 0x00000409, 0x0, 0x0, 0x0 } },
	{ FRENCH_BELGIAN,			{ 0x0000080c, 0x00000409, 0x0, 0x0, 0x0 } },
	{ FRENCH_CANADIAN,			{ 0x00000C0C, 0x00011009, 0x00000409, 0x0, 0x0 } },
	{ FRENCH_SWISS,				{ 0x0000100c, 0x00000409, 0x0, 0x0, 0x0 } },
	{ FRENCH_LUXEMBOURG,			{ 0x0000040c, 0x00000409, 0x0, 0x0, 0x0 } },
	{ FRENCH_MONACO,			{ 0x0000040c, 0x00000409, 0x0, 0x0, 0x0 } },
	{ GEORGIAN,				{ 0x00000437, 0x00000409, 0x00000419, 0x0, 0x0 } },
	{ GALICIAN,				{ 0x0000040a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ GERMAN_STANDARD,			{ 0x00000407, 0x00000409, 0x0, 0x0, 0x0 } },
	{ GERMAN_SWISS,				{ 0x00000807, 0x00000409, 0x0, 0x0, 0x0 } },
	{ GERMAN_AUSTRIAN,			{ 0x00000407, 0x00000409, 0x0, 0x0, 0x0 } },
	{ GERMAN_LUXEMBOURG,			{ 0x00000407, 0x00000409, 0x0, 0x0, 0x0 } },
	{ GERMAN_LIECHTENSTEIN,			{ 0x00000407, 0x00000409, 0x0, 0x0, 0x0 } },
	{ GREEK,				{ 0x00000408, 0x00000409, 0x0, 0x0, 0x0 } },
	{ GUJARATI,				{ 0x00000409, 0x00000447, 0x00010439, 0x0, 0x0 } },
	{ HEBREW,				{ 0x00000409, 0x0000040d, 0x0, 0x0, 0x0 } },
	{ HINDI,				{ 0x00000409, 0x00010439, 0x00000439, 0x0, 0x0 } },
	{ HUNGARIAN,				{ 0x0000040e, 0x00000409, 0x0, 0x0, 0x0 } },
	{ ICELANDIC,				{ 0x0000040f, 0x00000409, 0x0, 0x0, 0x0 } },
	{ INDONESIAN,				{ 0x00000409, 0x00000409, 0x0, 0x0, 0x0 } },
	{ ITALIAN_STANDARD,			{ 0x00000410, 0x00000409, 0x0, 0x0, 0x0 } },
	{ ITALIAN_SWISS,			{ 0x00000410, 0x00000409, 0x0, 0x0, 0x0 } },
	{ JAPANESE,				{ 0xe0010411, 0x0, 0x0, 0x0, 0x0 } },
	{ KANNADA,				{ 0x00000409, 0x0000044b, 0x00010439, 0x0, 0x0 } },
	{ KAZAKH,				{ 0x0000043f, 0x00000409, 0x00000419, 0x0, 0x0 } },
	{ KONKANI,				{ 0x00000409, 0x00000439, 0x0, 0x0, 0x0 } },
	{ KOREAN,				{ 0xE0010412, 0x0, 0x0, 0x0, 0x0 } },
	{ KYRGYZ,				{ 0x00000440, 0x00000409, 0x0, 0x0, 0x0 } },
	{ LATVIAN,				{ 0x00010426, 0x0, 0x0, 0x0, 0x0 } },
	{ LITHUANIAN,				{ 0x00010427, 0x0, 0x0, 0x0, 0x0 } },
	{ MACEDONIAN,				{ 0x0000042f, 0x00000409, 0x0, 0x0, 0x0 } },
	{ MALAY_MALAYSIA,			{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ MALAY_BRUNEI_DARUSSALAM,		{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ MALAYALAM,				{ 0x00000409, 0x0000044c, 0x0, 0x0, 0x0 } },
	{ MALTESE,				{ 0x00000409, 0x0000043a, 0x0, 0x0, 0x0 } },
	{ MAORI,				{ 0x00000409, 0x00000481, 0x0, 0x0, 0x0 } },
	{ MARATHI,				{ 0x00000409, 0x0000044e, 0x00000439, 0x0, 0x0 } },
	{ MONGOLIAN,				{ 0x00000450, 0x00000409, 0x0, 0x0, 0x0 } },
	{ NORWEGIAN_BOKMAL,			{ 0x00000414, 0x00000409, 0x0, 0x0, 0x0 } },
	{ NORWEGIAN_NYNORSK,			{ 0x00000414, 0x00000409, 0x0, 0x0, 0x0 } },
	{ POLISH,				{ 0x00010415, 0x00000415, 0x00000409, 0x0, 0x0 } },
	{ PORTUGUESE_BRAZILIAN,			{ 0x00000416, 0x00000409, 0x0, 0x0, 0x0 } },
	{ PORTUGUESE_STANDARD,			{ 0x00000816, 0x00000409, 0x0, 0x0, 0x0 } },
	{ PUNJABI,				{ 0x00000409, 0x00000446, 0x00010439, 0x0, 0x0 } },
	{ QUECHUA_BOLIVIA,			{ 0x00000409, 0x0000080A, 0x0, 0x0, 0x0 } },
	{ QUECHUA_ECUADOR,			{ 0x00000409, 0x0000080A, 0x0, 0x0, 0x0 } },
	{ QUECHUA_PERU,				{ 0x00000409, 0x0000080A, 0x0, 0x0, 0x0 } },
	{ ROMANIAN,				{ 0x00000418, 0x00000409, 0x0, 0x0, 0x0 } },
	{ RUSSIAN,				{ 0x00000419, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_INARI,				{ 0x0001083b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_LULE_NORWAY,			{ 0x0000043b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_LULE_SWEDEN,			{ 0x0000083b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_NORTHERN_FINLAND,		{ 0x0001083b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_NORTHERN_NORWAY,			{ 0x0000043b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_NORTHERN_SWEDEN,			{ 0x0000083b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_SKOLT,				{ 0x0001083b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_SOUTHERN_NORWAY,			{ 0x0000043b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SAMI_SOUTHERN_SWEDEN,			{ 0x0000083b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SANSKRIT,				{ 0x00000409, 0x00000439, 0x0, 0x0, 0x0 } },
	{ SERBIAN_LATIN,			{ 0x0000081a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SERBIAN_LATIN_BOSNIA_HERZEGOVINA,	{ 0x0000081a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SERBIAN_CYRILLIC,			{ 0x00000c1a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SERBIAN_CYRILLIC_BOSNIA_HERZEGOVINA,	{ 0x00000c1a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SLOVAK,				{ 0x0000041b, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SLOVENIAN,				{ 0x00000424, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_TRADITIONAL_SORT,		{ 0x0000040a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_MEXICAN,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_MODERN_SORT,			{ 0x0000040a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_GUATEMALA,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_COSTA_RICA,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_PANAMA,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_DOMINICAN_REPUBLIC,		{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_VENEZUELA,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_COLOMBIA,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_PERU,				{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_ARGENTINA,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_ECUADOR,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_CHILE,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_URUGUAY,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_PARAGUAY,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_BOLIVIA,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_EL_SALVADOR,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_HONDURAS,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_NICARAGUA,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SPANISH_PUERTO_RICO,			{ 0x0000080a, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SWAHILI,				{ 0x00000409, 0x0, 0x0, 0x0, 0x0 } },
	{ SWEDISH,				{ 0x0000041d, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SWEDISH_FINLAND,			{ 0x0000041d, 0x00000409, 0x0, 0x0, 0x0 } },
	{ SYRIAC,				{ 0x00000409, 0x0000045a, 0x0, 0x0, 0x0 } },
	{ TAMIL,				{ 0x00000409, 0x00000449, 0x0, 0x0, 0x0 } },
	{ TATAR,				{ 0x00000444, 0x00000409, 0x00000419, 0x0, 0x0 } },
	{ TELUGU,				{ 0x00000409, 0x0000044a, 0x00010439, 0x0, 0x0 } },
	{ THAI,					{ 0x00000409, 0x0000041e, 0x0, 0x0, 0x0 } },
	{ TSWANA,				{ 0x00000409, 0x0000041f, 0x0, 0x0, 0x0 } },
	{ UKRAINIAN,				{ 0x00000422, 0x00000409, 0x0, 0x0, 0x0 } },
	{ TURKISH,				{ 0x0000041f, 0x0000041f, 0x0, 0x0, 0x0 } },
	{ UKRAINIAN,				{ 0x00000422, 0x00000409, 0x0, 0x0, 0x0 } },
	{ URDU,					{ 0x00000401, 0x00000409, 0x0, 0x0, 0x0 } },
	{ UZBEK_LATIN,				{ 0x00000409, 0x00000843, 0x00000419, 0x0, 0x0 } },
	{ UZBEK_CYRILLIC,			{ 0x00000843, 0x00000409, 0x00000419, 0x0, 0x0 } },
	{ VIETNAMESE,				{ 0x00000409, 0x0000042a, 0x0, 0x0, 0x0 } },
	{ WELSH,				{ 0x00000452, 0x00000809, 0x0, 0x0, 0x0 } },
	{ XHOSA,				{ 0x00000409, 0x00000409, 0x0, 0x0, 0x0 } },
};

unsigned int detect_keyboard_layout_from_locale()
{
	int dot;
	int i, j, k;
	int underscore;
	char language[4];
	char country[10];

	/* LANG = <language>_<country>.<encoding> */
	char* envLang = getenv("LANG"); /* Get locale from environment variable LANG */

	if (envLang == NULL)
		return 0; /* LANG environment variable was not set */

	underscore = strcspn(envLang, "_");

	if (underscore > 3)
		return 0; /* The language name should not be more than 3 letters long */
	else
	{
		/* Get language code */
		strncpy(language, envLang, underscore);
		language[underscore] = '\0';
	}

	/*
	 * There is always the special case of "C" or "POSIX" as locale name
	 * In this case, use a U.S. keyboard and a U.S. keyboard layout
	 */

	if ((strcmp(language, "C") == 0) || (strcmp(language, "POSIX") == 0))
		return ENGLISH_UNITED_STATES; /* U.S. Keyboard Layout */

	dot = strcspn(envLang, ".");

	/* Get country code */
	if (dot > underscore)
	{
		strncpy(country, &envLang[underscore + 1], dot - underscore - 1);
		country[dot - underscore - 1] = '\0';
	}
	else
		return 0; /* Invalid locale */

	for (i = 0; i < sizeof(locales) / sizeof(locale); i++)
	{
		if ((strcmp(language, locales[i].language) == 0) && (strcmp(country, locales[i].country) == 0))
			break;
	}

	DEBUG_KBD("Found locale : %s_%s", locales[i].language, locales[i].country);

	for (j = 0; j < sizeof(defaultKeyboardLayouts) / sizeof(localeAndKeyboardLayout); j++)
	{
		if (defaultKeyboardLayouts[j].locale == locales[i].code)
		{
			/* Locale found in list of default keyboard layouts */
			for (k = 0; k < 5; k++)
			{
				if (defaultKeyboardLayouts[j].keyboardLayouts[k] == ENGLISH_UNITED_STATES)
				{
					continue; /* Skip, try to get a more localized keyboard layout */
				}
				else if (defaultKeyboardLayouts[j].keyboardLayouts[k] == 0)
				{
					break; /* No more keyboard layouts */
				}
				else
				{
					return defaultKeyboardLayouts[j].keyboardLayouts[k];
				}
			}

			/*
			 * If we skip the ENGLISH_UNITED_STATES keyboard layout but there are no
			 * other possible keyboard layout for the locale, we end up here with k > 1
			 */

			if (k >= 1)
				return ENGLISH_UNITED_STATES;
			else
				return 0;
		}
	}

	return 0; /* Could not detect the current keyboard layout from locale */
}
