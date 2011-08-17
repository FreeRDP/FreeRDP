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

/* Detection of plausible keyboard layout id based on current locale (LANG) setting. */

/*
 * Refer to "Windows XP/Server 2003 - List of Locale IDs, Input Locale, and Language Collection":
 * http://www.microsoft.com/globaldev/reference/winxp/xp-lcid.mspx
 */

#ifndef __LOCALES_H
#define __LOCALES_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define AFRIKAANS				0x0436
#define ALBANIAN				0x041c
#define ALSATIAN				0x0484
#define AMHARIC					0x045E
#define ARABIC_SAUDI_ARABIA			0x0401
#define ARABIC_IRAQ				0x0801
#define ARABIC_EGYPT				0x0c01
#define ARABIC_LIBYA				0x1001
#define ARABIC_ALGERIA				0x1401
#define ARABIC_MOROCCO				0x1801
#define ARABIC_TUNISIA				0x1c01
#define ARABIC_OMAN				0x2001
#define ARABIC_YEMEN				0x2401
#define ARABIC_SYRIA				0x2801
#define ARABIC_JORDAN				0x2c01
#define ARABIC_LEBANON				0x3001
#define ARABIC_KUWAIT				0x3401
#define ARABIC_UAE				0x3801
#define ARABIC_BAHRAIN				0x3c01
#define ARABIC_QATAR				0x4001
#define ARMENIAN				0x042b
#define ASSAMESE				0x044D
#define AZERI_LATIN				0x042c
#define AZERI_CYRILLIC				0x082c
#define BASHKIR					0x046D
#define BASQUE					0x042d
#define BELARUSIAN				0x0423
#define BENGALI_INDIA				0x0445
#define BOSNIAN_LATIN				0x141A
#define	BRETON					0x047E
#define BULGARIAN				0x0402
#define CATALAN					0x0403
#define CHINESE_TAIWAN				0x0404
#define CHINESE_PRC				0x0804
#define CHINESE_HONG_KONG			0x0c04
#define CHINESE_SINGAPORE			0x1004
#define CHINESE_MACAU				0x1404
#define CROATIAN				0x041a
#define CROATIAN_BOSNIA_HERZEGOVINA		0x101A
#define CZECH					0x0405
#define DANISH					0x0406
#define DARI					0x048C
#define DIVEHI					0x0465
#define DUTCH_STANDARD				0x0413
#define DUTCH_BELGIAN				0x0813
#define ENGLISH_UNITED_STATES			0x0409
#define ENGLISH_UNITED_KINGDOM			0x0809
#define ENGLISH_AUSTRALIAN			0x0c09
#define ENGLISH_CANADIAN			0x1009
#define ENGLISH_NEW_ZEALAND			0x1409
#define ENGLISH_INDIA				0x4009
#define ENGLISH_IRELAND				0x1809
#define ENGLISH_MALAYSIA			0x4409
#define ENGLISH_SOUTH_AFRICA			0x1c09
#define ENGLISH_JAMAICA				0x2009
#define ENGLISH_CARIBBEAN			0x2409
#define ENGLISH_BELIZE				0x2809
#define ENGLISH_TRINIDAD			0x2c09
#define ENGLISH_ZIMBABWE			0x3009
#define ENGLISH_PHILIPPINES			0x3409
#define ENGLISH_SINGAPORE			0x4809
#define ESTONIAN				0x0425
#define FAEROESE				0x0438
#define FARSI					0x0429
#define FILIPINO				0x0464
#define FINNISH					0x040b
#define FRENCH_STANDARD				0x040c
#define FRENCH_BELGIAN				0x080c
#define FRENCH_CANADIAN				0x0c0c
#define FRENCH_SWISS				0x100c
#define FRENCH_LUXEMBOURG			0x140c
#define FRENCH_MONACO				0x180c
#define FRISIAN					0x0462
#define GEORGIAN				0x0437
#define GALICIAN				0x0456
#define GERMAN_STANDARD				0x0407
#define GERMAN_SWISS				0x0807
#define GERMAN_AUSTRIAN				0x0c07
#define GERMAN_LUXEMBOURG			0x1007
#define GERMAN_LIECHTENSTEIN			0x1407
#define GREEK					0x0408
#define GREENLANDIC				0x046F
#define GUJARATI				0x0447
#define HEBREW					0x040d
#define HINDI					0x0439
#define HUNGARIAN				0x040e
#define ICELANDIC				0x040f
#define IGBO					0x0470
#define INDONESIAN				0x0421
#define IRISH					0x083C
#define ITALIAN_STANDARD			0x0410
#define ITALIAN_SWISS				0x0810
#define JAPANESE				0x0411
#define KANNADA					0x044b
#define KAZAKH					0x043f
#define KHMER					0x0453
#define KICHE					0x0486
#define KINYARWANDA				0x0487
#define KONKANI					0x0457
#define KOREAN					0x0412
#define KYRGYZ					0x0440
#define LAO					0x0454
#define LATVIAN					0x0426
#define LITHUANIAN				0x0427
#define LOWER_SORBIAN				0x082E
#define LUXEMBOURGISH				0x046E
#define MACEDONIAN				0x042f
#define MALAY_MALAYSIA				0x043e
#define MALAY_BRUNEI_DARUSSALAM			0x083e
#define MALAYALAM				0x044c
#define MALTESE					0x043a
#define MAPUDUNGUN				0x047A
#define MAORI					0x0481
#define MARATHI					0x044e
#define MOHAWK					0x047C
#define MONGOLIAN				0x0450
#define NEPALI					0x0461
#define NORWEGIAN_BOKMAL			0x0414
#define NORWEGIAN_NYNORSK			0x0814
#define OCCITAN					0x0482
#define ORIYA					0x0448
#define PASHTO					0x0463
#define POLISH					0x0415
#define PORTUGUESE_BRAZILIAN			0x0416
#define PORTUGUESE_STANDARD			0x0816
#define PUNJABI					0x0446
#define QUECHUA_BOLIVIA				0x046b
#define QUECHUA_ECUADOR				0x086b
#define QUECHUA_PERU				0x0c6b
#define ROMANIAN				0x0418
#define ROMANSH					0x0417
#define RUSSIAN					0x0419
#define SAMI_INARI				0x243b
#define SAMI_LULE_NORWAY			0x103b
#define SAMI_LULE_SWEDEN			0x143b
#define SAMI_NORTHERN_FINLAND			0x0c3b
#define SAMI_NORTHERN_NORWAY			0x043b
#define SAMI_NORTHERN_SWEDEN			0x083b
#define SAMI_SKOLT				0x203b
#define SAMI_SOUTHERN_NORWAY			0x183b
#define SAMI_SOUTHERN_SWEDEN			0x1c3b
#define SANSKRIT				0x044f
#define SERBIAN_LATIN				0x081a
#define SERBIAN_LATIN_BOSNIA_HERZEGOVINA	0x181a
#define SERBIAN_CYRILLIC			0x0c1a
#define SERBIAN_CYRILLIC_BOSNIA_HERZEGOVINA	0x1c1a
#define SESOTHO_SA_LEBOA			0x046C
#define SINHALA					0x045B
#define SLOVAK					0x041b
#define SLOVENIAN				0x0424
#define SPANISH_TRADITIONAL_SORT		0x040a
#define SPANISH_MEXICAN				0x080a
#define SPANISH_MODERN_SORT			0x0c0a
#define SPANISH_GUATEMALA			0x100a
#define SPANISH_COSTA_RICA			0x140a
#define SPANISH_PANAMA				0x180a
#define SPANISH_DOMINICAN_REPUBLIC		0x1c0a
#define SPANISH_VENEZUELA			0x200a
#define SPANISH_COLOMBIA			0x240a
#define SPANISH_PERU				0x280a
#define SPANISH_ARGENTINA			0x2c0a
#define SPANISH_ECUADOR				0x300a
#define SPANISH_CHILE				0x340a
#define SPANISH_UNITED_STATES			0x540A
#define SPANISH_URUGUAY				0x380a
#define SPANISH_PARAGUAY			0x3c0a
#define SPANISH_BOLIVIA				0x400a
#define SPANISH_EL_SALVADOR			0x440a
#define SPANISH_HONDURAS			0x480a
#define SPANISH_NICARAGUA			0x4c0a
#define SPANISH_PUERTO_RICO			0x500a
#define SWAHILI					0x0441
#define SWEDISH					0x041d
#define SWEDISH_FINLAND				0x081d
#define SYRIAC					0x045a
#define TAMIL					0x0449
#define TATAR					0x0444
#define TELUGU					0x044a
#define THAI					0x041e
#define TIBETAN_BHUTAN				0x0851
#define TIBETAN_PRC				0x0451
#define TSWANA					0x0432
#define UKRAINIAN				0x0422
#define TURKISH					0x041f
#define TURKMEN					0x0442
#define UIGHUR					0x0480
#define UPPER_SORBIAN				0x042E
#define URDU					0x0420
#define URDU_INDIA				0x0820
#define UZBEK_LATIN				0x0443
#define UZBEK_CYRILLIC				0x0843
#define VIETNAMESE				0x042a
#define WELSH					0x0452
#define WOLOF					0x0488
#define XHOSA					0x0434
#define YAKUT					0x0485
#define YI					0x0478
#define YORUBA					0x046A
#define ZULU					0x0435


/*
Time zones, taken from Windows Server 2008

(GMT -12:00) International Date Line West
(GMT -11:00) Midway Island, Samoa
(GMT -10:00) Hawaii
(GMT -09:00) Alaska
(GMT -08:00) Pacific Time (US & Canada)
(GMT -08:00) Tijuana, Baja California
(GMT -07:00) Arizona
(GMT -07:00) Chihuahua, La Paz, Mazatlan
(GMT -07:00) Mountain Time (US & Canada)
(GMT -06:00) Central America
(GMT -06:00) Central Time (US & Canada)
(GMT -06:00) Guadalajara, Mexico City, Monterrey
(GMT -06:00) Saskatchewan
(GMT -05:00) Bogota, Lima, Quito, Rio Branco
(GMT -05:00) Eastern Time (US & Canada)
(GMT -05:00) Indiana (East)
(GMT -04:30) Caracas
(GMT -04:00) Atlantic Time (Canada)
(GMT -04:00) La Paz
(GMT -04:00) Manaus
(GMT -04:00) Santiago
(GMT -03:30) Newfoundland
(GMT -03:00) Brasilia
(GMT -03:00) Buenos Aires
(GMT -03:00) Georgetown
(GMT -03:00) Greenland
(GMT -03:00) Montevideo
(GMT -02:00) Mid-Atlantic
(GMT -01:00) Azores
(GMT -01:00) Cape Verde Is.
(GMT +00:00) Casablanca
(GMT +00:00) Greenwich Mean Time: Dublin, Edinburgh, Lisbon, London
(GMT +00:00) Monrovia, Reykjavik
(GMT +01:00) Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna
(GMT +01:00) Belgrade, Bratislava, Budapest, Ljubljana, Prague
(GMT +01:00) Brussels, Copenhagen, Madrid, Paris
(GMT +01:00) Sarajevo, Skopje, Warsaw, Zagreb
(GMT +01:00) West Central Africa
(GMT +02:00) Amman
(GMT +02:00) Athens, Bucharest, Istanbul
(GMT +02:00) Beirut
(GMT +02:00) Cairo
(GMT +02:00) Harare, Pretoria
(GMT +02:00) Helsinki, Kyiv, Riga, Sofia, Tallinn, Vilnius
(GMT +02:00) Jerusalem
(GMT +02:00) Minsk
(GMT +02:00) Windhoek
(GMT +03:00) Baghdad
(GMT +03:00) Kuwait, Riyadh
(GMT +03:00) Moscow, St. Petersburg, Volgograd
(GMT +03:00) Nairobi
(GMT +03:00) Tbilisi
(GMT +03:30) Tehran
(GMT +04:00) Abu Dhabi, Muscat
(GMT +04:00) Baku
(GMT +04:00) Port Louis
(GMT +04:00) Yerevan
(GMT +04:30) Kabul
(GMT +05:00) Ekaterinburg
(GMT +05:00) Islamabad, Karachi
(GMT +05:00) Tashkent
(GMT +05:30) Chennai, Kolkata, Mumbai, New Delhi
(GMT +05:30) Sri Jayawardenepura
(GMT +05:45) Kathmandu
(GMT +06:00) Almaty, Novosibirsk
(GMT +06:00) Astana, Dhaka
(GMT +06:30) Yangon (Rangoon)
(GMT +07:00) Bangkok, Hanoi, Jakarta
(GMT +07:00) Krasnoyarsk
(GMT +08:00) Beijing, Chongqing, Hong Kong, Urumqi
(GMT +08:00) Irkutsk, Ulaan Bataar
(GMT +08:00) Kuala Lumpur, Singapore
(GMT +08:00) Perth
(GMT +08:00) Taipei
(GMT +09:00) Osaka, Sapporo, Tokyo
(GMT +09:00) Seoul
(GMT +09:00) Yakutsk
(GMT +09:30) Adelaide
(GMT +09:30) Darwin
(GMT +10:00) Brisbane
(GMT +10:00) Canberra, Melbourne, Sydney
(GMT +10:00) Guam, Port Moresby
(GMT +10:00) Hobart, Vladivostok
(GMT +11:00) Magadan, Solomon Is., New Caledonia
(GMT +12:00) Auckland, Wellington
(GMT +12:00) Fiji, Kamchatka, Marshall Is.
(GMT +13:00) Nuku'alofa
*/

FREERDP_API uint32 detect_keyboard_layout_from_locale();

#endif /* __LOCALES_H */
