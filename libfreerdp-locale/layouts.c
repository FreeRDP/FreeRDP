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
#include "liblocale.h"

#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/locale/vkcodes.h>
#include <freerdp/locale/layouts.h>

struct _keyboardLayout
{
	uint32 code; /* Keyboard layout code */
	const char* name; /* Keyboard layout name */
};
typedef struct _keyboardLayout keyboardLayout;

/*
 * In Windows XP, this information is available in the system registry at
 * HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet001/Control/Keyboard Layouts/
 */

static const keyboardLayout keyboardLayouts[] =
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

typedef struct
{
	uint32 code; /* Keyboard layout code */
	uint16 id; /* Keyboard variant ID */
	const char* name; /* Keyboard layout variant name */

} keyboardLayoutVariant;

static const keyboardLayoutVariant keyboardLayoutVariants[] =
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

typedef struct
{
	uint32 code; /* Keyboard layout code */
	const char* fileName; /* IME file name */
	const char* name; /* Keyboard layout name */

} keyboardIME;


/* Global Input Method Editors (IME) */

static const keyboardIME keyboardIMEs[] =
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

const virtualKey virtualKeyboard[] =
{
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_LBUTTON"          , NULL   },
	{ 0x00, 0, "VK_RBUTTON"          , NULL   },
	{ 0x00, 0, "VK_CANCEL"           , NULL   },
	{ 0x00, 0, "VK_MBUTTON"          , NULL   },
	{ 0x00, 0, "VK_XBUTTON1"         , NULL   },
	{ 0x00, 0, "VK_XBUTTON2"         , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x0E, 0, "VK_BACK"             , "BKSP" },
	{ 0x0F, 0, "VK_TAB"              , "TAB"  },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_CLEAR"            , NULL   },
	{ 0x1C, 0, "VK_RETURN"           , "RTRN" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x2A, 0, "VK_SHIFT"            , "LFSH" },
	{ 0x00, 0, "VK_CONTROL"          , NULL   },
	{ 0x38, 0, "VK_MENU"             , "LALT" },
	{ 0x46, 1, "VK_PAUSE"            , "PAUS" },
	{ 0x3A, 0, "VK_CAPITAL"          , "CAPS" },
	{ 0x72, 0, "VK_KANA / VK_HANGUL" , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_JUNJA"            , NULL   },
	{ 0x00, 0, "VK_FINAL"            , NULL   },
	{ 0x71, 0, "VK_HANJA / VK_KANJI" , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x01, 0, "VK_ESCAPE"           , "ESC"  },
	{ 0x00, 0, "VK_CONVERT"          , NULL   },
	{ 0x00, 0, "VK_NONCONVERT"       , NULL   },
	{ 0x00, 0, "VK_ACCEPT"           , NULL   },
	{ 0x00, 0, "VK_MODECHANGE"       , NULL   },
	{ 0x39, 0, "VK_SPACE"            , "SPCE" },
	{ 0x49, 1, "VK_PRIOR"            , "PGUP" },
	{ 0x51, 1, "VK_NEXT"             , "PGDN" },
	{ 0x4F, 1, "VK_END"              , "END"  },
	{ 0x47, 1, "VK_HOME"             , "HOME" },
	{ 0x4B, 1, "VK_LEFT"             , "LEFT" },
	{ 0x48, 1, "VK_UP"               , "UP"   },
	{ 0x4D, 1, "VK_RIGHT"            , "RGHT" },
	{ 0x50, 1, "VK_DOWN"             , "DOWN" },
	{ 0x00, 0, "VK_SELECT"           , NULL   },
	{ 0x37, 1, "VK_PRINT"            , "PRSC" },
	{ 0x37, 1, "VK_EXECUTE"          , NULL   },
	{ 0x37, 1, "VK_SNAPSHOT"         , NULL   },
	{ 0x52, 1, "VK_INSERT"           , "INS"  },
	{ 0x53, 1, "VK_DELETE"           , "DELE" },
	{ 0x63, 0, "VK_HELP"             , NULL   },
	{ 0x0B, 0, "VK_KEY_0"            , "AE10" },
	{ 0x02, 0, "VK_KEY_1"            , "AE01" },
	{ 0x03, 0, "VK_KEY_2"            , "AE02" },
	{ 0x04, 0, "VK_KEY_3"            , "AE03" },
	{ 0x05, 0, "VK_KEY_4"            , "AE04" },
	{ 0x06, 0, "VK_KEY_5"            , "AE05" },
	{ 0x07, 0, "VK_KEY_6"            , "AE06" },
	{ 0x08, 0, "VK_KEY_7"            , "AE07" },
	{ 0x09, 0, "VK_KEY_8"            , "AE08" },
	{ 0x0A, 0, "VK_KEY_9"            , "AE09" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x1E, 0, "VK_KEY_A"            , "AC01" },
	{ 0x30, 0, "VK_KEY_B"            , "AB05" },
	{ 0x2E, 0, "VK_KEY_C"            , "AB03" },
	{ 0x20, 0, "VK_KEY_D"            , "AC03" },
	{ 0x12, 0, "VK_KEY_E"            , "AD03" },
	{ 0x21, 0, "VK_KEY_F"            , "AC04" },
	{ 0x22, 0, "VK_KEY_G"            , "AC05" },
	{ 0x23, 0, "VK_KEY_H"            , "AC06" },
	{ 0x17, 0, "VK_KEY_I"            , "AD08" },
	{ 0x24, 0, "VK_KEY_J"            , "AC07" },
	{ 0x25, 0, "VK_KEY_K"            , "AC08" },
	{ 0x26, 0, "VK_KEY_L"            , "AC09" },
	{ 0x32, 0, "VK_KEY_M"            , "AB07" },
	{ 0x31, 0, "VK_KEY_N"            , "AB06" },
	{ 0x18, 0, "VK_KEY_O"            , "AD09" },
	{ 0x19, 0, "VK_KEY_P"            , "AD10" },
	{ 0x10, 0, "VK_KEY_Q"            , "AD01" },
	{ 0x13, 0, "VK_KEY_R"            , "AD04" },
	{ 0x1F, 0, "VK_KEY_S"            , "AC02" },
	{ 0x14, 0, "VK_KEY_T"            , "AD05" },
	{ 0x16, 0, "VK_KEY_U"            , "AD07" },
	{ 0x2F, 0, "VK_KEY_V"            , "AB04" },
	{ 0x11, 0, "VK_KEY_W"            , "AD02" },
	{ 0x2D, 0, "VK_KEY_X"            , "AB02" },
	{ 0x15, 0, "VK_KEY_Y"            , "AD06" },
	{ 0x2C, 0, "VK_KEY_Z"            , "AB01" },
	{ 0x5B, 1, "VK_LWIN"             , "LWIN" },
	{ 0x5C, 1, "VK_RWIN"             , "RWIN" },
	{ 0x5D, 1, "VK_APPS"             , "COMP" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x5F, 0, "VK_SLEEP"            , NULL   },
	{ 0x52, 0, "VK_NUMPAD0"          , "KP0"  },
	{ 0x4F, 0, "VK_NUMPAD1"          , "KP1"  },
	{ 0x50, 0, "VK_NUMPAD2"          , "KP2"  },
	{ 0x51, 0, "VK_NUMPAD3"          , "KP3"  },
	{ 0x4B, 0, "VK_NUMPAD4"          , "KP4"  },
	{ 0x4C, 0, "VK_NUMPAD5"          , "KP5"  },
	{ 0x4D, 0, "VK_NUMPAD6"          , "KP6"  },
	{ 0x47, 0, "VK_NUMPAD7"          , "KP7"  },
	{ 0x48, 0, "VK_NUMPAD8"          , "KP8"  },
	{ 0x49, 0, "VK_NUMPAD9"          , "KP9"  },
	{ 0x37, 0, "VK_MULTIPLY"         , "KPMU" },
	{ 0x4E, 0, "VK_ADD"              , "KPAD" },
	{ 0x00, 0, "VK_SEPARATOR"        , NULL   },
	{ 0x4A, 0, "VK_SUBTRACT"         , "KPSU" },
	{ 0x53, 0, "VK_DECIMAL"          , "KPDL" },
	{ 0x35, 0, "VK_DIVIDE"           , "KPDV" },
	{ 0x3B, 0, "VK_F1"               , "FK01" },
	{ 0x3C, 0, "VK_F2"               , "FK02" },
	{ 0x3D, 0, "VK_F3"               , "FK03" },
	{ 0x3E, 0, "VK_F4"               , "FK04" },
	{ 0x3F, 0, "VK_F5"               , "FK05" },
	{ 0x40, 0, "VK_F6"               , "FK06" },
	{ 0x41, 0, "VK_F7"               , "FK07" },
	{ 0x42, 0, "VK_F8"               , "FK08" },
	{ 0x43, 0, "VK_F9"               , "FK09" },
	{ 0x44, 0, "VK_F10"              , "FK10" },
	{ 0x57, 0, "VK_F11"              , "FK11" },
	{ 0x58, 0, "VK_F12"              , "FK12" },
	{ 0x64, 0, "VK_F13"              , NULL   },
	{ 0x65, 0, "VK_F14"              , NULL   },
	{ 0x66, 0, "VK_F15"              , NULL   },
	{ 0x67, 0, "VK_F16"              , NULL   },
	{ 0x68, 0, "VK_F17"              , NULL   },
	{ 0x69, 0, "VK_F18"              , NULL   },
	{ 0x6A, 0, "VK_F19"              , NULL   },
	{ 0x6B, 0, "VK_F20"              , NULL   },
	{ 0x6C, 0, "VK_F21"              , NULL   },
	{ 0x6D, 0, "VK_F22"              , NULL   },
	{ 0x6E, 0, "VK_F23"              , NULL   },
	{ 0x6F, 0, "VK_F24"              , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x45, 0, "VK_NUMLOCK"          , "NMLK" },
	{ 0x46, 0, "VK_SCROLL"           , "SCLK" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x2A, 0, "VK_LSHIFT"           , NULL   },
	{ 0x36, 0, "VK_RSHIFT"           , "RTSH" },
	{ 0x1D, 0, "VK_LCONTROL"         , "LCTL" },
	{ 0x1D, 1, "VK_RCONTROL"         , "RCTL" },
	{ 0x38, 0, "VK_LMENU"            , NULL   },
	{ 0x38, 1, "VK_RMENU"            , "RALT" },
	{ 0x00, 0, "VK_BROWSER_BACK"     , NULL   },
	{ 0x00, 0, "VK_BROWSER_FORWARD"  , NULL   },
	{ 0x00, 0, "VK_BROWSER_REFRESH"  , NULL   },
	{ 0x00, 0, "VK_BROWSER_STOP"     , NULL   },
	{ 0x00, 0, "VK_BROWSER_SEARCH"   , NULL   },
	{ 0x00, 0, "VK_BROWSER_FAVORITES", NULL   },
	{ 0x00, 0, "VK_BROWSER_HOME"     , NULL   },
	{ 0x00, 0, "VK_VOLUME_MUTE"      , NULL   },
	{ 0x00, 0, "VK_VOLUME_DOWN"      , NULL   },
	{ 0x00, 0, "VK_VOLUME_UP"        , NULL   },
	{ 0x00, 0, "VK_MEDIA_NEXT_TRACK" , NULL   },
	{ 0x00, 0, "VK_MEDIA_PREV_TRACK" , NULL   },
	{ 0x00, 0, "VK_MEDIA_STOP"       , NULL   },
	{ 0x00, 0, "VK_MEDIA_PLAY_PAUSE" , NULL   },
	{ 0x00, 0, "VK_LAUNCH_MAIL"      , NULL   },
	{ 0x00, 0, "VK_MEDIA_SELECT"     , NULL   },
	{ 0x00, 0, "VK_LAUNCH_APP1"      , NULL   },
	{ 0x00, 0, "VK_LAUNCH_APP2"      , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x27, 0, "VK_OEM_1"            , "AC10" },
	{ 0x0D, 0, "VK_OEM_PLUS"         , "AE12" },
	{ 0x33, 0, "VK_OEM_COMMA"        , "AB08" },
	{ 0x0C, 0, "VK_OEM_MINUS"        , "AE11" },
	{ 0x34, 0, "VK_OEM_PERIOD"       , "AB09" },
	{ 0x35, 0, "VK_OEM_2"            , "AB10" },
	{ 0x29, 0, "VK_OEM_3"            , "TLDE" },
	{ 0x73, 0, "VK_ABNT_C1"          , "AB11" },
	{ 0x7E, 0, "VK_ABNT_C2"          , "I129" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x1A, 0, "VK_OEM_4"            , "AD11" },
	{ 0x2B, 0, "VK_OEM_5"            , "BKSL" },
	{ 0x1B, 0, "VK_OEM_6"            , "AD12" },
	{ 0x28, 0, "VK_OEM_7"            , "AC11" },
	{ 0x1D, 0, "VK_OEM_8"            , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x56, 0, "VK_OEM_102"          , "LSGT" },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_PROCESSKEY"       , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_PACKET"           , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	{ 0x00, 0, "VK_ATTN"             , NULL   },
	{ 0x00, 0, "VK_CRSEL"            , NULL   },
	{ 0x00, 0, "VK_EXSEL"            , NULL   },
	{ 0x00, 0, "VK_EREOF"            , NULL   },
	{ 0x00, 0, "VK_PLAY"             , NULL   },
	{ 0x62, 0, "VK_ZOOM"             , NULL   },
	{ 0x00, 0, "VK_NONAME"           , NULL   },
	{ 0x00, 0, "VK_PA1"              , NULL   },
	{ 0x00, 0, "VK_OEM_CLEAR"        , NULL   },
	{ 0x00, 0, ""                    , NULL   },
	/* end of 256 VK entries */
	{ 0x54, 0, ""                    , "LVL3" },
	{ 0x1C, 1, ""                    , "KPEN" }
};

rdpKeyboardLayout* get_keyboard_layouts(uint32 types)
{
	int num, length, i;
	rdpKeyboardLayout* layouts;

	num = 0;
	layouts = (rdpKeyboardLayout*) xmalloc((num + 1) * sizeof(rdpKeyboardLayout));

	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_STANDARD) != 0)
	{
		length = sizeof(keyboardLayouts) / sizeof(keyboardLayout);

		layouts = (rdpKeyboardLayout *) xrealloc(layouts, (num + length + 1) * sizeof(rdpKeyboardLayout));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = keyboardLayouts[i].code;
			strcpy(layouts[num].name, keyboardLayouts[i].name);
		}
	}
	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_VARIANT) != 0)
	{
		length = sizeof(keyboardLayoutVariants) / sizeof(keyboardLayoutVariant);
		layouts = (rdpKeyboardLayout *) xrealloc(layouts, (num + length + 1) * sizeof(rdpKeyboardLayout));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = keyboardLayoutVariants[i].code;
			strcpy(layouts[num].name, keyboardLayoutVariants[i].name);
		}
	}
	if ((types & RDP_KEYBOARD_LAYOUT_TYPE_IME) != 0)
	{
		length = sizeof(keyboardIMEs) / sizeof(keyboardIME);
		layouts = (rdpKeyboardLayout *) realloc(layouts, (num + length + 1) * sizeof(rdpKeyboardLayout));

		for (i = 0; i < length; i++, num++)
		{
			layouts[num].code = keyboardIMEs[i].code;
			strcpy(layouts[num].name, keyboardIMEs[i].name);
		}
	}

	memset(&layouts[num], 0, sizeof(rdpKeyboardLayout));

	return layouts;
}

const char* get_layout_name(uint32 keyboardLayoutID)
{
	int i;

	for (i = 0; i < sizeof(keyboardLayouts) / sizeof(keyboardLayout); i++)
	{
		if (keyboardLayouts[i].code == keyboardLayoutID)
			return keyboardLayouts[i].name;
	}

	for (i = 0; i < sizeof(keyboardLayoutVariants) / sizeof(keyboardLayoutVariant); i++)
	{
		if (keyboardLayoutVariants[i].code == keyboardLayoutID)
			return keyboardLayoutVariants[i].name;
	}

	for (i = 0; i < sizeof(keyboardIMEs) / sizeof(keyboardIME); i++)
	{
		if (keyboardIMEs[i].code == keyboardLayoutID)
			return keyboardIMEs[i].name;
	}

	return "unknown";
}
