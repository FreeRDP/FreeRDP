/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Keyboard layout ID detection from common X11 xkb keyboard layout names
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

#include "xkb_layout_ids.h"

#include <string.h>

#include <winpr/crt.h>

#include <freerdp/locale/keyboard.h>

#include "liblocale.h"

struct _XKB_VARIANT
{
	const char* variant; /* XKB Keyboard layout variant */
	UINT32 keyboardLayoutID; /* Keyboard Layout ID */
};
typedef struct _XKB_VARIANT XKB_VARIANT;

struct _XKB_LAYOUT
{
	const char* layout; /* XKB Keyboard layout */
	UINT32 keyboardLayoutID; /* Keyboard Layout ID */
	const XKB_VARIANT* variants;
};
typedef struct _XKB_LAYOUT XKB_LAYOUT;

/* Those have been generated automatically and are waiting to be filled by hand */

/* USA */
static const XKB_VARIANT us_variants[] =
{
	{ "chr",		0 }, /* Cherokee */
	{ "euro",		0 }, /* With EuroSign on 5 */
	{ "intl",		KBD_UNITED_STATES_INTERNATIONAL }, /* International (with dead keys) */
	{ "alt-intl",		KBD_UNITED_STATES_INTERNATIONAL }, /* Alternative international (former us_intl) */
	{ "colemak",		0 }, /* Colemak */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "dvorak-intl",	KBD_UNITED_STATES_DVORAK }, /* Dvorak international */
	{ "dvorak-l",		KBD_UNITED_STATES_DVORAK_FOR_LEFT_HAND }, /* Left handed Dvorak */
	{ "dvorak-r",		KBD_UNITED_STATES_DVORAK_FOR_RIGHT_HAND }, /* Right handed Dvorak */
	{ "dvorak-classic",	KBD_UNITED_STATES_DVORAK }, /* Classic Dvorak */
	{ "dvp",		KBD_UNITED_STATES_DVORAK_PROGRAMMER }, /* Programmer Dvorak */
	{ "rus",		0 }, /* Russian phonetic */
	{ "mac",		KBD_US }, /* Macintosh */
	{ "altgr-intl",		KBD_UNITED_STATES_INTERNATIONAL }, /* International (AltGr dead keys) */
	{ "olpc2",		KBD_US }, /* Group toggle on multiply/divide key */
	{ "",			0 },
};

/* Afghanistan */
static const XKB_VARIANT af_variants[] =
{
	{ "ps",			KBD_PASHTO }, /* Pashto */
	{ "uz",			KBD_UZBEK_CYRILLIC }, /* Southern Uzbek */
	{ "olpc-ps",		KBD_PASHTO }, /* OLPC Pashto */
	{ "olpc-fa",		0 }, /* OLPC Dari */
	{ "olpc-uz",		KBD_UZBEK_CYRILLIC }, /* OLPC Southern Uzbek */
	{ "",			0 },
};

/* Arabic */
static const XKB_VARIANT ara_variants[] =
{
	{ "azerty",		KBD_ARABIC_102_AZERTY }, /* azerty */
	{ "azerty_digits",	KBD_ARABIC_102_AZERTY }, /* azerty/digits */
	{ "digits",		KBD_ARABIC_102_AZERTY }, /* digits */
	{ "qwerty",		KBD_ARABIC_101 }, /* qwerty */
	{ "qwerty_digits",	KBD_ARABIC_101 }, /* qwerty/digits */
	{ "buckwalter",		KBD_US_ENGLISH_TABLE_FOR_IBM_ARABIC_238_L }, /* Buckwalter */
	{ "",			0 },
};

/* Armenia */
static const XKB_VARIANT am_variants[] =
{
	{ "phonetic",		0 }, /* Phonetic */
	{ "phonetic-alt",	0 }, /* Alternative Phonetic */
	{ "eastern",		KBD_ARMENIAN_EASTERN }, /* Eastern */
	{ "western",		KBD_ARMENIAN_WESTERN }, /* Western */
	{ "eastern-alt",	KBD_ARMENIAN_EASTERN }, /* Alternative Eastern */
	{ "",			0 },
};

/* Azerbaijan */
static const XKB_VARIANT az_variants[] =
{
	{ "cyrillic",		KBD_AZERI_CYRILLIC }, /* Cyrillic */
	{ "",			0 },
};

/* Belarus */
static const XKB_VARIANT by_variants[] =
{
	{ "winkeys",		KBD_BELARUSIAN }, /* Winkeys */
	{ "latin",		KBD_BELARUSIAN }, /* Latin */
	{ "",			0 },
};

/* Belgium */
static const XKB_VARIANT be_variants[] =
{
	{ "oss",		KBD_BELGIAN_FRENCH }, /* Alternative */
	{ "oss_latin9",		KBD_BELGIAN_FRENCH }, /* Alternative, latin-9 only */
	{ "oss_sundeadkeys",	KBD_BELGIAN_PERIOD }, /* Alternative, Sun dead keys */
	{ "iso-alternate",	KBD_BELGIAN_COMMA }, /* ISO Alternate */
	{ "nodeadkeys",		KBD_BELGIAN_COMMA }, /* Eliminate dead keys */
	{ "sundeadkeys",	KBD_BELGIAN_PERIOD }, /* Sun dead keys */
	{ "wang",		KBD_BELGIAN_FRENCH }, /* Wang model 724 azerty */
	{ "",			0 },
};

/* Bangladesh */
static const XKB_VARIANT bd_variants[] =
{
	{ "probhat",		KBD_BENGALI_INSCRIPT }, /* Probhat */
	{ "",			0 },
};

/* India */
static const XKB_VARIANT in_variants[] =
{
	{ "ben",		KBD_BENGALI }, /* Bengali */
	{ "ben_probhat",	KBD_BENGALI_INSCRIPT }, /* Bengali Probhat */
	{ "guj",		KBD_GUJARATI }, /* Gujarati */
	{ "guru",		0 }, /* Gurmukhi */
	{ "jhelum",		0 }, /* Gurmukhi Jhelum */
	{ "kan",		KBD_KANNADA }, /* Kannada */
	{ "mal",		KBD_MALAYALAM }, /* Malayalam */
	{ "mal_lalitha",	KBD_MALAYALAM }, /* Malayalam Lalitha */
	{ "ori",		0 }, /* Oriya */
	{ "tam_unicode",	KBD_TAMIL }, /* Tamil Unicode */
	{ "tam_TAB",		KBD_TAMIL }, /* Tamil TAB Typewriter */
	{ "tam_TSCII",		KBD_TAMIL }, /* Tamil TSCII Typewriter */
	{ "tam",		KBD_TAMIL }, /* Tamil */
	{ "tel",		KBD_TELUGU }, /* Telugu */
	{ "urd-phonetic",	KBD_URDU }, /* Urdu, Phonetic */
	{ "urd-phonetic3",	KBD_URDU }, /* Urdu, Alternative phonetic */
	{ "urd-winkeys",	KBD_URDU }, /* Urdu, Winkeys */
	{ "bolnagri",		KBD_HINDI_TRADITIONAL }, /* Hindi Bolnagri */
	{ "hin-wx",		KBD_HINDI_TRADITIONAL }, /* Hindi Wx */
	{ "",			0 },
};

/* Bosnia and Herzegovina */
static const XKB_VARIANT ba_variants[] =
{
	{ "alternatequotes",	KBD_BOSNIAN }, /* Use guillemets for quotes */
	{ "unicode",		KBD_BOSNIAN }, /* Use Bosnian digraphs */
	{ "unicodeus",		KBD_BOSNIAN }, /* US keyboard with Bosnian digraphs */
	{ "us",			KBD_BOSNIAN_CYRILLIC }, /* US keyboard with Bosnian letters */
	{ "",			0 },
};

/* Brazil */
static const XKB_VARIANT br_variants[] =
{
	{ "nodeadkeys",		KBD_PORTUGUESE_BRAZILIAN_ABNT2 }, /* Eliminate dead keys */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "nativo",		KBD_PORTUGUESE_BRAZILIAN_ABNT2 }, /* Nativo */
	{ "nativo-us",		KBD_PORTUGUESE_BRAZILIAN_ABNT2 }, /* Nativo for USA keyboards */
	{ "nativo-epo",		KBD_PORTUGUESE_BRAZILIAN_ABNT2 }, /* Nativo for Esperanto */
	{ "",			0 },
};

/* Bulgaria */
static const XKB_VARIANT bg_variants[] =
{
	{ "phonetic",		KBD_BULGARIAN_LATIN }, /* Traditional Phonetic */
	{ "bas_phonetic",	KBD_BULGARIAN_LATIN }, /* Standard Phonetic */
	{ "",			0 },
};

/* Morocco */
static const XKB_VARIANT ma_variants[] =
{
	{ "french",			KBD_FRENCH }, /* French */
	{ "tifinagh",			0 }, /* Tifinagh */
	{ "tifinagh-alt",		0 }, /* Tifinagh Alternative */
	{ "tifinagh-alt-phonetic",	0 }, /* Tifinagh Alternative Phonetic */
	{ "tifinagh-extended",		0 }, /* Tifinagh Extended */
	{ "tifinagh-phonetic",		0 }, /* Tifinagh Phonetic */
	{ "tifinagh-extended-phonetic",	0 }, /* Tifinagh Extended Phonetic */
	{ "",				0 },
};

/* Canada */
static const XKB_VARIANT ca_variants[] =
{
	{ "fr",				KBD_CANADIAN_FRENCH }, /* French Dvorak */
	{ "fr-dvorak",		KBD_UNITED_STATES_DVORAK }, /* French Dvorak */
	{ "fr-legacy",		KBD_CANADIAN_FRENCH_LEGACY }, /* French (legacy) */
	{ "multix",		KBD_CANADIAN_MULTILINGUAL_STANDARD }, /* Multilingual */
	{ "multi",		KBD_CANADIAN_MULTILINGUAL_STANDARD }, /* Multilingual, first part */
	{ "multi-2gr",		KBD_CANADIAN_MULTILINGUAL_STANDARD }, /* Multilingual, second part */
	{ "ike",		KBD_INUKTITUT_LATIN }, /* Inuktitut */
	{ "shs",		0 }, /* Secwepemctsin */
	{ "kut",		0 }, /* Ktunaxa */
	{ "eng",		KBD_CANADIAN_ENGLISH }, /* English */
	{ "",			0 },
};

/* China */
static const XKB_VARIANT cn_variants[] =
{
	{ "tib",		0 }, /* Tibetan */
	{ "tib_asciinum",	0 }, /* Tibetan (with ASCII numerals) */
	{ "",			0 },
};

/* Croatia */
static const XKB_VARIANT hr_variants[] =
{
	{ "alternatequotes",	KBD_CROATIAN }, /* Use guillemets for quotes */
	{ "unicode",		KBD_CROATIAN }, /* Use Croatian digraphs */
	{ "unicodeus",		KBD_CROATIAN }, /* US keyboard with Croatian digraphs */
	{ "us",			KBD_CROATIAN }, /* US keyboard with Croatian letters */
	{ "",			0 },
};

/* Czechia */
static const XKB_VARIANT cz_variants[] =
{
	{ "bksl",		KBD_CZECH_PROGRAMMERS }, /* With &lt;\|&gt; key */
	{ "qwerty",		KBD_CZECH_QWERTY }, /* qwerty */
	{ "qwerty_bksl",	KBD_CZECH_QWERTY }, /* qwerty, extended Backslash */
	{ "ucw",		KBD_CZECH }, /* UCW layout (accented letters only) */
	{ "",			0 },
};

/* Denmark */
static const XKB_VARIANT dk_variants[] =
{
	{ "nodeadkeys",		KBD_DANISH }, /* Eliminate dead keys */
	{ "mac",		KBD_DANISH }, /* Macintosh */
	{ "mac_nodeadkeys",	KBD_DANISH }, /* Macintosh, eliminate dead keys */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "",			0 },
};

/* Netherlands */
static const XKB_VARIANT nl_variants[] =
{
	{ "sundeadkeys",	KBD_SWISS_FRENCH }, /* Sun dead keys */
	{ "mac",		KBD_SWISS_FRENCH }, /* Macintosh */
	{ "std",		KBD_SWISS_FRENCH }, /* Standard */
	{ "",			0 },
};

/* Estonia */
static const XKB_VARIANT ee_variants[] =
{
	{ "nodeadkeys",		KBD_US }, /* Eliminate dead keys */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "us",			KBD_UNITED_STATES_INTERNATIONAL }, /* US keyboard with Estonian letters */
	{ "",			0 },
};

/* Iran */
static const XKB_VARIANT ir_variants[] =
{
	{ "pro",		0 }, /* Pro */
	{ "keypad",		0 }, /* Keypad */
	{ "pro_keypad",		0 }, /* Pro Keypad */
	{ "ku",			0 }, /* Kurdish, Latin Q */
	{ "ku_f",		0 }, /* Kurdish, (F) */
	{ "ku_alt",		0 }, /* Kurdish, Latin Alt-Q */
	{ "ku_ara",		0 }, /* Kurdish, Arabic-Latin */
	{ "",			0 },
};

/* Iraq */
static const XKB_VARIANT iq_variants[] =
{
	{ "ku",			0 }, /* Kurdish, Latin Q */
	{ "ku_f",		0 }, /* Kurdish, (F) */
	{ "ku_alt",		0 }, /* Kurdish, Latin Alt-Q */
	{ "ku_ara",		0 }, /* Kurdish, Arabic-Latin */
	{ "",			0 },
};

/* Faroe Islands */
static const XKB_VARIANT fo_variants[] =
{
	{ "nodeadkeys",		0 }, /* Eliminate dead keys */
	{ "",			0 },
};

/* Finland */
static const XKB_VARIANT fi_variants[] =
{
	{ "nodeadkeys",		0 }, /* Eliminate dead keys */
	{ "smi",		0 }, /* Northern Saami */
	{ "classic",		0 }, /* Classic */
	{ "mac",		0 }, /* Macintosh */
	{ "",			0 },
};

/* France */
static const XKB_VARIANT fr_variants[] =
{
	{ "nodeadkeys",		0 }, /* Eliminate dead keys */
	{ "sundeadkeys",	0 }, /* Sun dead keys */
	{ "oss",		0 }, /* Alternative */
	{ "oss_latin9",		0 }, /* Alternative, latin-9 only */
	{ "oss_nodeadkeys",	0 }, /* Alternative, eliminate dead keys */
	{ "oss_sundeadkeys",	0 }, /* Alternative, Sun dead keys */
	{ "latin9",		0 }, /* (Legacy) Alternative */
	{ "latin9_nodeadkeys",	0 }, /* (Legacy) Alternative, eliminate dead keys */
	{ "latin9_sundeadkeys",	0 }, /* (Legacy) Alternative, Sun dead keys */
	{ "bepo",		0 }, /* Bepo, ergonomic, Dvorak way */
	{ "bepo_latin9",	0 }, /* Bepo, ergonomic, Dvorak way, latin-9 only */
	{ "dvorak",		0 }, /* Dvorak */
	{ "mac",		0 }, /* Macintosh */
	{ "bre",		0 }, /* Breton */
	{ "oci",		0 }, /* Occitan */
	{ "geo",		0 }, /* Georgian AZERTY Tskapo */
	{ "",			0 },
};

/* Ghana */
static const XKB_VARIANT gh_variants[] =
{
	{ "generic",		0 }, /* Multilingual */
	{ "akan",		0 }, /* Akan */
	{ "ewe",		0 }, /* Ewe */
	{ "fula",		0 }, /* Fula */
	{ "ga",			0 }, /* Ga */
	{ "hausa",		0 }, /* Hausa */
	{ "",			0 },
};

/* Georgia */
static const XKB_VARIANT ge_variants[] =
{
	{ "ergonomic",		0 }, /* Ergonomic */
	{ "mess",		0 }, /* MESS */
	{ "ru",			0 }, /* Russian */
	{ "os",			0 }, /* Ossetian */
	{ "",			0 },
};

/* Germany */
static const XKB_VARIANT de_variants[] =
{
	{ "deadacute",		KBD_GERMAN }, /* Dead acute */
	{ "deadgraveacute",	KBD_GERMAN }, /* Dead grave acute */
	{ "nodeadkeys",		KBD_GERMAN }, /* Eliminate dead keys */
	{ "ro",			KBD_GERMAN }, /* Romanian keyboard with German letters */
	{ "ro_nodeadkeys",	KBD_GERMAN }, /* Romanian keyboard with German letters, eliminate dead keys */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "sundeadkeys",	KBD_GERMAN }, /* Sun dead keys */
	{ "neo",		KBD_GERMAN_NEO }, /* Neo 2 */
	{ "mac",		KBD_GERMAN }, /* Macintosh */
	{ "mac_nodeadkeys",	KBD_GERMAN }, /* Macintosh, eliminate dead keys */
	{ "dsb",		KBD_GERMAN }, /* Lower Sorbian */
	{ "dsb_qwertz",		KBD_GERMAN }, /* Lower Sorbian (qwertz) */
	{ "qwerty",		KBD_GERMAN_IBM }, /* qwerty */
	{ "",			0 },
};

/* Greece */
static const XKB_VARIANT gr_variants[] =
{
	{ "simple",		KBD_GREEK_220 }, /* Simple */
	{ "extended",		KBD_GREEK_319 }, /* Extended */
	{ "nodeadkeys",		KBD_GREEK_319}, /* Eliminate dead keys */
	{ "polytonic",		KBD_GREEK_POLYTONIC }, /* Polytonic */
	{ "",			0 },
};

/* Hungary */
static const XKB_VARIANT hu_variants[] =
{
	{ "standard",				KBD_HUNGARIAN_101_KEY }, /* Standard */
	{ "nodeadkeys",				KBD_HUNGARIAN_101_KEY }, /* Eliminate dead keys */
	{ "qwerty",				KBD_HUNGARIAN_101_KEY }, /* qwerty */
	{ "101_qwertz_comma_dead",		KBD_HUNGARIAN_101_KEY }, /* 101/qwertz/comma/Dead keys */
	{ "101_qwertz_comma_nodead",		KBD_HUNGARIAN_101_KEY }, /* 101/qwertz/comma/Eliminate dead keys */
	{ "101_qwertz_dot_dead",		KBD_HUNGARIAN_101_KEY }, /* 101/qwertz/dot/Dead keys */
	{ "101_qwertz_dot_nodead",		KBD_HUNGARIAN_101_KEY }, /* 101/qwertz/dot/Eliminate dead keys */
	{ "101_qwerty_comma_dead",		KBD_HUNGARIAN_101_KEY }, /* 101/qwerty/comma/Dead keys */
	{ "101_qwerty_comma_nodead",		KBD_HUNGARIAN_101_KEY }, /* 101/qwerty/comma/Eliminate dead keys */
	{ "101_qwerty_dot_dead",		KBD_HUNGARIAN_101_KEY }, /* 101/qwerty/dot/Dead keys */
	{ "101_qwerty_dot_nodead",		KBD_HUNGARIAN_101_KEY }, /* 101/qwerty/dot/Eliminate dead keys */
	{ "102_qwertz_comma_dead",		KBD_HUNGARIAN_101_KEY }, /* 102/qwertz/comma/Dead keys */
	{ "102_qwertz_comma_nodead",		KBD_HUNGARIAN_101_KEY }, /* 102/qwertz/comma/Eliminate dead keys */
	{ "102_qwertz_dot_dead",		KBD_HUNGARIAN_101_KEY }, /* 102/qwertz/dot/Dead keys */
	{ "102_qwertz_dot_nodead",		KBD_HUNGARIAN_101_KEY }, /* 102/qwertz/dot/Eliminate dead keys */
	{ "102_qwerty_comma_dead",		KBD_HUNGARIAN_101_KEY }, /* 102/qwerty/comma/Dead keys */
	{ "102_qwerty_comma_nodead",		KBD_HUNGARIAN_101_KEY }, /* 102/qwerty/comma/Eliminate dead keys */
	{ "102_qwerty_dot_dead",		KBD_HUNGARIAN_101_KEY }, /* 102/qwerty/dot/Dead keys */
	{ "102_qwerty_dot_nodead",		KBD_HUNGARIAN_101_KEY }, /* 102/qwerty/dot/Eliminate dead keys */
	{ "",					0 },
};

/* Iceland */
static const XKB_VARIANT is_variants[] =
{
	{ "Sundeadkeys",	KBD_ICELANDIC }, /* Sun dead keys */
	{ "nodeadkeys",		KBD_ICELANDIC }, /* Eliminate dead keys */
	{ "mac",		KBD_ICELANDIC }, /* Macintosh */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "",			0 },
};

/* Israel */
static const XKB_VARIANT il_variants[] =
{
	{ "lyx",		KBD_HEBREW }, /* lyx */
	{ "phonetic",		KBD_HEBREW }, /* Phonetic */
	{ "biblical",		KBD_HEBREW }, /* Biblical Hebrew (Tiro) */
	{ "",			0 },
};

/* Italy */
static const XKB_VARIANT it_variants[] =
{
	{ "nodeadkeys",		KBD_ITALIAN_142 }, /* Eliminate dead keys */
	{ "mac",		KBD_ITALIAN }, /* Macintosh */
	{ "geo",		KBD_GEORGIAN }, /* Georgian */
	{ "",			0 },
};

/* Japan */
static const XKB_VARIANT jp_variants[] =
{
	{ "kana",		KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002 }, /* Kana */
	{ "OADG109A",		KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002 }, /* OADG 109A */
	{ "",			0 },
};

/* Kyrgyzstan */
static const XKB_VARIANT kg_variants[] =
{
	{ "phonetic",		KBD_KYRGYZ_CYRILLIC }, /* Phonetic */
	{ "",			0 },
};

/* Kazakhstan */
static const XKB_VARIANT kz_variants[] =
{
	{ "ruskaz",		KBD_KAZAKH }, /* Russian with Kazakh */
	{ "kazrus",		KBD_KAZAKH }, /* Kazakh with Russian */
	{ "",			0 },
};

/* Latin America */
static const XKB_VARIANT latam_variants[] =
{
	{ "nodeadkeys",		KBD_LATIN_AMERICAN }, /* Eliminate dead keys */
	{ "deadtilde",		KBD_LATIN_AMERICAN }, /* Include dead tilde */
	{ "sundeadkeys",	KBD_LATIN_AMERICAN }, /* Sun dead keys */
	{ "",			0 },
};

/* Lithuania */
static const XKB_VARIANT lt_variants[] =
{
	{ "std",		KBD_LITHUANIAN }, /* Standard */
	{ "us",			KBD_LITHUANIAN_IBM }, /* US keyboard with Lithuanian letters */
	{ "ibm",		KBD_LITHUANIAN_IBM }, /* IBM (LST 1205-92) */
	{ "lekp",		KBD_LITHUANIAN }, /* LEKP */
	{ "lekpa",		KBD_LITHUANIAN }, /* LEKPa */
	{ "balticplus",		KBD_LITHUANIAN }, /* Baltic+ */
	{ "",			0 },
};

/* Latvia */
static const XKB_VARIANT lv_variants[] =
{
	{ "apostrophe",		KBD_LATVIAN }, /* Apostrophe (') variant */
	{ "tilde",		KBD_LATVIAN }, /* Tilde (~) variant */
	{ "fkey",		KBD_LATVIAN }, /* F-letter (F) variant */
	{ "",			0 },
};

/* Montenegro */
static const XKB_VARIANT me_variants[] =
{
	{ "cyrillic",			0 }, /* Cyrillic */
	{ "cyrillicyz",			0 }, /* Cyrillic, Z and ZHE swapped */
	{ "latinunicode",		0 }, /* Latin unicode */
	{ "latinyz",			0 }, /* Latin qwerty */
	{ "latinunicodeyz",		0 }, /* Latin unicode qwerty */
	{ "cyrillicalternatequotes",	0 }, /* Cyrillic with guillemets */
	{ "latinalternatequotes",	0 }, /* Latin with guillemets */
	{ "",			0 },
};

/* Macedonia */
static const XKB_VARIANT mk_variants[] =
{
	{ "nodeadkeys",		KBD_FYRO_MACEDONIAN }, /* Eliminate dead keys */
	{ "",			0 },
};

/* Malta */
static const XKB_VARIANT mt_variants[] =
{
	{ "us",			KBD_MALTESE_48_KEY }, /* Maltese keyboard with US layout */
	{ "",			0 },
};

/* Norway */
static const XKB_VARIANT no_variants[] =
{
	{ "nodeadkeys",		KBD_NORWEGIAN }, /* Eliminate dead keys */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "smi",		KBD_NORWEGIAN_WITH_SAMI }, /* Northern Saami */
	{ "smi_nodeadkeys",	KBD_SAMI_EXTENDED_NORWAY }, /* Northern Saami, eliminate dead keys */
	{ "mac",		KBD_NORWEGIAN }, /* Macintosh */
	{ "mac_nodeadkeys",	KBD_SAMI_EXTENDED_NORWAY }, /* Macintosh, eliminate dead keys */
	{ "",			0 },
};

/* Poland */
static const XKB_VARIANT pl_variants[] =
{
	{ "qwertz",		KBD_POLISH_214 }, /* qwertz */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "dvorak_quotes",	KBD_UNITED_STATES_DVORAK }, /* Dvorak, Polish quotes on quotemark key */
	{ "dvorak_altquotes",	KBD_UNITED_STATES_DVORAK }, /* Dvorak, Polish quotes on key 1 */
	{ "csb",		0 }, /* Kashubian */
	{ "ru_phonetic_dvorak",	KBD_UNITED_STATES_DVORAK }, /* Russian phonetic Dvorak */
	{ "",			0 },
};

/* Portugal */
static const XKB_VARIANT pt_variants[] =
{
	{ "nodeadkeys",		KBD_PORTUGUESE }, /* Eliminate dead keys */
	{ "sundeadkeys",	KBD_PORTUGUESE }, /* Sun dead keys */
	{ "mac",		KBD_PORTUGUESE }, /* Macintosh */
	{ "mac_nodeadkeys",	KBD_PORTUGUESE }, /* Macintosh, eliminate dead keys */
	{ "mac_sundeadkeys",	KBD_PORTUGUESE }, /* Macintosh, Sun dead keys */
	{ "nativo",		KBD_PORTUGUESE }, /* Nativo */
	{ "nativo-us",		KBD_PORTUGUESE }, /* Nativo for USA keyboards */
	{ "nativo-epo",		KBD_PORTUGUESE }, /* Nativo for Esperanto */
	{ "",			0 },
};

/* Romania */
static const XKB_VARIANT ro_variants[] =
{
	{ "cedilla",		KBD_ROMANIAN }, /* Cedilla */
	{ "std",		KBD_ROMANIAN }, /* Standard */
	{ "std_cedilla",	KBD_ROMANIAN }, /* Standard (Cedilla) */
	{ "winkeys",		KBD_ROMANIAN }, /* Winkeys */
	{ "crh_f",		KBD_TURKISH_F }, /* Crimean Tatar (Turkish F) */
	{ "crh_alt",		KBD_TURKISH_Q }, /* Crimean Tatar (Turkish Alt-Q) */
	{ "crh_dobruca1",	KBD_TATAR }, /* Crimean Tatar (Dobruca-1 Q) */
	{ "crh_dobruca2",	KBD_TATAR }, /* Crimean Tatar (Dobruca-2 Q) */
	{ "",			0 },
};

/* Russia */
static const XKB_VARIANT ru_variants[] =
{
	{ "phonetic",		KBD_RUSSIAN }, /* Phonetic */
	{ "phonetic_winkeys",	KBD_RUSSIAN }, /* Phonetic Winkeys */
	{ "typewriter",		KBD_RUSSIAN_TYPEWRITER }, /* Typewriter */
	{ "legacy",		KBD_RUSSIAN }, /* Legacy */
	{ "tt",			KBD_TATAR }, /* Tatar */
	{ "os_legacy",		0 }, /* Ossetian, legacy */
	{ "os_winkeys",		0 }, /* Ossetian, Winkeys */
	{ "cv",			0 }, /* Chuvash */
	{ "cv_latin",		0 }, /* Chuvash Latin */
	{ "udm",		0 }, /* Udmurt */
	{ "kom",		0 }, /* Komi */
	{ "sah",		0 }, /* Yakut */
	{ "xal",		0 }, /* Kalmyk */
	{ "dos",		0 }, /* DOS */
	{ "",			0 },
};

/* Serbia */
static const XKB_VARIANT rs_variants[] =
{
	{ "yz",				KBD_SERBIAN_CYRILLIC }, /* Z and ZHE swapped */
	{ "latin",			KBD_SERBIAN_LATIN }, /* Latin */
	{ "latinunicode",		KBD_SERBIAN_LATIN }, /* Latin Unicode */
	{ "latinyz",			KBD_SERBIAN_LATIN }, /* Latin qwerty */
	{ "latinunicodeyz",		KBD_SERBIAN_LATIN }, /* Latin Unicode qwerty */
	{ "alternatequotes",		KBD_SERBIAN_CYRILLIC }, /* With guillemets */
	{ "latinalternatequotes",	KBD_SERBIAN_LATIN }, /* Latin with guillemets */
	{ "",				0 },
};

/* Slovenia */
static const XKB_VARIANT si_variants[] =
{
	{ "alternatequotes",	KBD_SLOVENIAN }, /* Use guillemets for quotes */
	{ "us",			KBD_UNITED_STATES_INTERNATIONAL }, /* US keyboard with Slovenian letters */
	{ "",			0 },
};

/* Slovakia */
static const XKB_VARIANT sk_variants[] =
{
	{ "bksl",		KBD_SLOVAK }, /* Extended Backslash */
	{ "qwerty",		KBD_SLOVAK_QWERTY }, /* qwerty */
	{ "qwerty_bksl",	KBD_SLOVAK_QWERTY }, /* qwerty, extended Backslash */
	{ "",			0 },
};

/* Spain */
static const XKB_VARIANT es_variants[] =
{
	{ "nodeadkeys",		KBD_SPANISH_VARIATION }, /* Eliminate dead keys */
	{ "deadtilde",		KBD_SPANISH_VARIATION }, /* Include dead tilde */
	{ "sundeadkeys",	KBD_SPANISH }, /* Sun dead keys */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "ast",		KBD_SPANISH_VARIATION }, /* Asturian variant with bottom-dot H and bottom-dot L */
	{ "cat",		KBD_SPANISH_VARIATION }, /* Catalan variant with middle-dot L */
	{ "mac",		KBD_SPANISH }, /* Macintosh */
	{ "",			0 },
};

/* Sweden */
static const XKB_VARIANT se_variants[] =
{
	{ "nodeadkeys",		KBD_SWEDISH }, /* Eliminate dead keys */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "rus",		KBD_RUSSIAN }, /* Russian phonetic */
	{ "rus_nodeadkeys",	KBD_RUSSIAN }, /* Russian phonetic, eliminate dead keys */
	{ "smi",		KBD_SWEDISH_WITH_SAMI }, /* Northern Saami */
	{ "mac",		KBD_SWEDISH }, /* Macintosh */
	{ "svdvorak",		KBD_UNITED_STATES_DVORAK }, /* Svdvorak */
	{ "",			0 },
};

/* Switzerland */
static const XKB_VARIANT ch_variants[] =
{
	{ "de_nodeadkeys",	KBD_SWISS_GERMAN }, /* German, eliminate dead keys */
	{ "de_sundeadkeys",	KBD_SWISS_GERMAN }, /* German, Sun dead keys */
	{ "fr",			KBD_SWISS_FRENCH }, /* French */
	{ "fr_nodeadkeys",	KBD_SWISS_FRENCH }, /* French, eliminate dead keys */
	{ "fr_sundeadkeys",	KBD_SWISS_FRENCH }, /* French, Sun dead keys */
	{ "fr_mac",		KBD_SWISS_FRENCH }, /* French (Macintosh) */
	{ "de_mac",		KBD_SWISS_GERMAN }, /* German (Macintosh) */
	{ "",			0 },
};

/* Syria */
static const XKB_VARIANT sy_variants[] =
{
	{ "syc",		KBD_SYRIAC }, /* Syriac */
	{ "syc_phonetic",	KBD_SYRIAC_PHONETIC }, /* Syriac phonetic */
	{ "ku",			0 }, /* Kurdish, Latin Q */
	{ "ku_f",		0 }, /* Kurdish, (F) */
	{ "ku_alt",		0 }, /* Kurdish, Latin Alt-Q */
	{ "",			0 },
};

/* Tajikistan */
static const XKB_VARIANT tj_variants[] =
{
	{ "legacy",		0 }, /* Legacy */
	{ "",			0 },
};

/* Sri Lanka */
static const XKB_VARIANT lk_variants[] =
{
	{ "tam_unicode",	KBD_TAMIL }, /* Tamil Unicode */
	{ "tam_TAB",		KBD_TAMIL }, /* Tamil TAB Typewriter */
	{ "",			0 },
};

/* Thailand */
static const XKB_VARIANT th_variants[] =
{
	{ "tis",		KBD_THAI_KEDMANEE_NON_SHIFTLOCK }, /* TIS-820.2538 */
	{ "pat",		KBD_THAI_PATTACHOTE }, /* Pattachote */
	{ "",			0 },
};

/* Turkey */
static const XKB_VARIANT tr_variants[] =
{
	{ "f",			KBD_TURKISH_F }, /* (F) */
	{ "alt",		KBD_TURKISH_Q }, /* Alt-Q */
	{ "sundeadkeys",	KBD_TURKISH_F }, /* Sun dead keys */
	{ "ku",			0 }, /* Kurdish, Latin Q */
	{ "ku_f",		0 }, /* Kurdish, (F) */
	{ "ku_alt",		0 }, /* Kurdish, Latin Alt-Q */
	{ "intl",		KBD_TURKISH_F }, /* International (with dead keys) */
	{ "crh",		KBD_TATAR }, /* Crimean Tatar (Turkish Q) */
	{ "crh_f",		KBD_TURKISH_F }, /* Crimean Tatar (Turkish F) */
	{ "crh_alt",		KBD_TURKISH_Q }, /* Crimean Tatar (Turkish Alt-Q) */
	{ "",			0 },
};

/* Ukraine */
static const XKB_VARIANT ua_variants[] =
{
	{ "phonetic",		KBD_UKRAINIAN }, /* Phonetic */
	{ "typewriter",		KBD_UKRAINIAN }, /* Typewriter */
	{ "winkeys",		KBD_UKRAINIAN }, /* Winkeys */
	{ "legacy",		KBD_UKRAINIAN }, /* Legacy */
	{ "rstu",		KBD_UKRAINIAN }, /* Standard RSTU */
	{ "rstu_ru",		KBD_UKRAINIAN }, /* Standard RSTU on Russian layout */
	{ "homophonic",		KBD_UKRAINIAN }, /* Homophonic */
	{ "crh",		KBD_TATAR }, /* Crimean Tatar (Turkish Q) */
	{ "crh_f",		KBD_TURKISH_F }, /* Crimean Tatar (Turkish F) */
	{ "crh_alt",		KBD_TURKISH_Q }, /* Crimean Tatar (Turkish Alt-Q) */
	{ "",			0 },
};

/* United Kingdom */
static const XKB_VARIANT gb_variants[] =
{
	{ "extd",		KBD_UNITED_KINGDOM_EXTENDED }, /* Extended - Winkeys */
	{ "intl",		KBD_UNITED_KINGDOM_EXTENDED }, /* International (with dead keys) */
	{ "dvorak",		KBD_UNITED_STATES_DVORAK }, /* Dvorak */
	{ "dvorakukp",		KBD_UNITED_STATES_DVORAK }, /* Dvorak (UK Punctuation) */
	{ "mac",		KBD_UNITED_KINGDOM }, /* Macintosh */
	{ "colemak",		0 }, /* Colemak */
	{ "",			0 },
};

/* Uzbekistan */
static const XKB_VARIANT uz_variants[] =
{
	{ "latin",		0 }, /* Latin */
	{ "crh",		KBD_TATAR }, /* Crimean Tatar (Turkish Q) */
	{ "crh_f",		KBD_TURKISH_F }, /* Crimean Tatar (Turkish F) */
	{ "crh_alt",		KBD_TURKISH_Q }, /* Crimean Tatar (Turkish Alt-Q) */
	{ "",			0 },
};

/* Korea, Republic of */
static const XKB_VARIANT kr_variants[] =
{
	{ "kr104",		KBD_KOREAN_INPUT_SYSTEM_IME_2000 }, /* 101/104 key Compatible */
	{ "",			0 },
};

/* Ireland */
static const XKB_VARIANT ie_variants[] =
{
	{ "CloGaelach",		KBD_GAELIC }, /* CloGaelach */
	{ "UnicodeExpert",	KBD_GAELIC }, /* UnicodeExpert */
	{ "ogam",		KBD_GAELIC }, /* Ogham */
	{ "ogam_is434",		KBD_GAELIC }, /* Ogham IS434 */
	{ "",			0 },
};

/* Pakistan */
static const XKB_VARIANT pk_variants[] =
{
	{ "urd-crulp",		0 }, /* CRULP */
	{ "urd-nla",		0 }, /* NLA */
	{ "ara",		KBD_ARABIC_101 }, /* Arabic */
	{ "",			0 },
};

/* Esperanto */
static const XKB_VARIANT epo_variants[] =
{
	{ "legacy",		0 }, /* displaced semicolon and quote (obsolete) */
	{ "",			0 },
};

/* Nigeria */
static const XKB_VARIANT ng_variants[] =
{
	{ "igbo",		0 }, /* Igbo */
	{ "yoruba",		0 }, /* Yoruba */
	{ "hausa",		0 }, /* Hausa */
	{ "",			0 },
};

/* Braille */
static const XKB_VARIANT brai_variants[] =
{
	{ "left_hand",		0 }, /* Left hand */
	{ "right_hand",		0 }, /* Right hand */
	{ "",			0 },
};

/* Turkmenistan */
static const XKB_VARIANT tm_variants[] =
{
	{ "alt",		KBD_TURKISH_Q }, /* Alt-Q */
	{ "",			0 },
};

static const XKB_LAYOUT xkbLayouts[] =
{
	{ "us",		 KBD_US, us_variants }, /* USA */
	{ "ad",		 0, NULL }, /* Andorra */
	{ "af",		 KBD_FARSI, af_variants }, /* Afghanistan */
	{ "ara",	 KBD_ARABIC_101, ara_variants }, /* Arabic */
	{ "al",		 0, NULL }, /* Albania */
	{ "am",		 KBD_ARMENIAN_EASTERN, am_variants }, /* Armenia */
	{ "az",		 KBD_AZERI_CYRILLIC, az_variants }, /* Azerbaijan */
	{ "by",		 KBD_BELARUSIAN, by_variants }, /* Belarus */
	{ "be",		 KBD_BELGIAN_FRENCH, be_variants }, /* Belgium */
	{ "bd",		 KBD_BENGALI, bd_variants }, /* Bangladesh */
	{ "in",		 KBD_HINDI_TRADITIONAL, in_variants }, /* India */
	{ "ba",		 KBD_CROATIAN, ba_variants }, /* Bosnia and Herzegovina */
	{ "br",		 KBD_PORTUGUESE_BRAZILIAN_ABNT, br_variants }, /* Brazil */
	{ "bg",		 KBD_BULGARIAN_LATIN, bg_variants }, /* Bulgaria */
	{ "ma",		 KBD_FRENCH, ma_variants }, /* Morocco */
	{ "mm",		 0, NULL }, /* Myanmar */
	{ "ca",		 KBD_US, ca_variants }, /* Canada */
	{ "cd",		 0, NULL }, /* Congo, Democratic Republic of the */
	{ "cn",		 KBD_CHINESE_TRADITIONAL_PHONETIC, cn_variants }, /* China */
	{ "hr",		 KBD_CROATIAN, hr_variants }, /* Croatia */
	{ "cz",		 KBD_CZECH, cz_variants }, /* Czechia */
	{ "dk",		 KBD_DANISH, dk_variants }, /* Denmark */
	{ "nl",		 KBD_DUTCH, nl_variants }, /* Netherlands */
	{ "bt",		 0, NULL }, /* Bhutan */
	{ "ee",		 KBD_ESTONIAN, ee_variants }, /* Estonia */
	{ "ir",		 0, ir_variants }, /* Iran */
	{ "iq",		 0, iq_variants }, /* Iraq */
	{ "fo",		 0, fo_variants }, /* Faroe Islands */
	{ "fi",		 KBD_FINNISH, fi_variants }, /* Finland */
	{ "fr",		 KBD_FRENCH, fr_variants }, /* France */
	{ "gh",		 0, gh_variants }, /* Ghana */
	{ "gn",		 0, NULL }, /* Guinea */
	{ "ge",		 KBD_GEORGIAN, ge_variants }, /* Georgia */
	{ "de",		 KBD_GERMAN, de_variants }, /* Germany */
	{ "gr",		 KBD_GREEK, gr_variants }, /* Greece */
	{ "hu",		 KBD_HUNGARIAN, hu_variants }, /* Hungary */
	{ "is",		 KBD_ICELANDIC, is_variants }, /* Iceland */
	{ "il",		 KBD_HEBREW, il_variants }, /* Israel */
	{ "it",		 KBD_ITALIAN, it_variants }, /* Italy */
	{ "jp",		 KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002, jp_variants }, /* Japan */
	{ "kg",		 0, kg_variants }, /* Kyrgyzstan */
	{ "kh",		 0, NULL }, /* Cambodia */
	{ "kz",		 KBD_KAZAKH, kz_variants }, /* Kazakhstan */
	{ "la",		 0, NULL }, /* Laos */
	{ "latam",	 KBD_LATIN_AMERICAN, latam_variants }, /* Latin America */
	{ "lt",		 KBD_LITHUANIAN, lt_variants }, /* Lithuania */
	{ "lv",		 KBD_LATVIAN, lv_variants }, /* Latvia */
	{ "mao",	 KBD_MAORI, NULL }, /* Maori */
	{ "me",		 KBD_SERBIAN_LATIN, me_variants }, /* Montenegro */
	{ "mk",		 KBD_FYRO_MACEDONIAN, mk_variants }, /* Macedonia */
	{ "mt",		 KBD_MALTESE_48_KEY, mt_variants }, /* Malta */
	{ "mn",		 KBD_MONGOLIAN_CYRILLIC, NULL }, /* Mongolia */
	{ "no",		 KBD_NORWEGIAN, no_variants }, /* Norway */
	{ "pl",		 KBD_POLISH_214, pl_variants }, /* Poland */
	{ "pt",		 KBD_PORTUGUESE, pt_variants }, /* Portugal */
	{ "ro",		 KBD_ROMANIAN, ro_variants }, /* Romania */
	{ "ru",		 KBD_RUSSIAN, ru_variants }, /* Russia */
	{ "rs",		 KBD_SERBIAN_LATIN, rs_variants }, /* Serbia */
	{ "si",		 KBD_SLOVENIAN, si_variants }, /* Slovenia */
	{ "sk",		 KBD_SLOVAK, sk_variants }, /* Slovakia */
	{ "es",		 KBD_SPANISH, es_variants }, /* Spain */
	{ "se",		 KBD_SWEDISH, se_variants }, /* Sweden */
	{ "ch",		 KBD_SWISS_FRENCH, ch_variants }, /* Switzerland */
	{ "sy",		 KBD_SYRIAC, sy_variants }, /* Syria */
	{ "tj",		 0, tj_variants }, /* Tajikistan */
	{ "lk",		 0, lk_variants }, /* Sri Lanka */
	{ "th",		 KBD_THAI_KEDMANEE, th_variants }, /* Thailand */
	{ "tr",		 KBD_TURKISH_Q, tr_variants }, /* Turkey */
	{ "ua",		 KBD_UKRAINIAN, ua_variants }, /* Ukraine */
	{ "gb",		 KBD_UNITED_KINGDOM, gb_variants }, /* United Kingdom */
	{ "uz",		 KBD_UZBEK_CYRILLIC, uz_variants }, /* Uzbekistan */
	{ "vn",		 KBD_VIETNAMESE, NULL }, /* Vietnam */
	{ "kr",		 KBD_KOREAN_INPUT_SYSTEM_IME_2000, kr_variants }, /* Korea, Republic of */
	{ "ie",		 KBD_UNITED_KINGDOM, ie_variants }, /* Ireland */
	{ "pk",		 0, pk_variants }, /* Pakistan */
	{ "mv",		 0, NULL }, /* Maldives */
	{ "za",		 0, NULL }, /* South Africa */
	{ "epo",	 0, epo_variants }, /* Esperanto */
	{ "np",		 KBD_NEPALI, NULL }, /* Nepal */
	{ "ng",		 0, ng_variants }, /* Nigeria */
	{ "et",		 0, NULL }, /* Ethiopia */
	{ "sn",		 0, NULL }, /* Senegal */
	{ "brai",	 0, brai_variants }, /* Braille */
	{ "tm",		 KBD_TURKISH_Q, tm_variants }, /* Turkmenistan */
};

UINT32 find_keyboard_layout_in_xorg_rules(char* layout, char* variant)
{
	int i, j;

	if ((layout == NULL) || (variant == NULL))
		return 0;

	DEBUG_KBD("xkbLayout: %s\txkbVariant: %s", layout, variant);

	for (i = 0; i < ARRAYSIZE(xkbLayouts); i++)
	{
		if (strcmp(xkbLayouts[i].layout, layout) == 0)
		{
			for (j = 0; xkbLayouts[i].variants[j].variant != NULL && strlen(xkbLayouts[i].variants[j].variant) > 0; j++)
			{
				if (strcmp(xkbLayouts[i].variants[j].variant, variant) == 0)
				{
					return xkbLayouts[i].variants[j].keyboardLayoutID;
				}
			}

			return xkbLayouts[i].keyboardLayoutID;
		}
	}

	return 0;
}
