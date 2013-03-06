/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Microsoft Locales
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

/* Detection of plausible keyboard layout id based on current locale (LANG) setting. */

/*
 * Refer to "Windows XP/Server 2003 - List of Locale IDs, Input Locale, and Language Collection":
 * http://www.microsoft.com/globaldev/reference/winxp/xp-lcid.mspx
 */

#ifndef FREERDP_LOCALE_H
#define FREERDP_LOCALE_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define AFRIKAANS					0x0436
#define ALBANIAN					0x041C
#define ALSATIAN					0x0484
#define AMHARIC						0x045E
#define ARABIC_SAUDI_ARABIA				0x0401
#define ARABIC_IRAQ					0x0801
#define ARABIC_EGYPT					0x0C01
#define ARABIC_LIBYA					0x1001
#define ARABIC_ALGERIA					0x1401
#define ARABIC_MOROCCO					0x1801
#define ARABIC_TUNISIA					0x1C01
#define ARABIC_OMAN					0x2001
#define ARABIC_YEMEN					0x2401
#define ARABIC_SYRIA					0x2801
#define ARABIC_JORDAN					0x2C01
#define ARABIC_LEBANON					0x3001
#define ARABIC_KUWAIT					0x3401
#define ARABIC_UAE					0x3801
#define ARABIC_BAHRAIN					0x3C01
#define ARABIC_QATAR					0x4001
#define ARMENIAN					0x042B
#define ASSAMESE					0x044D
#define AZERI_LATIN					0x042C
#define AZERI_CYRILLIC					0x082C
#define BASHKIR						0x046D
#define BASQUE						0x042D
#define BELARUSIAN					0x0423
#define BENGALI_INDIA					0x0445
#define BOSNIAN_LATIN					0x141A
#define BRETON						0x047E
#define BULGARIAN					0x0402
#define CATALAN						0x0403
#define CHINESE_TAIWAN					0x0404
#define CHINESE_PRC					0x0804
#define CHINESE_HONG_KONG				0x0C04
#define CHINESE_SINGAPORE				0x1004
#define CHINESE_MACAU					0x1404
#define CROATIAN					0x041A
#define CROATIAN_BOSNIA_HERZEGOVINA			0x101A
#define CZECH						0x0405
#define DANISH						0x0406
#define DARI						0x048C
#define DIVEHI						0x0465
#define DUTCH_STANDARD					0x0413
#define DUTCH_BELGIAN					0x0813
#define ENGLISH_UNITED_STATES				0x0409
#define ENGLISH_UNITED_KINGDOM				0x0809
#define ENGLISH_AUSTRALIAN				0x0C09
#define ENGLISH_CANADIAN				0x1009
#define ENGLISH_NEW_ZEALAND				0x1409
#define ENGLISH_INDIA					0x4009
#define ENGLISH_IRELAND					0x1809
#define ENGLISH_MALAYSIA				0x4409
#define ENGLISH_SOUTH_AFRICA				0x1C09
#define ENGLISH_JAMAICA					0x2009
#define ENGLISH_CARIBBEAN				0x2409
#define ENGLISH_BELIZE					0x2809
#define ENGLISH_TRINIDAD				0x2C09
#define ENGLISH_ZIMBABWE				0x3009
#define ENGLISH_PHILIPPINES				0x3409
#define ENGLISH_SINGAPORE				0x4809
#define ESTONIAN					0x0425
#define FAEROESE					0x0438
#define FARSI						0x0429
#define FILIPINO					0x0464
#define FINNISH						0x040B
#define FRENCH_STANDARD					0x040C
#define FRENCH_BELGIAN					0x080C
#define FRENCH_CANADIAN					0x0C0C
#define FRENCH_SWISS					0x100C
#define FRENCH_LUXEMBOURG				0x140C
#define FRENCH_MONACO					0x180C
#define FRISIAN						0x0462
#define GEORGIAN					0x0437
#define GALICIAN					0x0456
#define GERMAN_STANDARD					0x0407
#define GERMAN_SWISS					0x0807
#define GERMAN_AUSTRIAN					0x0C07
#define GERMAN_LUXEMBOURG				0x1007
#define GERMAN_LIECHTENSTEIN				0x1407
#define GREEK						0x0408
#define GREENLANDIC					0x046F
#define GUJARATI					0x0447
#define HEBREW						0x040D
#define HINDI						0x0439
#define HUNGARIAN					0x040E
#define ICELANDIC					0x040F
#define IGBO						0x0470
#define INDONESIAN					0x0421
#define IRISH						0x083C
#define ITALIAN_STANDARD				0x0410
#define ITALIAN_SWISS					0x0810
#define JAPANESE					0x0411
#define KANNADA						0x044B
#define KAZAKH						0x043F
#define KHMER						0x0453
#define KICHE						0x0486
#define KINYARWANDA					0x0487
#define KONKANI						0x0457
#define KOREAN						0x0412
#define KYRGYZ						0x0440
#define LAO						0x0454
#define LATVIAN						0x0426
#define LITHUANIAN					0x0427
#define LOWER_SORBIAN					0x082E
#define LUXEMBOURGISH					0x046E
#define MACEDONIAN					0x042F
#define MALAY_MALAYSIA					0x043E
#define MALAY_BRUNEI_DARUSSALAM				0x083E
#define MALAYALAM					0x044C
#define MALTESE						0x043A
#define MAPUDUNGUN					0x047A
#define MAORI						0x0481
#define MARATHI						0x044E
#define MOHAWK						0x047C
#define MONGOLIAN					0x0450
#define NEPALI						0x0461
#define NORWEGIAN_BOKMAL				0x0414
#define NORWEGIAN_NYNORSK				0x0814
#define OCCITAN						0x0482
#define ORIYA						0x0448
#define PASHTO						0x0463
#define POLISH						0x0415
#define PORTUGUESE_BRAZILIAN				0x0416
#define PORTUGUESE_STANDARD				0x0816
#define PUNJABI						0x0446
#define QUECHUA_BOLIVIA					0x046B
#define QUECHUA_ECUADOR					0x086B
#define QUECHUA_PERU					0x0C6B
#define ROMANIAN					0x0418
#define ROMANSH						0x0417
#define RUSSIAN						0x0419
#define SAMI_INARI					0x243B
#define SAMI_LULE_NORWAY				0x103B
#define SAMI_LULE_SWEDEN				0x143B
#define SAMI_NORTHERN_FINLAND				0x0C3B
#define SAMI_NORTHERN_NORWAY				0x043B
#define SAMI_NORTHERN_SWEDEN				0x083B
#define SAMI_SKOLT					0x203B
#define SAMI_SOUTHERN_NORWAY				0x183B
#define SAMI_SOUTHERN_SWEDEN				0x1C3B
#define SANSKRIT					0x044F
#define SERBIAN_LATIN					0x081A
#define SERBIAN_LATIN_BOSNIA_HERZEGOVINA		0x181A
#define SERBIAN_CYRILLIC				0x0C1A
#define SERBIAN_CYRILLIC_BOSNIA_HERZEGOVINA		0x1C1A
#define SESOTHO_SA_LEBOA				0x046C
#define SINHALA						0x045B
#define SLOVAK						0x041B
#define SLOVENIAN					0x0424
#define SPANISH_TRADITIONAL_SORT			0x040A
#define SPANISH_MEXICAN					0x080A
#define SPANISH_MODERN_SORT				0x0C0A
#define SPANISH_GUATEMALA				0x100A
#define SPANISH_COSTA_RICA				0x140A
#define SPANISH_PANAMA					0x180A
#define SPANISH_DOMINICAN_REPUBLIC			0x1C0A
#define SPANISH_VENEZUELA				0x200A
#define SPANISH_COLOMBIA				0x240A
#define SPANISH_PERU					0x280A
#define SPANISH_ARGENTINA				0x2C0A
#define SPANISH_ECUADOR					0x300A
#define SPANISH_CHILE					0x340A
#define SPANISH_UNITED_STATES				0x540A
#define SPANISH_URUGUAY					0x380A
#define SPANISH_PARAGUAY				0x3C0A
#define SPANISH_BOLIVIA					0x400A
#define SPANISH_EL_SALVADOR				0x440A
#define SPANISH_HONDURAS				0x480A
#define SPANISH_NICARAGUA				0x4C0A
#define SPANISH_PUERTO_RICO				0x500A
#define SWAHILI						0x0441
#define SWEDISH						0x041D
#define SWEDISH_FINLAND					0x081D
#define SYRIAC						0x045A
#define TAMIL						0x0449
#define TATAR						0x0444
#define TELUGU						0x044A
#define THAI						0x041E
#define TIBETAN_BHUTAN					0x0851
#define TIBETAN_PRC					0x0451
#define TSWANA						0x0432
#define UKRAINIAN					0x0422
#define TURKISH						0x041F
#define TURKMEN						0x0442
#define UIGHUR						0x0480
#define UPPER_SORBIAN					0x042E
#define URDU						0x0420
#define URDU_INDIA					0x0820
#define UZBEK_LATIN					0x0443
#define UZBEK_CYRILLIC					0x0843
#define VIETNAMESE					0x042A
#define WELSH						0x0452
#define WOLOF						0x0488
#define XHOSA						0x0434
#define YAKUT						0x0485
#define YI						0x0478
#define YORUBA						0x046A
#define ZULU						0x0435

FREERDP_API DWORD freerdp_get_system_locale_id(void);
FREERDP_API const char* freerdp_get_system_locale_name_from_id(DWORD localeId);
FREERDP_API int freerdp_detect_keyboard_layout_from_system_locale(DWORD* keyboardLayoutId);

#endif /* FREERDP_LOCALE_H */
