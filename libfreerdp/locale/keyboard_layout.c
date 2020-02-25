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

struct LanguageIdentifier
{
	/* LanguageIdentifier = (SublangaugeIdentifier<<2) | PrimaryLanguageIdentifier
	 * The table at
	 * https://docs.microsoft.com/en-us/windows/win32/intl/language-identifier-constants-and-strings
	 * is sometimes missing one or both of the entries.
	 */
	const char* locale; /* en_US type strings for the locale */
	UINT16 LanguageIdentifier;
	const char* PrimaryLanguage;
	UINT8 PrimaryLanguageIdentifier;
	const char* PrimaryLanguageSymbol;
	const char* Sublanguage;
	UINT8 SublangaugeIdentifier;
	const char* SublanguageSymbol;
};

const struct LanguageIdentifier language_identifiers[] = {
	/* 	[Language identifier]		[Primary language]		[Prim. lang. identifier]		[Prim.
	   lang. symbol]		[Sublanguage]		[Sublang. identifier]		[Sublang. symbol]	*/
	{ "", 0xc00, "Default custom locale language", 0x0, "LANG_NEUTRAL",
	  "Default custom sublanguage", 0x3, "SUBLANG_CUSTOM_DEFAULT" },
	{ "", 0x1400, "Default custom MUI locale language", 0x0, "LANG_NEUTRAL",
	  "Default custom MUI sublanguage", 0x5, "SUBLANG_UI_CUSTOM_DEFAULT" },
	{ "", 0x7f, "Invariant locale language", 0x7f, "LANG_INVARIANT", "Invariant sublanguage", 0x0,
	  "SUBLANG_NEUTRAL" },
	{ "", 0x0, "Neutral locale language", 0x0, "LANG_NEUTRAL", "Neutral sublanguage", 0x0,
	  "SUBLANG_NEUTRAL" },
	{ "", 0x800, "System default locale language", 0x2, "LANG_SYSTEM_DEFAULT",
	  "System default sublanguage", 0x2, "SUBLANG_SYS_DEFAULT" },
	{ "", 0x1000, "Unspecified custom locale language", 0x0, "LANG_NEUTRAL",
	  "Unspecified custom sublanguage", 0x4, "SUBLANG_CUSTOM_UNSPECIFIED" },
	{ "", 0x400, "User default locale language", 0x0, "LANG_USER_DEFAULT",
	  "User default sublanguage", 0x1, "SUBLANG_DEFAULT" },
	{ "af_ZA", 0x436, "Afrikaans (af)", 0x36, "LANG_AFRIKAANS", "South Africa (ZA)", 0x1,
	  "SUBLANG_AFRIKAANS_SOUTH_AFRICA" },
	{ "sq_AL", 0x41c, "Albanian (sq)", 0x1c, "LANG_ALBANIAN", "Albania (AL)", 0x1,
	  "SUBLANG_ALBANIAN_ALBANIA" },
	{ "gsw_FR", 0x484, "Alsatian (gsw)", 0x84, "LANG_ALSATIAN", "France (FR)", 0x1,
	  "SUBLANG_ALSATIAN_FRANCE" },
	{ "am_ET", 0x45e, "Amharic (am)", 0x5e, "LANG_AMHARIC", "Ethiopia (ET)", 0x1,
	  "SUBLANG_AMHARIC_ETHIOPIA" },
	{ "ar_DZ", 0x1401, "Arabic (ar)", 0x1, "LANG_ARABIC", "Algeria (DZ)", 0x5,
	  "SUBLANG_ARABIC_ALGERIA" },
	{ "ar_BH", 0x3c01, "Arabic (ar)", 0x01, "LANG_ARABIC", "Bahrain (BH)", 0xf,
	  "SUBLANG_ARABIC_BAHRAIN" },
	{ "ar_EG", 0xc01, "Arabic (ar)", 0x01, "LANG_ARABIC", "Egypt (EG)", 0x3,
	  "SUBLANG_ARABIC_EGYPT" },
	{ "ar_IQ", 0x801, "Arabic (ar)", 0x01, "LANG_ARABIC", "Iraq (IQ)", 0x2, "SUBLANG_ARABIC_IRAQ" },
	{ "ar_JO", 0x2c01, "Arabic (ar)", 0x01, "LANG_ARABIC", "Jordan (JO)", 0xb,
	  "SUBLANG_ARABIC_JORDAN" },
	{ "ar_KW", 0x3401, "Arabic (ar)", 0x01, "LANG_ARABIC", "Kuwait (KW)", 0xd,
	  "SUBLANG_ARABIC_KUWAIT" },
	{ "ar_LB", 0x3001, "Arabic (ar)", 0x01, "LANG_ARABIC", "Lebanon (LB)", 0xc,
	  "SUBLANG_ARABIC_LEBANON" },
	{ "ar_LY", 0x1001, "Arabic (ar)", 0x01, "LANG_ARABIC", "Libya (LY)", 0x4,
	  "SUBLANG_ARABIC_LIBYA" },
	{ "ar_MA", 0x1801, "Arabic (ar)", 0x01, "LANG_ARABIC", "Morocco (MA)", 0x6,
	  "SUBLANG_ARABIC_MOROCCO" },
	{ "ar_OM", 0x2001, "Arabic (ar)", 0x01, "LANG_ARABIC", "Oman (OM)", 0x8,
	  "SUBLANG_ARABIC_OMAN" },
	{ "ar_QA", 0x4001, "Arabic (ar)", 0x01, "LANG_ARABIC", "Qatar (QA)", 0x10,
	  "SUBLANG_ARABIC_QATAR" },
	{ "ar_SA", 0x401, "Arabic (ar)", 0x01, "LANG_ARABIC", "Saudi Arabia (SA)", 0x1,
	  "SUBLANG_ARABIC_SAUDI_ARABIA" },
	{ "ar_SY", 0x2801, "Arabic (ar)", 0x01, "LANG_ARABIC", "Syria (SY)", 0xa,
	  "SUBLANG_ARABIC_SYRIA" },
	{ "ar_TN", 0x1c01, "Arabic (ar)", 0x01, "LANG_ARABIC", "Tunisia (TN)", 0x7,
	  "SUBLANG_ARABIC_TUNISIA" },
	{ "ar_AE", 0x3801, "Arabic (ar)", 0x01, "LANG_ARABIC", "U.A.E. (AE)", 0xe,
	  "SUBLANG_ARABIC_UAE" },
	{ "ar_YE", 0x2401, "Arabic (ar)", 0x01, "LANG_ARABIC", "Yemen (YE)", 0x9,
	  "SUBLANG_ARABIC_YEMEN" },
	{ "hy_AM", 0x42b, "Armenian (hy)", 0x2b, "LANG_ARMENIAN", "Armenia (AM)", 0x1,
	  "SUBLANG_ARMENIAN_ARMENIA" },
	{ "as_IN", 0x44d, "Assamese (as)", 0x4d, "LANG_ASSAMESE", "India (IN)", 0x1,
	  "SUBLANG_ASSAMESE_INDIA" },
	{ "az_AZ", 0x82c, "Azerbaijani (az)", 0x2c, "LANG_AZERI", "Azerbaijan, Cyrillic (AZ)", 0x2,
	  "SUBLANG_AZERI_CYRILLIC" },
	{ "az_AZ", 0x42c, "Azerbaijani (az)", 0x2c, "LANG_AZERI", "Azerbaijan, Latin (AZ)", 0x1,
	  "SUBLANG_AZERI_LATIN" },
	{ "bn_BD", 0x445, "Bangla (bn)", 0x45, "LANG_BANGLA", "Bangladesh (BD)", 0x2,
	  "SUBLANG_BANGLA_BANGLADESH" },
	{ "bn_IN", 0x445, "Bangla (bn)", 0x45, "LANG_BANGLA", "India (IN)", 0x1,
	  "SUBLANG_BANGLA_INDIA" },
	{ "ba_RU", 0x46d, "Bashkir (ba)", 0x6d, "LANG_BASHKIR", "Russia (RU)", 0x1,
	  "SUBLANG_BASHKIR_RUSSIA" },
	{ "", 0x42d, "Basque (Basque)", 0x2d, "LANG_BASQUE", "Basque (Basque)", 0x1,
	  "SUBLANG_BASQUE_BASQUE" },
	{ "be_BY", 0x423, "Belarusian (be)", 0x23, "LANG_BELARUSIAN", "Belarus (BY)", 0x1,
	  "SUBLANG_BELARUSIAN_BELARUS" },
	{ "bs", 0x781a, "Bosnian (bs)", 0x1a, "LANG_BOSNIAN_NEUTRAL", "Neutral", 0x1E, "" },
	{ "bs_BA", 0x201a, "Bosnian (bs)", 0x1a, "LANG_BOSNIAN",
	  "Bosnia and Herzegovina, Cyrillic (BA)", 0x8, "SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC" },
	{ "bs_BA", 0x141a, "Bosnian (bs)", 0x1a, "LANG_BOSNIAN", "Bosnia and Herzegovina, Latin (BA)",
	  0x5, "SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN" },
	{ "br_FR", 0x47e, "Breton (br)", 0x7e, "LANG_BRETON", "France (FR)", 0x1,
	  "SUBLANG_BRETON_FRANCE" },
	{ "bg_BG", 0x402, "Bulgarian (bg)", 0x2, "LANG_BULGARIAN", "Bulgaria (BG)", 0x1,
	  "SUBLANG_BULGARIAN_BULGARIA" },
	{ "ku_IQ", 0x492, "Central Kurdish (ku)", 0x92, "LANG_CENTRAL_KURDISH", "Iraq (IQ)", 0x1,
	  "SUBLANG_CENTRAL_KURDISH_IRAQ" },
	{ "chr_US", 0x45c, "Cherokee (chr)", 0x5c, "LANG_CHEROKEE", "Cherokee (Cher)", 0x1,
	  "SUBLANG_CHEROKEE_CHEROKEE" },
	{ "ca_ES", 0x403, "Catalan (ca)", 0x3, "LANG_CATALAN", "Spain (ES)", 0x1,
	  "SUBLANG_CATALAN_CATALAN" },
	{ "zh_HK", 0xc04, "Chinese (zh)", 0x04, "LANG_CHINESE", "Hong Kong SAR, PRC (HK)", 0x3,
	  "SUBLANG_CHINESE_HONGKONG" },
	{ "zh_MO", 0x1404, "Chinese (zh)", 0x04, "LANG_CHINESE", "Macao SAR (MO)", 0x5,
	  "SUBLANG_CHINESE_MACAU" },
	{ "zh_SG", 0x1004, "Chinese (zh)", 0x04, "LANG_CHINESE", "Singapore (SG)", 0x4,
	  "SUBLANG_CHINESE_SINGAPORE" },
	{ "zh_CN", 0x4, "Chinese (zh)", 0x4, "LANG_CHINESE_SIMPLIFIED", "Simplified (Hans)", 0x2,
	  "SUBLANG_CHINESE_SIMPLIFIED" },
	{ "zh_CN", 0x7c04, "Chinese (zh)", 0x04, "LANG_CHINESE_TRADITIONAL", "Traditional (Hant)", 0x1,
	  "SUBLANG_CHINESE_TRADITIONAL" },
	{ "co_FR", 0x483, "Corsican (co)", 0x83, "LANG_CORSICAN", "France (FR)", 0x1,
	  "SUBLANG_CORSICAN_FRANCE" },
	{ "hr", 0x1a, "Croatian (hr)", 0x1a, "LANG_CROATIAN", "Neutral", 0x00, "" },
	{ "hr_BA", 0x101a, "Croatian (hr)", 0x1a, "LANG_CROATIAN", "Bosnia and Herzegovina, Latin (BA)",
	  0x4, "SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN" },
	{ "hr_HR", 0x41a, "Croatian (hr)", 0x1a, "LANG_CROATIAN", "Croatia (HR)", 0x1,
	  "SUBLANG_CROATIAN_CROATIA" },
	{ "cs_CZ", 0x405, "Czech (cs)", 0x5, "LANG_CZECH", "Czech Republic (CZ)", 0x1,
	  "SUBLANG_CZECH_CZECH_REPUBLIC" },
	{ "da_DK", 0x406, "Danish (da)", 0x6, "LANG_DANISH", "Denmark (DK)", 0x1,
	  "SUBLANG_DANISH_DENMARK" },
	{ "prs_AF", 0x48c, "Dari (prs)", 0x8c, "LANG_DARI", "Afghanistan (AF)", 0x1,
	  "SUBLANG_DARI_AFGHANISTAN" },
	{ "dv_MV", 0x465, "Divehi (dv)", 0x65, "LANG_DIVEHI", "Maldives (MV)", 0x1,
	  "SUBLANG_DIVEHI_MALDIVES" },
	{ "nl_BE", 0x813, "Dutch (nl)", 0x13, "LANG_DUTCH", "Belgium (BE)", 0x2,
	  "SUBLANG_DUTCH_BELGIAN" },
	{ "nl_NL", 0x413, "Dutch (nl)", 0x13, "LANG_DUTCH", "Netherlands (NL)", 0x1, "SUBLANG_DUTCH" },
	{ "en_AU", 0xc09, "English (en)", 0x9, "LANG_ENGLISH", "Australia (AU)", 0x3,
	  "SUBLANG_ENGLISH_AUS" },
	{ "en_BZ", 0x2809, "English (en)", 0x09, "LANG_ENGLISH", "Belize (BZ)", 0xa,
	  "SUBLANG_ENGLISH_BELIZE" },
	{ "en_CA", 0x1009, "English (en)", 0x09, "LANG_ENGLISH", "Canada (CA)", 0x4,
	  "SUBLANG_ENGLISH_CAN" },
	{ "en_CB", 0x2409, "English (en)", 0x09, "LANG_ENGLISH", "Caribbean (029)", 0x9,
	  "SUBLANG_ENGLISH_CARIBBEAN" },
	{ "en_IN", 0x4009, "English (en)", 0x09, "LANG_ENGLISH", "India (IN)", 0x10,
	  "SUBLANG_ENGLISH_INDIA" },
	{ "en_IE", 0x1809, "English (en)", 0x09, "LANG_ENGLISH", "Ireland (IE)", 0x6,
	  "SUBLANG_ENGLISH_EIRE" },
	{ "en_IE", 0x1809, "English (en)", 0x09, "LANG_ENGLISH", "Ireland (IE)", 0x6,
	  "SUBLANG_ENGLISH_IRELAND" },
	{ "en_JM", 0x2009, "English (en)", 0x09, "LANG_ENGLISH", "Jamaica (JM)", 0x8,
	  "SUBLANG_ENGLISH_JAMAICA" },
	{ "en_MY", 0x4409, "English (en)", 0x09, "LANG_ENGLISH", "Malaysia (MY)", 0x11,
	  "SUBLANG_ENGLISH_MALAYSIA" },
	{ "en_NZ", 0x1409, "English (en)", 0x09, "LANG_ENGLISH", "New Zealand (NZ)", 0x5,
	  "SUBLANG_ENGLISH_NZ" },
	{ "en_PH", 0x3409, "English (en)", 0x09, "LANG_ENGLISH", "Philippines (PH)", 0xd,
	  "SUBLANG_ENGLISH_PHILIPPINES" },
	{ "en_SG", 0x4809, "English (en)", 0x09, "LANG_ENGLISH", "Singapore (SG)", 0x12,
	  "SUBLANG_ENGLISH_SINGAPORE" },
	{ "en_ZA", 0x1c09, "English (en)", 0x09, "LANG_ENGLISH", "South Africa (ZA)", 0x7,
	  "SUBLANG_ENGLISH_SOUTH_AFRICA" },
	{ "en_TT", 0x2c09, "English (en)", 0x09, "LANG_ENGLISH", "Trinidad and Tobago (TT)", 0xb,
	  "SUBLANG_ENGLISH_TRINIDAD" },
	{ "en_GB", 0x809, "English (en)", 0x09, "LANG_ENGLISH", "United Kingdom (GB)", 0x2,
	  "SUBLANG_ENGLISH_UK" },
	{ "en_US", 0x409, "English (en)", 0x09, "LANG_ENGLISH", "United States (US)", 0x1,
	  "SUBLANG_ENGLISH_US" },
	{ "en_ZW", 0x3009, "English (en)", 0x09, "LANG_ENGLISH", "Zimbabwe (ZW)", 0xc,
	  "SUBLANG_ENGLISH_ZIMBABWE" },
	{ "et_EE", 0x425, "Estonian (et)", 0x25, "LANG_ESTONIAN", "Estonia (EE)", 0x1,
	  "SUBLANG_ESTONIAN_ESTONIA" },
	{ "fo_FO", 0x438, "Faroese (fo)", 0x38, "LANG_FAEROESE", "Faroe Islands (FO)", 0x1,
	  "SUBLANG_FAEROESE_FAROE_ISLANDS" },
	{ "fil_PH", 0x464, "Filipino (fil)", 0x64, "LANG_FILIPINO", "Philippines (PH)", 0x1,
	  "SUBLANG_FILIPINO_PHILIPPINES" },
	{ "fi_FI", 0x40b, "Finnish (fi)", 0xb, "LANG_FINNISH", "Finland (FI)", 0x1,
	  "SUBLANG_FINNISH_FINLAND" },
	{ "fr_BE", 0x80c, "French (fr)", 0xc, "LANG_FRENCH", "Belgium (BE)", 0x2,
	  "SUBLANG_FRENCH_BELGIAN" },
	{ "fr_CA", 0xc0c, "French (fr)", 0x0c, "LANG_FRENCH", "Canada (CA)", 0x3,
	  "SUBLANG_FRENCH_CANADIAN" },
	{ "fr_FR", 0x40c, "French (fr)", 0x0c, "LANG_FRENCH", "France (FR)", 0x1, "SUBLANG_FRENCH" },
	{ "fr_LU", 0x140c, "French (fr)", 0x0c, "LANG_FRENCH", "Luxembourg (LU)", 0x5,
	  "SUBLANG_FRENCH_LUXEMBOURG" },
	{ "fr_MC", 0x180c, "French (fr)", 0x0c, "LANG_FRENCH", "Monaco (MC)", 0x6,
	  "SUBLANG_FRENCH_MONACO" },
	{ "fr_CH", 0x100c, "French (fr)", 0x0c, "LANG_FRENCH", "Switzerland (CH)", 0x4,
	  "SUBLANG_FRENCH_SWISS" },
	{ "fy_NL", 0x462, "Frisian (fy)", 0x62, "LANG_FRISIAN", "Netherlands (NL)", 0x1,
	  "SUBLANG_FRISIAN_NETHERLANDS" },
	{ "gl_ES", 0x456, "Galician (gl)", 0x56, "LANG_GALICIAN", "Spain (ES)", 0x1,
	  "SUBLANG_GALICIAN_GALICIAN" },
	{ "ka_GE", 0x437, "Georgian (ka)", 0x37, "LANG_GEORGIAN", "Georgia (GE)", 0x1,
	  "SUBLANG_GEORGIAN_GEORGIA" },
	{ "de_AT", 0xc07, "German (de)", 0x7, "LANG_GERMAN", "Austria (AT)", 0x3,
	  "SUBLANG_GERMAN_AUSTRIAN" },
	{ "de_DE", 0x407, "German (de)", 0x07, "LANG_GERMAN", "Germany (DE)", 0x1, "SUBLANG_GERMAN" },
	{ "de_LI", 0x1407, "German (de)", 0x07, "LANG_GERMAN", "Liechtenstein (LI)", 0x5,
	  "SUBLANG_GERMAN_LIECHTENSTEIN" },
	{ "de_LU", 0x1007, "German (de)", 0x07, "LANG_GERMAN", "Luxembourg (LU)", 0x4,
	  "SUBLANG_GERMAN_LUXEMBOURG" },
	{ "de_CH", 0x807, "German (de)", 0x07, "LANG_GERMAN", "Switzerland (CH)", 0x2,
	  "SUBLANG_GERMAN_SWISS" },
	{ "el_GR", 0x408, "Greek (el)", 0x8, "LANG_GREEK", "Greece (GR)", 0x1, "SUBLANG_GREEK_GREECE" },
	{ "kl_GL", 0x46f, "Greenlandic (kl)", 0x6f, "LANG_GREENLANDIC", "Greenland (GL)", 0x1,
	  "SUBLANG_GREENLANDIC_GREENLAND" },
	{ "gu_IN", 0x447, "Gujarati (gu)", 0x47, "LANG_GUJARATI", "India (IN)", 0x1,
	  "SUBLANG_GUJARATI_INDIA" },
	{ "ha_NG", 0x468, "Hausa (ha)", 0x68, "LANG_HAUSA", "Nigeria (NG)", 0x1,
	  "SUBLANG_HAUSA_NIGERIA_LATIN" },
	{ "haw_US", 0x475, "Hawiian (haw)", 0x75, "LANG_HAWAIIAN", "United States (US)", 0x1,
	  "SUBLANG_HAWAIIAN_US" },
	{ "he_IL", 0x40d, "Hebrew (he)", 0xd, "LANG_HEBREW", "Israel (IL)", 0x1,
	  "SUBLANG_HEBREW_ISRAEL" },
	{ "hi_IN", 0x439, "Hindi (hi)", 0x39, "LANG_HINDI", "India (IN)", 0x1, "SUBLANG_HINDI_INDIA" },
	{ "hu_HU", 0x40e, "Hungarian (hu)", 0xe, "LANG_HUNGARIAN", "Hungary (HU)", 0x1,
	  "SUBLANG_HUNGARIAN_HUNGARY" },
	{ "is_IS", 0x40f, "Icelandic (is)", 0xf, "LANG_ICELANDIC", "Iceland (IS)", 0x1,
	  "SUBLANG_ICELANDIC_ICELAND" },
	{ "ig_NG", 0x470, "Igbo (ig)", 0x70, "LANG_IGBO", "Nigeria (NG)", 0x1, "SUBLANG_IGBO_NIGERIA" },
	{ "id_ID", 0x421, "Indonesian (id)", 0x21, "LANG_INDONESIAN", "Indonesia (ID)", 0x1,
	  "SUBLANG_INDONESIAN_INDONESIA" },
	{ "iu_CA", 0x85d, "Inuktitut (iu)", 0x5d, "LANG_INUKTITUT", "Canada (CA), Latin", 0x2,
	  "SUBLANG_INUKTITUT_CANADA_LATIN" },
	{ "iu_CA", 0x45d, "Inuktitut (iu)", 0x5d, "LANG_INUKTITUT", "Canada (CA), Canadian Syllabics",
	  0x1, "SUBLANG_INUKTITUT_CANADA" },
	{ "ga_IE", 0x83c, "Irish (ga)", 0x3c, "LANG_IRISH", "Ireland (IE)", 0x2,
	  "SUBLANG_IRISH_IRELAND" },
	{ "xh_ZA", 0x434, "isiXhosa (xh)", 0x34, "LANG_XHOSA", "South Africa (ZA)", 0x1,
	  "SUBLANG_XHOSA_SOUTH_AFRICA" },
	{ "zu_ZA", 0x435, "isiZulu (zu)", 0x35, "LANG_ZULU", "South Africa (ZA)", 0x1,
	  "SUBLANG_ZULU_SOUTH_AFRICA" },
	{ "it_IT", 0x410, "Italian (it)", 0x10, "LANG_ITALIAN", "Italy (IT)", 0x1, "SUBLANG_ITALIAN" },
	{ "it_CH", 0x810, "Italian (it)", 0x10, "LANG_ITALIAN", "Switzerland (CH)", 0x2,
	  "SUBLANG_ITALIAN_SWISS" },
	{ "ja_JP", 0x411, "Japanese (ja)", 0x11, "LANG_JAPANESE", "Japan (JP)", 0x1,
	  "SUBLANG_JAPANESE_JAPAN" },
	{ "kn_IN", 0x44b, "Kannada (kn)", 0x4b, "LANG_KANNADA", "India (IN)", 0x1,
	  "SUBLANG_KANNADA_INDIA" },
	{ "kk_KZ", 0x43f, "Kazakh (kk)", 0x3f, "LANG_KAZAK", "Kazakhstan (KZ)", 0x1,
	  "SUBLANG_KAZAK_KAZAKHSTAN" },
	{ "kh_KH", 0x453, "Khmer (kh)", 0x53, "LANG_KHMER", "Cambodia (KH)", 0x1,
	  "SUBLANG_KHMER_CAMBODIA" },
	{ "qut_GT", 0x486, "K'iche (qut)", 0x86, "LANG_KICHE", "Guatemala (GT)", 0x1,
	  "SUBLANG_KICHE_GUATEMALA" },
	{ "rw_RW", 0x487, "Kinyarwanda (rw)", 0x87, "LANG_KINYARWANDA", "Rwanda (RW)", 0x1,
	  "SUBLANG_KINYARWANDA_RWANDA" },
	{ "kok_IN", 0x457, "Konkani (kok)", 0x57, "LANG_KONKANI", "India (IN)", 0x1,
	  "SUBLANG_KONKANI_INDIA" },
	{ "ko_KR", 0x412, "Korean (ko)", 0x12, "LANG_KOREAN", "Korea (KR)", 0x1, "SUBLANG_KOREAN" },
	{ "ky_KG", 0x440, "Kyrgyz (ky)", 0x40, "LANG_KYRGYZ", "Kyrgyzstan (KG)", 0x1,
	  "SUBLANG_KYRGYZ_KYRGYZSTAN" },
	{ "lo_LA", 0x454, "Lao (lo)", 0x54, "LANG_LAO", "Lao PDR (LA)", 0x1, "SUBLANG_LAO_LAO" },
	{ "lv_LV", 0x426, "Latvian (lv)", 0x26, "LANG_LATVIAN", "Latvia (LV)", 0x1,
	  "SUBLANG_LATVIAN_LATVIA" },
	{ "lt_LT", 0x427, "Lithuanian (lt)", 0x27, "LANG_LITHUANIAN", "Lithuanian (LT)", 0x1,
	  "SUBLANG_LITHUANIAN_LITHUANIA" },
	{ "dsb_DE", 0x82e, "Lower Sorbian (dsb)", 0x2e, "LANG_LOWER_SORBIAN", "Germany (DE)", 0x2,
	  "SUBLANG_LOWER_SORBIAN_GERMANY" },
	{ "lb_LU", 0x46e, "Luxembourgish (lb)", 0x6e, "LANG_LUXEMBOURGISH", "Luxembourg (LU)", 0x1,
	  "SUBLANG_LUXEMBOURGISH_LUXEMBOURG" },
	{ "mk_MK", 0x42f, "Macedonian (mk)", 0x2f, "LANG_MACEDONIAN", "Macedonia (FYROM) (MK)", 0x1,
	  "SUBLANG_MACEDONIAN_MACEDONIA" },
	{ "ms_BN", 0x83e, "Malay (ms)", 0x3e, "LANG_MALAY", "Brunei Darassalam (BN)", 0x2,
	  "SUBLANG_MALAY_BRUNEI_DARUSSALAM" },
	{ "ms_MY", 0x43e, "Malay (ms)", 0x3e, "LANG_MALAY", "Malaysia (MY)", 0x1,
	  "SUBLANG_MALAY_MALAYSIA" },
	{ "ml_IN", 0x44c, "Malayalam (ml)", 0x4c, "LANG_MALAYALAM", "India (IN)", 0x1,
	  "SUBLANG_MALAYALAM_INDIA" },
	{ "mt_MT", 0x43a, "Maltese (mt)", 0x3a, "LANG_MALTESE", "Malta (MT)", 0x1,
	  "SUBLANG_MALTESE_MALTA" },
	{ "mi_NZ", 0x481, "Maori (mi)", 0x81, "LANG_MAORI", "New Zealand (NZ)", 0x1,
	  "SUBLANG_MAORI_NEW_ZEALAND" },
	{ "arn_CL", 0x47a, "Mapudungun (arn)", 0x7a, "LANG_MAPUDUNGUN", "Chile (CL)", 0x1,
	  "SUBLANG_MAPUDUNGUN_CHILE" },
	{ "mr_IN", 0x44e, "Marathi (mr)", 0x4e, "LANG_MARATHI", "India (IN)", 0x1,
	  "SUBLANG_MARATHI_INDIA" },
	{ "moh_CA", 0x47c, "Mohawk (moh)", 0x7c, "LANG_MOHAWK", "Canada (CA)", 0x1,
	  "SUBLANG_MOHAWK_MOHAWK" },
	{ "mn_MN", 0x450, "Mongolian (mn)", 0x50, "LANG_MONGOLIAN", "Mongolia, Cyrillic (MN)", 0x1,
	  "SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA" },
	{ "mn_MN", 0x850, "Mongolian (mn)", 0x50, "LANG_MONGOLIAN", "Mongolia, Mong (MN)", 0x2,
	  "SUBLANG_MONGOLIAN_PRC" },
	{ "ne_NP", 0x461, "Nepali (ne)", 0x61, "LANG_NEPALI", "Nepal (NP)", 0x1,
	  "SUBLANG_NEPALI_NEPAL" },
	{ "ne_IN", 0x861, "Nepali (ne)", 0x61, "LANG_NEPALI", "India (IN)", 0x2,
	  "SUBLANG_NEPALI_INDIA" },
	{ "no_NO", 0x414, "Norwegian (no)", 0x14, "LANG_NORWEGIAN", "Bokmål, Norway (NO)", 0x1,
	  "SUBLANG_NORWEGIAN_BOKMAL" },
	{ "no_NO", 0x814, "Norwegian (no)", 0x14, "LANG_NORWEGIAN", "Nynorsk, Norway (NO)", 0x2,
	  "SUBLANG_NORWEGIAN_NYNORSK" },
	{ "oc_FR", 0x482, "Occitan (oc)", 0x82, "LANG_OCCITAN", "France (FR)", 0x1,
	  "SUBLANG_OCCITAN_FRANCE" },
	{ "or_IN", 0x448, "Odia (or)", 0x48, "LANG_ORIYA", "India (IN)", 0x1, "SUBLANG_ORIYA_INDIA" },
	{ "ps_AF", 0x463, "Pashto (ps)", 0x63, "LANG_PASHTO", "Afghanistan (AF)", 0x1,
	  "SUBLANG_PASHTO_AFGHANISTAN" },
	{ "fa_IR", 0x429, "Persian (fa)", 0x29, "LANG_PERSIAN", "Iran (IR)", 0x1,
	  "SUBLANG_PERSIAN_IRAN" },
	{ "pl_PL", 0x415, "Polish (pl)", 0x15, "LANG_POLISH", "Poland (PL)", 0x1,
	  "SUBLANG_POLISH_POLAND" },
	{ "pt_BR", 0x416, "Portuguese (pt)", 0x16, "LANG_PORTUGUESE", "Brazil (BR)", 0x1,
	  "SUBLANG_PORTUGUESE_BRAZILIAN" },
	{ "pt_PT", 0x816, "Portuguese (pt)", 0x16, "LANG_PORTUGUESE", "Portugal (PT)", 0x2,
	  "SUBLANG_PORTUGUESE" },
	{ "ff_SN", 0x867, "Pular (ff)", 0x67, "LANG_PULAR", "Senegal (SN)", 0x2,
	  "SUBLANG_PULAR_SENEGAL" },
	{ "pa_IN", 0x446, "Punjabi (pa)", 0x46, "LANG_PUNJABI", "India, Gurmukhi script (IN)", 0x1,
	  "SUBLANG_PUNJABI_INDIA" },
	{ "pa_PK", 0x846, "Punjabi (pa)", 0x46, "LANG_PUNJABI", "Pakistan, Arabic script(PK)", 0x2,
	  "SUBLANG_PUNJABI_PAKISTAN" },
	{ "quz_BO", 0x46b, "Quechua (quz)", 0x6b, "LANG_QUECHUA", "Bolivia (BO)", 0x1,
	  "SUBLANG_QUECHUA_BOLIVIA" },
	{ "quz_EC", 0x86b, "Quechua (quz)", 0x6b, "LANG_QUECHUA", "Ecuador (EC)", 0x2,
	  "SUBLANG_QUECHUA_ECUADOR" },
	{ "quz_PE", 0xc6b, "Quechua (quz)", 0x6b, "LANG_QUECHUA", "Peru (PE)", 0x3,
	  "SUBLANG_QUECHUA_PERU" },
	{ "ro_RO", 0x418, "Romanian (ro)", 0x18, "LANG_ROMANIAN", "Romania (RO)", 0x1,
	  "SUBLANG_ROMANIAN_ROMANIA" },
	{ "rm_CH", 0x417, "Romansh (rm)", 0x17, "LANG_ROMANSH", "Switzerland (CH)", 0x1,
	  "SUBLANG_ROMANSH_SWITZERLAND" },
	{ "ru_RU", 0x419, "Russian (ru)", 0x19, "LANG_RUSSIAN", "Russia (RU)", 0x1,
	  "SUBLANG_RUSSIAN_RUSSIA" },
	{ "sah_RU", 0x485, "Sakha (sah)", 0x85, "LANG_SAKHA", "Russia (RU)", 0x1,
	  "SUBLANG_SAKHA_RUSSIA" },
	{ "smn_FI", 0x243b, "Sami (smn)", 0x3b, "LANG_SAMI", "Inari, Finland (FI)", 0x9,
	  "SUBLANG_SAMI_INARI_FINLAND" },
	{ "smj_NO", 0x103b, "Sami (smj)", 0x3b, "LANG_SAMI", "Lule, Norway (NO)", 0x4,
	  "SUBLANG_SAMI_LULE_NORWAY" },
	{ "smj_SE", 0x143b, "Sami (smj)", 0x3b, "LANG_SAMI", "Lule, Sweden (SE)", 0x5,
	  "SUBLANG_SAMI_LULE_SWEDEN" },
	{ "se_FI", 0xc3b, "Sami (se)", 0x3b, "LANG_SAMI", "Northern, Finland (FI)", 0x3,
	  "SUBLANG_SAMI_NORTHERN_FINLAND" },
	{ "se_NO", 0x43b, "Sami (se)", 0x3b, "LANG_SAMI", "Northern, Norway (NO)", 0x1,
	  "SUBLANG_SAMI_NORTHERN_NORWAY" },
	{ "se_SE", 0x83b, "Sami (se)", 0x3b, "LANG_SAMI", "Northern, Sweden (SE)", 0x2,
	  "SUBLANG_SAMI_NORTHERN_SWEDEN" },
	{ "sms_FI", 0x203b, "Sami (sms)", 0x3b, "LANG_SAMI", "Skolt, Finland (FI)", 0x8,
	  "SUBLANG_SAMI_SKOLT_FINLAND" },
	{ "sma_NO", 0x183b, "Sami (sma)", 0x3b, "LANG_SAMI", "Southern, Norway (NO)", 0x6,
	  "SUBLANG_SAMI_SOUTHERN_NORWAY" },
	{ "sma_SE", 0x1c3b, "Sami (sma)", 0x3b, "LANG_SAMI", "Southern, Sweden (SE)", 0x7,
	  "SUBLANG_SAMI_SOUTHERN_SWEDEN" },
	{ "sa_IN", 0x44f, "Sanskrit (sa)", 0x4f, "LANG_SANSKRIT", "India (IN)", 0x1,
	  "SUBLANG_SANSKRIT_INDIA" },
	{ "sr", 0x7c1a, "Serbian (sr)", 0x1a, "LANG_SERBIAN_NEUTRAL", "Neutral", 0x00, "" },
	{ "sr_BA", 0x1c1a, "Serbian (sr)", 0x1a, "LANG_SERBIAN",
	  "Bosnia and Herzegovina, Cyrillic (BA)", 0x7, "SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_CYRILLIC" },
	{ "sr_BA", 0x181a, "Serbian (sr)", 0x1a, "LANG_SERBIAN", "Bosnia and Herzegovina, Latin (BA)",
	  0x6, "SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_LATIN" },
	{ "sr_HR", 0x41a, "Serbian (sr)", 0x1a, "LANG_SERBIAN", "Croatia (HR)", 0x1,
	  "SUBLANG_SERBIAN_CROATIA" },
	{ "sr_CS", 0xc1a, "Serbian (sr)", 0x1a, "LANG_SERBIAN",
	  "Serbia and Montenegro (former), Cyrillic (CS)", 0x3, "SUBLANG_SERBIAN_CYRILLIC" },
	{ "sr_CS", 0x81a, "Serbian (sr)", 0x1a, "LANG_SERBIAN",
	  "Serbia and Montenegro (former), Latin (CS)", 0x2, "SUBLANG_SERBIAN_LATIN" },
	{ "nso_ZA", 0x46c, "Sesotho sa Leboa (nso)", 0x6c, "LANG_SOTHO", "South Africa (ZA)", 0x1,
	  "SUBLANG_SOTHO_NORTHERN_SOUTH_AFRICA" },
	{ "tn_BW", 0x832, "Setswana / Tswana (tn)", 0x32, "LANG_TSWANA", "Botswana (BW)", 0x2,
	  "SUBLANG_TSWANA_BOTSWANA" },
	{ "tn_ZA", 0x432, "Setswana / Tswana (tn)", 0x32, "LANG_TSWANA", "South Africa (ZA)", 0x1,
	  "SUBLANG_TSWANA_SOUTH_AFRICA" },
	{ "", 0x859, "(reserved)", 0x59, "LANG_SINDHI", "(reserved)", 0x2,
	  "SUBLANG_SINDHI_AFGHANISTAN" },
	{ "", 0x459, "(reserved)", 0x59, "LANG_SINDHI", "(reserved)", 0x1, "SUBLANG_SINDHI_INDIA" },
	{ "sd_PK", 0x859, "Sindhi (sd)", 0x59, "LANG_SINDHI", "Pakistan (PK)", 0x2,
	  "SUBLANG_SINDHI_PAKISTAN" },
	{ "si_LK", 0x45b, "Sinhala (si)", 0x5b, "LANG_SINHALESE", "Sri Lanka (LK)", 0x1,
	  "SUBLANG_SINHALESE_SRI_LANKA" },
	{ "sk_SK", 0x41b, "Slovak (sk)", 0x1b, "LANG_SLOVAK", "Slovakia (SK)", 0x1,
	  "SUBLANG_SLOVAK_SLOVAKIA" },
	{ "sl_SI", 0x424, "Slovenian (sl)", 0x24, "LANG_SLOVENIAN", "Slovenia (SI)", 0x1,
	  "SUBLANG_SLOVENIAN_SLOVENIA" },
	{ "es_AR", 0x2c0a, "Spanish (es)", 0xa, "LANG_SPANISH", "Argentina (AR)", 0xb,
	  "SUBLANG_SPANISH_ARGENTINA" },
	{ "es_BO", 0x400a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Bolivia (BO)", 0x10,
	  "SUBLANG_SPANISH_BOLIVIA" },
	{ "es_CL", 0x340a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Chile (CL)", 0xd,
	  "SUBLANG_SPANISH_CHILE" },
	{ "es_CO", 0x240a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Colombia (CO)", 0x9,
	  "SUBLANG_SPANISH_COLOMBIA" },
	{ "es_CR", 0x140a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Costa Rica (CR)", 0x5,
	  "SUBLANG_SPANISH_COSTA_RICA" },
	{ "es_DO", 0x1c0a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Dominican Republic (DO)", 0x7,
	  "SUBLANG_SPANISH_DOMINICAN_REPUBLIC" },
	{ "es_EC", 0x300a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Ecuador (EC)", 0xc,
	  "SUBLANG_SPANISH_ECUADOR" },
	{ "es_SV", 0x440a, "Spanish (es)", 0x0a, "LANG_SPANISH", "El Salvador (SV)", 0x11,
	  "SUBLANG_SPANISH_EL_SALVADOR" },
	{ "es_GT", 0x100a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Guatemala (GT)", 0x4,
	  "SUBLANG_SPANISH_GUATEMALA" },
	{ "es_HN", 0x480a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Honduras (HN)", 0x12,
	  "SUBLANG_SPANISH_HONDURAS" },
	{ "es_MX", 0x80a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Mexico (MX)", 0x2,
	  "SUBLANG_SPANISH_MEXICAN" },
	{ "es_NI", 0x4c0a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Nicaragua (NI)", 0x13,
	  "SUBLANG_SPANISH_NICARAGUA" },
	{ "es_PA", 0x180a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Panama (PA)", 0x6,
	  "SUBLANG_SPANISH_PANAMA" },
	{ "es_PY", 0x3c0a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Paraguay (PY)", 0xf,
	  "SUBLANG_SPANISH_PARAGUAY" },
	{ "es_PE", 0x280a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Peru (PE)", 0xa,
	  "SUBLANG_SPANISH_PERU" },
	{ "es_PR", 0x500a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Puerto Rico (PR)", 0x14,
	  "SUBLANG_SPANISH_PUERTO_RICO" },
	{ "es_ES", 0xc0a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Spain, Modern Sort (ES)", 0x3,
	  "SUBLANG_SPANISH_MODERN" },
	{ "es_ES", 0x40a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Spain, Traditional Sort (ES)", 0x1,
	  "SUBLANG_SPANISH" },
	{ "es_US", 0x540a, "Spanish (es)", 0x0a, "LANG_SPANISH", "United States (US)", 0x15,
	  "SUBLANG_SPANISH_US" },
	{ "es_UY", 0x380a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Uruguay (UY)", 0xe,
	  "SUBLANG_SPANISH_URUGUAY" },
	{ "es_VE", 0x200a, "Spanish (es)", 0x0a, "LANG_SPANISH", "Venezuela (VE)", 0x8,
	  "SUBLANG_SPANISH_VENEZUELA" },
	{ "sw_KE", 0x441, "Swahili (sw)", 0x41, "LANG_SWAHILI", "Kenya (KE)", 0x1, "SUBLANG_SWAHILI" },
	{ "sv_FI", 0x81d, "Swedish (sv)", 0x1d, "LANG_SWEDISH", "Finland (FI)", 0x2,
	  "SUBLANG_SWEDISH_FINLAND" },
	{ "sv_SE", 0x41d, "Swedish (sv)", 0x1d, "LANG_SWEDISH", "Sweden (SE);", 0x1,
	  "SUBLANG_SWEDISH" },
	{ "sv_SE", 0x41d, "Swedish (sv)", 0x1d, "LANG_SWEDISH", "Sweden (SE);", 0x1,
	  "SUBLANG_SWEDISH_SWEDEN" },
	{ "syr_SY", 0x45a, "Syriac (syr)", 0x5a, "LANG_SYRIAC", "Syria (SY)", 0x1, "SUBLANG_SYRIAC" },
	{ "tg_TJ", 0x428, "Tajik (tg)", 0x28, "LANG_TAJIK", "Tajikistan, Cyrillic (TJ)", 0x1,
	  "SUBLANG_TAJIK_TAJIKISTAN" },
	{ "tzm_DZ", 0x85f, "Tamazight (tzm)", 0x5f, "LANG_TAMAZIGHT", "Algeria, Latin (DZ)", 0x2,
	  "SUBLANG_TAMAZIGHT_ALGERIA_LATIN" },
	{ "ta_IN", 0x449, "Tamil (ta)", 0x49, "LANG_TAMIL", "India (IN)", 0x1, "SUBLANG_TAMIL_INDIA" },
	{ "ta_LK", 0x849, "Tamil (ta)", 0x49, "LANG_TAMIL", "Sri Lanka (LK)", 0x2,
	  "SUBLANG_TAMIL_SRI_LANKA" },
	{ "tt_RU", 0x444, "Tatar (tt)", 0x44, "LANG_TATAR", "Russia (RU)", 0x1,
	  "SUBLANG_TATAR_RUSSIA" },
	{ "te_IN", 0x44a, "Telugu (te)", 0x4a, "LANG_TELUGU", "India (IN)", 0x1,
	  "SUBLANG_TELUGU_INDIA" },
	{ "th_TH", 0x41e, "Thai (th)", 0x1e, "LANG_THAI", "Thailand (TH)", 0x1,
	  "SUBLANG_THAI_THAILAND" },
	{ "bo_CN", 0x451, "Tibetan (bo)", 0x51, "LANG_TIBETAN", "PRC (CN)", 0x1,
	  "SUBLANG_TIBETAN_PRC" },
	{ "ti_ER", 0x873, "Tigrinya (ti)", 0x73, "LANG_TIGRINYA", "Eritrea (ER)", 0x2,
	  "SUBLANG_TIGRINYA_ERITREA" },
	{ "ti_ET", 0x473, "Tigrinya (ti)", 0x73, "LANG_TIGRIGNA", "Ethiopia (ET)", 0x1,
	  "SUBLANG_TIGRINYA_ETHIOPIA" },
	{ "tr_TR", 0x41f, "Turkish (tr)", 0x1f, "LANG_TURKISH", "Turkey (TR)", 0x1,
	  "SUBLANG_TURKISH_TURKEY" },
	{ "tk_KM", 0x442, "Turkmen (tk)", 0x42, "LANG_TURKMEN", "Turkmenistan (TM)", 0x1,
	  "SUBLANG_TURKMEN_TURKMENISTAN" },
	{ "uk_UA", 0x422, "Ukrainian (uk)", 0x22, "LANG_UKRAINIAN", "Ukraine (UA)", 0x1,
	  "SUBLANG_UKRAINIAN_UKRAINE" },
	{ "hsb_DE", 0x42e, "Upper Sorbian (hsb)", 0x2e, "LANG_UPPER_SORBIAN", "Germany (DE)", 0x1,
	  "SUBLANG_UPPER_SORBIAN_GERMANY" },
	{ "ur", 0x820, "Urdu (ur)", 0x20, "LANG_URDU", "(reserved)", 0x2, "SUBLANG_URDU_INDIA" },
	{ "ur_PK", 0x420, "Urdu (ur)", 0x20, "LANG_URDU", "Pakistan (PK)", 0x1,
	  "SUBLANG_URDU_PAKISTAN" },
	{ "ug_CN", 0x480, "Uyghur (ug)", 0x80, "LANG_UIGHUR", "PRC (CN)", 0x1, "SUBLANG_UIGHUR_PRC" },
	{ "uz_UZ", 0x843, "Uzbek (uz)", 0x43, "LANG_UZBEK", "Uzbekistan, Cyrillic (UZ)", 0x2,
	  "SUBLANG_UZBEK_CYRILLIC" },
	{ "uz_UZ", 0x443, "Uzbek (uz)", 0x43, "LANG_UZBEK", "Uzbekistan, Latin (UZ)", 0x1,
	  "SUBLANG_UZBEK_LATIN" },
	{ "ca", 0x803, "Valencian (ca)", 0x3, "LANG_VALENCIAN", "Valencia (ES-Valencia)", 0x2,
	  "SUBLANG_VALENCIAN_VALENCIA" },
	{ "vi_VN", 0x42a, "Vietnamese (vi)", 0x2a, "LANG_VIETNAMESE", "Vietnam (VN)", 0x1,
	  "SUBLANG_VIETNAMESE_VIETNAM" },
	{ "cy_GB", 0x452, "Welsh (cy)", 0x52, "LANG_WELSH", "United Kingdom (GB)", 0x1,
	  "SUBLANG_WELSH_UNITED_KINGDOM" },
	{ "wo_SN", 0x488, "Wolof (wo)", 0x88, "LANG_WOLOF", "Senegal (SN)", 0x1,
	  "SUBLANG_WOLOF_SENEGAL" },
	{ "ii_CN", 0x478, "Yi (ii)", 0x78, "LANG_YI", "PRC (CN)", 0x1, "SUBLANG_YI_PRC" },
	{ "yu_NG", 0x46a, "Yoruba (yo)", 0x6a, "LANG_YORUBA", "Nigeria (NG)", 0x1,
	  "SUBLANG_YORUBA_NIGERIA" },
};

/*
 * In Windows XP, this information is available in the system registry at
 * HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet001/Control/Keyboard Layouts/
 * https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-language-pack-default-values
 */
static const RDP_KEYBOARD_LAYOUT RDP_KEYBOARD_LAYOUT_TABLE[] = {
	{ 0x0000041c, "Albanian" },
	{ 0x00000401, "Arabic (101)" },
	{ 0x00010401, "Arabic (102)" },
	{ 0x00020401, "Arabic (102) AZERTY" },
	{ 0x0000042b, "Armenian Eastern" },
	{ 0x0002042b, "Armenian Phonetic" },
	{ 0x0003042b, "Armenian Typewriter" },
	{ 0x0001042b, "Armenian Western" },
	{ 0x0000044d, "Assamese - Inscript" },
	{ 0x0001042c, "Azerbaijani (Standard)" },
	{ 0x0000082c, "Azerbaijani Cyrillic" },
	{ 0x0000042c, "Azerbaijani Latin" },
	{ 0x0000046d, "Bashkir" },
	{ 0x00000423, "Belarusian" },
	{ 0x0001080c, "Belgian (Comma)" },
	{ 0x00000813, "Belgian (Period)" },
	{ 0x0000080c, "Belgian French" },
	{ 0x00000445, "Bangla (Bangladesh)" },
	{ 0x00020445, "Bangla (India)" },
	{ 0x00010445, "Bangla (India - Legacy)" },
	{ 0x0000201a, "Bosnian (Cyrillic)" },
	{ 0x000b0c00, "Buginese" },
	{ 0x00030402, "Bulgarian" },
	{ 0x00010402, "Bulgarian (Latin)" },
	{ 0x00020402, "Bulgarian (phonetic layout)" },
	{ 0x00040402, "Bulgarian (phonetic traditional)" },
	{ 0x00000402, "Bulgarian (Typewriter)" },
	{ 0x00001009, "Canadian French" },
	{ 0x00000c0c, "Canadian French (Legacy)" },
	{ 0x00011009, "Canadian Multilingual Standard" },
	{ 0x0000085f, "Central Atlas Tamazight" },
	{ 0x00000429, "Central Kurdish" },
	{ 0x0000045c, "Cherokee Nation" },
	{ 0x0001045c, "Cherokee Nation Phonetic" },
	{ 0x00000804, "Chinese (Simplified) - US Keyboard" },
	{ 0x00000404, "Chinese (Traditional) - US Keyboard" },
	{ 0x00000c04, "Chinese (Traditional, Hong Kong S.A.R.)" },
	{ 0x00001404, "Chinese (Traditional Macao S.A.R.) US Keyboard" },
	{ 0x00001004, "Chinese (Simplified, Singapore) - US keyboard" },
	{ 0x0000041a, "Croatian" },
	{ 0x00000405, "Czech" },
	{ 0x00010405, "Czech (QWERTY)" },
	{ 0x00020405, "Czech Programmers" },
	{ 0x00000406, "Danish" },
	{ 0x00000439, "Devanagari-INSCRIPT" },
	{ 0x00000465, "Divehi Phonetic" },
	{ 0x00010465, "Divehi Typewriter" },
	{ 0x00000413, "Dutch" },
	{ 0x00000c51, "Dzongkha" },
	{ 0x00000425, "Estonian" },
	{ 0x00000438, "Faeroese" },
	{ 0x0000040b, "Finnish" },
	{ 0x0001083b, "Finnish with Sami" },
	{ 0x0000040c, "French" },
	{ 0x00120c00, "Futhark" },
	{ 0x00000437, "Georgian" },
	{ 0x00020437, "Georgian (Ergonomic)" },
	{ 0x00010437, "Georgian (QWERTY)" },
	{ 0x00030437, "Georgian Ministry of Education and Science Schools" },
	{ 0x00040437, "Georgian (Old Alphabets)" },
	{ 0x00000407, "German" },
	{ 0x00010407, "German (IBM)" },
	{ 0x000c0c00, "Gothic" },
	{ 0x00000408, "Greek" },
	{ 0x00010408, "Greek (220)" },
	{ 0x00030408, "Greek (220) Latin" },
	{ 0x00020408, "Greek (319)" },
	{ 0x00040408, "Greek (319) Latin" },
	{ 0x00050408, "Greek Latin" },
	{ 0x00060408, "Greek Polytonic" },
	{ 0x0000046f, "Greenlandic" },
	{ 0x00000474, "Guarani" },
	{ 0x00000447, "Gujarati" },
	{ 0x00000468, "Hausa" },
	{ 0x0000040d, "Hebrew" },
	{ 0x00010439, "Hindi Traditional" },
	{ 0x0000040e, "Hungarian" },
	{ 0x0001040e, "Hungarian 101-key" },
	{ 0x0000040f, "Icelandic" },
	{ 0x00000470, "Igbo" },
	{ 0x00004009, "India" },
	{ 0x0000085d, "Inuktitut - Latin" },
	{ 0x0001045d, "Inuktitut - Naqittaut" },
	{ 0x00001809, "Irish" },
	{ 0x00000410, "Italian" },
	{ 0x00010410, "Italian (142)" },
	{ 0x00000411, "Japanese" },
	{ 0x00110c00, "Javanese" },
	{ 0x0000044b, "Kannada" },
	{ 0x0000043f, "Kazakh" },
	{ 0x00000453, "Khmer" },
	{ 0x00010453, "Khmer (NIDA)" },
	{ 0x00000412, "Korean" },
	{ 0x00000440, "Kyrgyz Cyrillic" },
	{ 0x00000454, "Lao" },
	{ 0x0000080a, "Latin American" },
	{ 0x00020426, "Latvian (Standard)" },
	{ 0x00010426, "Latvian (Legacy)" },
	{ 0x00070c00, "Lisu (Basic)" },
	{ 0x00080c00, "Lisu (Standard)" },
	{ 0x00010427, "Lithuanian" },
	{ 0x00000427, "Lithuanian IBM" },
	{ 0x00020427, "Lithuanian Standard" },
	{ 0x0000046e, "Luxembourgish" },
	{ 0x0000042f, "Macedonia (FYROM)" },
	{ 0x0001042f, "Macedonia (FYROM) - Standard" },
	{ 0x0000044c, "Malayalam" },
	{ 0x0000043a, "Maltese 47-Key" },
	{ 0x0001043a, "Maltese 48-key" },
	{ 0x00000481, "Maori" },
	{ 0x0000044e, "Marathi" },
	{ 0x00000850, "Mongolian (Mongolian Script - Legacy)" },
	{ 0x00020850, "Mongolian (Mongolian Script - Standard)" },
	{ 0x00000450, "Mongolian Cyrillic" },
	{ 0x00010c00, "Myanmar" },
	{ 0x00090c00, "N'ko" },
	{ 0x00000461, "Nepali" },
	{ 0x00020c00, "New Tai Lue" },
	{ 0x00000414, "Norwegian" },
	{ 0x0000043b, "Norwegian with Sami" },
	{ 0x00000448, "Odia" },
	{ 0x000d0c00, "Ol Chiki" },
	{ 0x000f0c00, "Old Italic" },
	{ 0x000e0c00, "Osmanya" },
	{ 0x00000463, "Pashto (Afghanistan)" },
	{ 0x00000429, "Persian" },
	{ 0x00050429, "Persian (Standard)" },
	{ 0x000a0c00, "Phags-pa" },
	{ 0x00010415, "Polish (214)" },
	{ 0x00000415, "Polish (Programmers)" },
	{ 0x00000816, "Portuguese" },
	{ 0x00000416, "Portuguese (Brazilian ABNT)" },
	{ 0x00010416, "Portuguese (Brazilian ABNT2)" },
	{ 0x00000446, "Punjabi" },
	{ 0x00000418, "Romanian (Legacy)" },
	{ 0x00020418, "Romanian (Programmers)" },
	{ 0x00010418, "Romanian (Standard)" },
	{ 0x00000419, "Russian" },
	{ 0x00020419, "Russian - Mnemonic" },
	{ 0x00010419, "Russian (Typewriter)" },
	{ 0x00000485, "Sakha" },
	{ 0x0002083b, "Sami Extended Finland-Sweden" },
	{ 0x0001043b, "Sami Extended Norway" },
	{ 0x00011809, "Scottish Gaelic" },
	{ 0x00000c1a, "Serbian (Cyrillic)" },
	{ 0x0000081a, "Serbian (Latin)" },
	{ 0x0000046c, "Sesotho sa Leboa" },
	{ 0x00000432, "Setswana" },
	{ 0x0000045b, "Sinhala" },
	{ 0x0001045b, "Sinhala - wij 9" },
	{ 0x0000041b, "Slovak" },
	{ 0x0001041b, "Slovak (QWERTY)" },
	{ 0x00000424, "Slovenian" },
	{ 0x00100c00, "Sora" },
	{ 0x0001042e, "Sorbian Extended" },
	{ 0x0002042e, "Sorbian Standard" },
	{ 0x0000042e, "Sorbian Standard (Legacy)" },
	{ 0x0000040a, "Spanish" },
	{ 0x0001040a, "Spanish Variation" },
	{ 0x0000041d, "Swedish" },
	{ 0x0000083b, "Swedish with Sami" },
	{ 0x0000100c, "Swiss French" },
	{ 0x00000807, "Swiss German" },
	{ 0x0000045a, "Syriac" },
	{ 0x0001045a, "Syriac Phonetic" },
	{ 0x00030c00, "Tai Le" },
	{ 0x00000428, "Tajik" },
	{ 0x00000449, "Tamil" },
	{ 0x00010444, "Tatar" },
	{ 0x00000444, "Tatar (Legacy)" },
	{ 0x0000044a, "Telugu" },
	{ 0x0000041e, "Thai Kedmanee" },
	{ 0x0002041e, "Thai Kedmanee (non-ShiftLock)" },
	{ 0x0001041e, "Thai Pattachote" },
	{ 0x0003041e, "Thai Pattachote (non-ShiftLock)" },
	{ 0x00010451, "Tibetan (PRC - Standard)" },
	{ 0x00000451, "Tibetan (PRC - Legacy)" },
	{ 0x00050c00, "Tifinagh (Basic)" },
	{ 0x00060c00, "Tifinagh (Full)" },
	{ 0x0001041f, "Turkish F" },
	{ 0x0000041f, "Turkish Q" },
	{ 0x00000442, "Turkmen" },
	{ 0x00010408, "Uyghur" },
	{ 0x00000480, "Uyghur (Legacy)" },
	{ 0x00000422, "Ukrainian" },
	{ 0x00020422, "Ukrainian (Enhanced)" },
	{ 0x00000809, "United Kingdom" },
	{ 0x00000452, "United Kingdom Extended" },
	{ 0x00010409, "United States - Dvorak" },
	{ 0x00020409, "United States - International" },
	{ 0x00030409, "United States-Dvorak for left hand" },
	{ 0x00040409, "United States-Dvorak for right hand" },
	{ 0x00000409, "United States - English" },
	{ 0x00000420, "Urdu" },
	{ 0x00010480, "Uyghur" },
	{ 0x00000843, "Uzbek Cyrillic" },
	{ 0x0000042a, "Vietnamese" },
	{ 0x00000488, "Wolof" },
	{ 0x00000485, "Yakut" },
	{ 0x0000046a, "Yoruba" },
};

struct _RDP_KEYBOARD_LAYOUT_VARIANT
{
	DWORD code;       /* Keyboard layout code */
	DWORD id;         /* Keyboard variant ID */
	const char* name; /* Keyboard layout variant name */
};
typedef struct _RDP_KEYBOARD_LAYOUT_VARIANT RDP_KEYBOARD_LAYOUT_VARIANT;

static const RDP_KEYBOARD_LAYOUT_VARIANT RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[] = {
	{ KBD_ARABIC_102, 0x0028, "Arabic (102)" },
	{ KBD_BULGARIAN_LATIN, 0x0004, "Bulgarian (Latin)" },
	{ KBD_CZECH_QWERTY, 0x0005, "Czech (QWERTY)" },
	{ KBD_GERMAN_IBM, 0x0012, "German (IBM)" },
	{ KBD_GREEK_220, 0x0016, "Greek (220)" },
	{ KBD_UNITED_STATES_DVORAK, 0x0002, "United States-Dvorak" },
	{ KBD_SPANISH_VARIATION, 0x0086, "Spanish Variation" },
	{ KBD_HUNGARIAN_101_KEY, 0x0006, "Hungarian 101-key" },
	{ KBD_ITALIAN_142, 0x0003, "Italian (142)" },
	{ KBD_POLISH_214, 0x0007, "Polish (214)" },
	{ KBD_PORTUGUESE_BRAZILIAN_ABNT2, 0x001D, "Portuguese (Brazilian ABNT2)" },
	{ KBD_RUSSIAN_TYPEWRITER, 0x0008, "Russian (Typewriter)" },
	{ KBD_SLOVAK_QWERTY, 0x0013, "Slovak (QWERTY)" },
	{ KBD_THAI_PATTACHOTE, 0x0021, "Thai Pattachote" },
	{ KBD_TURKISH_F, 0x0014, "Turkish F" },
	{ KBD_LATVIAN_QWERTY, 0x0015, "Latvian (QWERTY)" },
	{ KBD_LITHUANIAN, 0x0027, "Lithuanian" },
	{ KBD_ARMENIAN_WESTERN, 0x0025, "Armenian Western" },
	{ KBD_HINDI_TRADITIONAL, 0x000C, "Hindi Traditional" },
	{ KBD_MALTESE_48_KEY, 0x002B, "Maltese 48-key" },
	{ KBD_SAMI_EXTENDED_NORWAY, 0x002C, "Sami Extended Norway" },
	{ KBD_BENGALI_INSCRIPT, 0x002A, "Bengali (Inscript)" },
	{ KBD_SYRIAC_PHONETIC, 0x000E, "Syriac Phonetic" },
	{ KBD_DIVEHI_TYPEWRITER, 0x000D, "Divehi Typewriter" },
	{ KBD_BELGIAN_COMMA, 0x001E, "Belgian (Comma)" },
	{ KBD_FINNISH_WITH_SAMI, 0x002D, "Finnish with Sami" },
	{ KBD_CANADIAN_MULTILINGUAL_STANDARD, 0x0020, "Canadian Multilingual Standard" },
	{ KBD_GAELIC, 0x0026, "Gaelic" },
	{ KBD_ARABIC_102_AZERTY, 0x0029, "Arabic (102) AZERTY" },
	{ KBD_CZECH_PROGRAMMERS, 0x000A, "Czech Programmers" },
	{ KBD_GREEK_319, 0x0018, "Greek (319)" },
	{ KBD_UNITED_STATES_INTERNATIONAL, 0x0001, "United States-International" },
	{ KBD_THAI_KEDMANEE_NON_SHIFTLOCK, 0x0022, "Thai Kedmanee (non-ShiftLock)" },
	{ KBD_SAMI_EXTENDED_FINLAND_SWEDEN, 0x002E, "Sami Extended Finland-Sweden" },
	{ KBD_GREEK_220_LATIN, 0x0017, "Greek (220) Latin" },
	{ KBD_UNITED_STATES_DVORAK_FOR_LEFT_HAND, 0x001A, "United States-Dvorak for left hand" },
	{ KBD_THAI_PATTACHOTE_NON_SHIFTLOCK, 0x0023, "Thai Pattachote (non-ShiftLock)" },
	{ KBD_GREEK_319_LATIN, 0x0011, "Greek (319) Latin" },
	{ KBD_UNITED_STATES_DVORAK_FOR_RIGHT_HAND, 0x001B, "United States-Dvorak for right hand" },
	{ KBD_UNITED_STATES_DVORAK_PROGRAMMER, 0x001C, "United States-Programmer Dvorak" },
	{ KBD_GREEK_LATIN, 0x0019, "Greek Latin" },
	{ KBD_US_ENGLISH_TABLE_FOR_IBM_ARABIC_238_L, 0x000B, "US English Table for IBM Arabic 238_L" },
	{ KBD_GREEK_POLYTONIC, 0x001F, "Greek Polytonic" },
	{ KBD_FRENCH_BEPO, 0x00C0, "French Bépo" },
	{ KBD_GERMAN_NEO, 0x00C0, "German Neo" }
};

/* Input Method Editor (IME) */

struct _RDP_KEYBOARD_IME
{
	DWORD code;       /* Keyboard layout code */
	const char* file; /* IME file */
	const char* name; /* Keyboard layout name */
};
typedef struct _RDP_KEYBOARD_IME RDP_KEYBOARD_IME;

/* Global Input Method Editors (IME) */

static const RDP_KEYBOARD_IME RDP_KEYBOARD_IME_TABLE[] = {
	{ KBD_CHINESE_TRADITIONAL_PHONETIC, "phon.ime", "Chinese (Traditional) - Phonetic" },
	{ KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002, "imjp81.ime", "Japanese Input System (MS-IME2002)" },
	{ KBD_KOREAN_INPUT_SYSTEM_IME_2000, "imekr61.ime", "Korean Input System (IME 2000)" },
	{ KBD_CHINESE_SIMPLIFIED_QUANPIN, "winpy.ime", "Chinese (Simplified) - QuanPin" },
	{ KBD_CHINESE_TRADITIONAL_CHANGJIE, "chajei.ime", "Chinese (Traditional) - ChangJie" },
	{ KBD_CHINESE_SIMPLIFIED_SHUANGPIN, "winsp.ime", "Chinese (Simplified) - ShuangPin" },
	{ KBD_CHINESE_TRADITIONAL_QUICK, "quick.ime", "Chinese (Traditional) - Quick" },
	{ KBD_CHINESE_SIMPLIFIED_ZHENGMA, "winzm.ime", "Chinese (Simplified) - ZhengMa" },
	{ KBD_CHINESE_TRADITIONAL_BIG5_CODE, "winime.ime", "Chinese (Traditional) - Big5 Code" },
	{ KBD_CHINESE_TRADITIONAL_ARRAY, "winar30.ime", "Chinese (Traditional) - Array" },
	{ KBD_CHINESE_SIMPLIFIED_NEIMA, "wingb.ime", "Chinese (Simplified) - NeiMa" },
	{ KBD_CHINESE_TRADITIONAL_DAYI, "dayi.ime", "Chinese (Traditional) - DaYi" },
	{ KBD_CHINESE_TRADITIONAL_UNICODE, "unicdime.ime", "Chinese (Traditional) - Unicode" },
	{ KBD_CHINESE_TRADITIONAL_NEW_PHONETIC, "TINTLGNT.IME",
	  "Chinese (Traditional) - New Phonetic" },
	{ KBD_CHINESE_TRADITIONAL_NEW_CHANGJIE, "CINTLGNT.IME",
	  "Chinese (Traditional) - New ChangJie" },
	{ KBD_CHINESE_TRADITIONAL_MICROSOFT_PINYIN_IME_3, "pintlgnt.ime",
	  "Chinese (Traditional) - Microsoft Pinyin IME 3.0" },
	{ KBD_CHINESE_TRADITIONAL_ALPHANUMERIC, "romanime.ime", "Chinese (Traditional) - Alphanumeric" }
};

void freerdp_keyboard_layouts_free(RDP_KEYBOARD_LAYOUT* layouts)
{
	RDP_KEYBOARD_LAYOUT* current = layouts;

	if (!layouts)
		return;

	while ((current->code != 0) && (current->name != NULL))
	{
		free(current->name);
		current++;
	}

	free(layouts);
}

RDP_KEYBOARD_LAYOUT* freerdp_keyboard_get_layouts(DWORD types)
{
	size_t num, length, i;
	RDP_KEYBOARD_LAYOUT* layouts;
	RDP_KEYBOARD_LAYOUT* new;
	num = 0;
	layouts = (RDP_KEYBOARD_LAYOUT*)calloc((num + 1), sizeof(RDP_KEYBOARD_LAYOUT));

	if (!layouts)
		return NULL;

	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_STANDARD) != 0)
	{
		length = ARRAYSIZE(RDP_KEYBOARD_LAYOUT_TABLE);
		new = (RDP_KEYBOARD_LAYOUT*)realloc(layouts,
		                                    (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		if (!new)
			goto fail;

		layouts = new;

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_LAYOUT_TABLE[i].code;
			layouts[num].name = _strdup(RDP_KEYBOARD_LAYOUT_TABLE[i].name);

			if (!layouts[num].name)
				goto fail;
		}
	}

	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_VARIANT) != 0)
	{
		length = ARRAYSIZE(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE);
		new = (RDP_KEYBOARD_LAYOUT*)realloc(layouts,
		                                    (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		if (!new)
			goto fail;

		layouts = new;

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].code;
			layouts[num].name = _strdup(RDP_KEYBOARD_LAYOUT_VARIANT_TABLE[i].name);

			if (!layouts[num].name)
				goto fail;
		}
	}

	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_IME) != 0)
	{
		length = ARRAYSIZE(RDP_KEYBOARD_IME_TABLE);
		new = (RDP_KEYBOARD_LAYOUT*)realloc(layouts,
		                                    (num + length + 1) * sizeof(RDP_KEYBOARD_LAYOUT));

		if (!new)
			goto fail;

		layouts = new;

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = RDP_KEYBOARD_IME_TABLE[i].code;
			layouts[num].name = _strdup(RDP_KEYBOARD_IME_TABLE[i].name);

			if (!layouts[num].name)
				goto fail;
		}
	}

	ZeroMemory(&layouts[num], sizeof(RDP_KEYBOARD_LAYOUT));
	return layouts;
fail:
	freerdp_keyboard_layouts_free(layouts);
	return NULL;
}

const char* freerdp_keyboard_get_layout_name_from_id(DWORD keyboardLayoutID)
{
	size_t i;

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
	size_t i;

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

static void copy(const struct LanguageIdentifier* id, RDP_CODEPAGE* cp)
{
	cp->id = id->LanguageIdentifier;
	cp->subId = id->SublangaugeIdentifier;
	cp->primaryId = id->PrimaryLanguageIdentifier;
	if (id->locale)
		strncpy(cp->locale, id->locale, ARRAYSIZE(cp->locale) - 1);
	if (id->PrimaryLanguage)
		strncpy(cp->primaryLanguage, id->PrimaryLanguage, ARRAYSIZE(cp->primaryLanguage) - 1);
	if (id->PrimaryLanguageSymbol)
		strncpy(cp->primaryLanguageSymbol, id->PrimaryLanguageSymbol,
		        ARRAYSIZE(cp->primaryLanguageSymbol) - 1);
	if (id->Sublanguage)
		strncpy(cp->subLanguage, id->Sublanguage, ARRAYSIZE(cp->subLanguage) - 1);
	if (id->SublanguageSymbol)
		strncpy(cp->subLanguageSymbol, id->SublanguageSymbol, ARRAYSIZE(cp->subLanguageSymbol) - 1);
}

static BOOL copyOnMatch(DWORD column, const char* filter, const struct LanguageIdentifier* cur,
                        RDP_CODEPAGE* dst)
{
	const char* what;
	switch (column)
	{
		case 0:
			what = cur->locale;
			break;
		case 1:
			what = cur->PrimaryLanguage;
			break;
		case 2:
			what = cur->PrimaryLanguageSymbol;
			break;
		case 3:
			what = cur->Sublanguage;
			break;
		case 4:
			what = cur->SublanguageSymbol;
			break;
		default:
			return FALSE;
	}

	if (filter)
	{
		if (!strstr(what, filter))
			return FALSE;
	}
	copy(cur, dst);
	return TRUE;
}

RDP_CODEPAGE* freerdp_keyboard_get_matching_codepages(DWORD column, const char* filter,
                                                      size_t* count)
{
	size_t x;
	size_t cnt = 0;
	const size_t c = ARRAYSIZE(language_identifiers);
	RDP_CODEPAGE* pages = calloc(ARRAYSIZE(language_identifiers), sizeof(RDP_CODEPAGE));

	if (!pages)
		return NULL;

	if (count)
		*count = 0;

	if (column > 4)
		goto fail;

	for (x = 0; x < c; x++)
	{
		const struct LanguageIdentifier* cur = &language_identifiers[x];
		if (copyOnMatch(column, filter, cur, &pages[cnt]))
			cnt++;
	}

	if (cnt == 0)
		goto fail;

	if (count)
		*count = cnt;

	return pages;
fail:
	freerdp_codepages_free(pages);
	return NULL;
}

void freerdp_codepages_free(RDP_CODEPAGE* pages)
{
	free(pages);
}
