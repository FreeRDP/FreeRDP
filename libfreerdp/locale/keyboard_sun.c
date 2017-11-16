/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Solaris Keyboard Mapping
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
#include <errno.h>

#include <winpr/crt.h>

#include "liblocale.h"

#include <freerdp/locale/keyboard.h>

#include "keyboard_x11.h"

#include "keyboard_sun.h"

/* OpenSolaris 2008.11 and 2009.06 keyboard layouts
 *
 * While OpenSolaris comes with Xorg and XKB, it maintains a set of keyboard layout
 * names that map directly to a particular keyboard layout in XKB. Fortunately for us,
 * this way of doing things comes from Solaris, which is XKB unaware. The same keyboard
 * layout naming system is used in Solaris, so we can use the same XKB configuration as
 * we would on OpenSolaris and get an accurate keyboard layout detection :)
 *
 * We can check for the current keyboard layout using the "kbd -l" command:
 *
 * type=6
 * layout=33 (0x21)
 * delay(ms)=500
 * rate(ms)=40
 *
 * We can check at runtime if the kbd utility is present, parse the output, and use the
 * keyboard layout indicated by the index given (in this case, 33, or US-English).
 */

struct _SOLARIS_KEYBOARD
{
	UINT32 type; /* Solaris keyboard type */
	UINT32 layout; /* Layout */
	char* xkbType; /* XKB keyboard */
	UINT32 keyboardLayoutId; /* XKB keyboard layout */
};
typedef struct _SOLARIS_KEYBOARD SOLARIS_KEYBOARD;

static const SOLARIS_KEYBOARD SOLARIS_KEYBOARD_TABLE[] =
{
	{ 4,   0,    "sun(type4)",               KBD_US					}, /*  US4 */
	{ 4,   1,    "sun(type4)",               KBD_US					}, /*  US4 */
	{ 4,   2,    "sun(type4tuv)",            KBD_FRENCH				}, /*  FranceBelg4 */
	{ 4,   3,    "sun(type4_ca)",            KBD_US					}, /*  Canada4 */
	{ 4,   4,    "sun(type4tuv)",            KBD_DANISH				}, /*  Denmark4 */
	{ 4,   5,    "sun(type4tuv)",            KBD_GERMAN				}, /*  Germany4 */
	{ 4,   6,    "sun(type4tuv)",            KBD_ITALIAN				}, /*  Italy4 */
	{ 4,   7,    "sun(type4tuv)",            KBD_DUTCH				}, /*  Netherland4 */
	{ 4,   8,    "sun(type4tuv)",            KBD_NORWEGIAN				}, /*  Norway4 */
	{ 4,   9,    "sun(type4tuv)",            KBD_PORTUGUESE				}, /*  Portugal4 */
	{ 4,   10,   "sun(type4tuv)",            KBD_SPANISH				}, /*  SpainLatAm4 */
	{ 4,   11,   "sun(type4tuv)",            KBD_SWEDISH				}, /*  SwedenFin4 */
	{ 4,   12,   "sun(type4tuv)",            KBD_SWISS_FRENCH			}, /*  Switzer_Fr4 */
	{ 4,   13,   "sun(type4tuv)",            KBD_SWISS_GERMAN			}, /*  Switzer_Ge4 */
	{ 4,   14,   "sun(type4tuv)",            KBD_UNITED_KINGDOM			}, /*  UK4 */
	{ 4,   16,   "sun(type4)",               KBD_KOREAN_INPUT_SYSTEM_IME_2000	}, /*  Korea4 */
	{ 4,   17,   "sun(type4)",               KBD_CHINESE_TRADITIONAL_PHONETIC	}, /*  Taiwan4 */
	{ 4,   32,   "sun(type4jp)",             KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002	}, /*  Japan4 */
	{ 4,   19,   "sun(type5)",               KBD_US					}, /*  US101A_PC */
	{ 4,   33,   "sun(type5)",               KBD_US					}, /*  US5 */
	{ 4,   34,   "sun(type5unix)",           KBD_US					}, /*  US_UNIX5 */
	{ 4,   35,   "sun(type5tuv)",            KBD_FRENCH				}, /*  France5 */
	{ 4,   36,   "sun(type5tuv)",            KBD_DANISH				}, /*  Denmark5 */
	{ 4,   37,   "sun(type5tuv)",            KBD_GERMAN				}, /*  Germany5 */
	{ 4,   38,   "sun(type5tuv)",            KBD_ITALIAN				}, /*  Italy5 */
	{ 4,   39,   "sun(type5tuv)",            KBD_DUTCH				}, /*  Netherland5 */
	{ 4,   40,   "sun(type5tuv)",            KBD_NORWEGIAN				}, /*  Norway5 */
	{ 4,   41,   "sun(type5tuv)",            KBD_PORTUGUESE				}, /*  Portugal5 */
	{ 4,   42,   "sun(type5tuv)",            KBD_SPANISH				}, /*  Spain5 */
	{ 4,   43,   "sun(type5tuv)",            KBD_SWEDISH				}, /*  Sweden5 */
	{ 4,   44,   "sun(type5tuv)",            KBD_SWISS_FRENCH			}, /*  Switzer_Fr5 */
	{ 4,   45,   "sun(type5tuv)",            KBD_SWISS_GERMAN			}, /*  Switzer_Ge5 */
	{ 4,   46,   "sun(type5tuv)",            KBD_UNITED_KINGDOM			}, /*  UK5 */
	{ 4,   47,   "sun(type5)",               KBD_KOREAN_INPUT_SYSTEM_IME_2000	}, /*  Korea5 */
	{ 4,   48,   "sun(type5)",               KBD_CHINESE_TRADITIONAL_PHONETIC	}, /*  Taiwan5 */
	{ 4,   49,   "sun(type5jp)",             KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002	}, /*  Japan5 */
	{ 4,   50,   "sun(type5tuv)",            KBD_CANADIAN_FRENCH			}, /*  Canada_Fr5 */
	{ 4,   51,   "sun(type5tuv)",            KBD_HUNGARIAN				}, /*  Hungary5 */
	{ 4,   52,   "sun(type5tuv)",            KBD_POLISH_214				}, /*  Poland5 */
	{ 4,   53,   "sun(type5tuv)",            KBD_CZECH				}, /*  Czech5 */
	{ 4,   54,   "sun(type5tuv)",            KBD_RUSSIAN				}, /*  Russia5 */
	{ 4,   55,   "sun(type5tuv)",            KBD_LATVIAN				}, /*  Latvia5 */
	{ 4,   57,   "sun(type5tuv)",            KBD_GREEK				}, /*  Greece5 */
	{ 4,   59,   "sun(type5tuv)",            KBD_LITHUANIAN				}, /*  Lithuania5 */
	{ 4,   63,   "sun(type5tuv)",            KBD_CANADIAN_FRENCH			}, /*  Canada_Fr5_TBITS5 */
	{ 4,   56,   "sun(type5tuv)",            KBD_TURKISH_Q				}, /*  TurkeyQ5 */
	{ 4,   58,   "sun(type5tuv)",            KBD_ARABIC_101				}, /*  Arabic5 */
	{ 4,   60,   "sun(type5tuv)",            KBD_BELGIAN_FRENCH			}, /*  Belgian5 */
	{ 4,   62,   "sun(type5tuv)",            KBD_TURKISH_F				}, /*  TurkeyF5 */
	{ 4,   80,   "sun(type5hobo)",           KBD_US					}, /*  US5_Hobo */
	{ 4,   81,   "sun(type5hobo)",           KBD_US					}, /*  US_UNIX5_Hobo */
	{ 4,   82,   "sun(type5tuvhobo)",        KBD_FRENCH				}, /*  France5_Hobo */
	{ 4,   83,   "sun(type5tuvhobo)",        KBD_DANISH				}, /*  Denmark5_Hobo */
	{ 4,   84,   "sun(type5tuvhobo)",        KBD_GERMAN				}, /*  Germany5_Hobo */
	{ 4,   85,   "sun(type5tuvhobo)",        KBD_ITALIAN				}, /*  Italy5_Hobo */
	{ 4,   86,   "sun(type5tuvhobo)",        KBD_DUTCH				}, /*  Netherland5_Hobo */
	{ 4,   87,   "sun(type5tuvhobo)",        KBD_NORWEGIAN				}, /*  Norway5_Hobo */
	{ 4,   88,   "sun(type5tuvhobo)",        KBD_PORTUGUESE				}, /*  Portugal5_Hobo */
	{ 4,   89,   "sun(type5tuvhobo)",        KBD_SPANISH				}, /*  Spain5_Hobo */
	{ 4,   90,   "sun(type5tuvhobo)",        KBD_SWEDISH				}, /*  Sweden5_Hobo */
	{ 4,   91,   "sun(type5tuvhobo)",        KBD_SWISS_FRENCH			}, /*  Switzer_Fr5_Hobo */
	{ 4,   92,   "sun(type5tuvhobo)",        KBD_SWISS_GERMAN			}, /*  Switzer_Ge5_Hobo */
	{ 4,   93,   "sun(type5tuvhobo)",        KBD_UNITED_KINGDOM			}, /*  UK5_Hobo */
	{ 4,   94,   "sun(type5hobo)",           KBD_KOREAN_INPUT_SYSTEM_IME_2000	}, /*  Korea5_Hobo */
	{ 4,   95,   "sun(type5hobo)",           KBD_CHINESE_TRADITIONAL_PHONETIC	}, /*  Taiwan5_Hobo */
	{ 4,   96,   "sun(type5jphobo)",         KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002	}, /*  Japan5_Hobo */
	{ 4,   97,   "sun(type5tuvhobo)",        KBD_CANADIAN_FRENCH			}, /*  Canada_Fr5_Hobo */
	{ 101, 1,    "digital_vndr/pc(pc104)",   KBD_US					}, /*  US101A_x86 */
	{ 101, 34,   "digital_vndr/pc(pc104)",   KBD_US					}, /*  J3100_x86 */
	{ 101, 35,   "digital_vndr/pc(pc104)",   KBD_FRENCH				}, /*  France_x86 */
	{ 101, 36,   "digital_vndr/pc(pc104)",   KBD_DANISH				}, /*  Denmark_x86 */
	{ 101, 37,   "digital_vndr/pc(pc104)",   KBD_GERMAN				}, /*  Germany_x86 */
	{ 101, 38,   "digital_vndr/pc(pc104)",   KBD_ITALIAN				}, /*  Italy_x86 */
	{ 101, 39,   "digital_vndr/pc(pc104)",   KBD_DUTCH				}, /*  Netherland_x86 */
	{ 101, 40,   "digital_vndr/pc(pc104)",   KBD_NORWEGIAN				}, /*  Norway_x86 */
	{ 101, 41,   "digital_vndr/pc(pc104)",   KBD_PORTUGUESE				}, /*  Portugal_x86 */
	{ 101, 42,   "digital_vndr/pc(pc104)",   KBD_SPANISH				}, /*  Spain_x86 */
	{ 101, 43,   "digital_vndr/pc(pc104)",   KBD_SWEDISH				}, /*  Sweden_x86 */
	{ 101, 44,   "digital_vndr/pc(pc104)",   KBD_SWISS_FRENCH			}, /*  Switzer_Fr_x86 */
	{ 101, 45,   "digital_vndr/pc(pc104)",   KBD_SWISS_GERMAN			}, /*  Switzer_Ge_x86 */
	{ 101, 46,   "digital_vndr/pc(pc104)",   KBD_UNITED_KINGDOM			}, /*  UK_x86 */
	{ 101, 47,   "digital_vndr/pc(pc104)",   KBD_KOREAN_INPUT_SYSTEM_IME_2000	}, /*  Korea_x86 */
	{ 101, 48,   "digital_vndr/pc(pc104)",   KBD_CHINESE_TRADITIONAL_PHONETIC	}, /*  Taiwan_x86 */
	{ 101, 49,   "digital_vndr/pc(lk411jj)", KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002	}, /*  Japan_x86 */
	{ 101, 50,   "digital_vndr/pc(pc104)",   KBD_CANADIAN_FRENCH			}, /*  Canada_Fr2_x86 */
	{ 101, 51,   "digital_vndr/pc(pc104)",   KBD_HUNGARIAN				}, /*  Hungary_x86 */
	{ 101, 52,   "digital_vndr/pc(pc104)",   KBD_POLISH_214				}, /*  Poland_x86 */
	{ 101, 53,   "digital_vndr/pc(pc104)",   KBD_CZECH				}, /*  Czech_x86 */
	{ 101, 54,   "digital_vndr/pc(pc104)",   KBD_RUSSIAN				}, /*  Russia_x86 */
	{ 101, 55,   "digital_vndr/pc(pc104)",   KBD_LATVIAN				}, /*  Latvia_x86 */
	{ 101, 56,   "digital_vndr/pc(pc104)",   KBD_TURKISH_Q				}, /*  Turkey_x86 */
	{ 101, 57,   "digital_vndr/pc(pc104)",   KBD_GREEK				}, /*  Greece_x86 */
	{ 101, 59,   "digital_vndr/pc(pc104)",   KBD_LITHUANIAN				}, /*  Lithuania_x86 */
	{ 101, 1001, "digital_vndr/pc(pc104)",   KBD_US					}, /*  MS_US101A_x86 */
	{ 6,   6,    "sun(type6tuv)",            KBD_DANISH				}, /*  Denmark6_usb */
	{ 6,   7,    "sun(type6tuv)",            KBD_FINNISH				}, /*  Finnish6_usb */
	{ 6,   8,    "sun(type6tuv)",            KBD_FRENCH				}, /*  France6_usb */
	{ 6,   9,    "sun(type6tuv)",            KBD_GERMAN				}, /*  Germany6_usb */
	{ 6,   14,   "sun(type6tuv)",            KBD_ITALIAN				}, /*  Italy6_usb */
	{ 6,   15,   "sun(type6jp)",             KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002	}, /*  Japan7_usb */
	{ 6,   16,   "sun(type6)",               KBD_KOREAN_INPUT_SYSTEM_IME_2000	}, /*  Korea6_usb */
	{ 6,   18,   "sun(type6tuv)",            KBD_DUTCH				}, /*  Netherland6_usb */
	{ 6,   19,   "sun(type6tuv)",            KBD_NORWEGIAN				}, /*  Norway6_usb */
	{ 6,   22,   "sun(type6tuv)",            KBD_PORTUGUESE				}, /*  Portugal6_usb */
	{ 6,   23,   "sun(type6tuv)",            KBD_RUSSIAN				}, /*  Russia6_usb */
	{ 6,   25,   "sun(type6tuv)",            KBD_SPANISH				}, /*  Spain6_usb */
	{ 6,   26,   "sun(type6tuv)",            KBD_SWEDISH				}, /*  Sweden6_usb */
	{ 6,   27,   "sun(type6tuv)",            KBD_SWISS_FRENCH			}, /*  Switzer_Fr6_usb */
	{ 6,   28,   "sun(type6tuv)",            KBD_SWISS_GERMAN			}, /*  Switzer_Ge6_usb */
	{ 6,   30,   "sun(type6)",               KBD_CHINESE_TRADITIONAL_PHONETIC	}, /*  Taiwan6_usb */
	{ 6,   32,   "sun(type6tuv)",            KBD_UNITED_KINGDOM			}, /*  UK6_usb */
	{ 6,   33,   "sun(type6)",               KBD_US					}, /*  US6_usb */
	{ 6,   1,    "sun(type6tuv)",            KBD_ARABIC_101				}, /*  Arabic6_usb */
	{ 6,   2,    "sun(type6tuv)",            KBD_BELGIAN_FRENCH			}, /*  Belgian6_usb */
	{ 6,   31,   "sun(type6tuv)",            KBD_TURKISH_Q				}, /*  TurkeyQ6_usb */
	{ 6,   35,   "sun(type6tuv)",            KBD_TURKISH_F				}, /*  TurkeyF6_usb */
	{ 6,   271,  "sun(type6jp)",             KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002	}, /*  Japan6_usb */
	{ 6,   264,  "sun(type6tuv)",            KBD_ALBANIAN				}, /*  Albanian6_usb */
	{ 6,   261,  "sun(type6tuv)",            KBD_BELARUSIAN				}, /*  Belarusian6_usb */
	{ 6,   260,  "sun(type6tuv)",            KBD_BULGARIAN				}, /*  Bulgarian6_usb */
	{ 6,   259,  "sun(type6tuv)",            KBD_CROATIAN				}, /*  Croatian6_usb */
	{ 6,   5,    "sun(type6tuv)",            KBD_CZECH				}, /*  Czech6_usb */
	{ 6,   4,    "sun(type6tuv)",            KBD_CANADIAN_FRENCH			}, /*  French-Canadian6_usb */
	{ 6,   12,   "sun(type6tuv)",            KBD_HUNGARIAN				}, /*  Hungarian6_usb */
	{ 6,   10,   "sun(type6tuv)",            KBD_GREEK				}, /*  Greek6_usb */
	{ 6,   17,   "sun(type6)",               KBD_LATIN_AMERICAN			}, /*  Latin-American6_usb */
	{ 6,   265,  "sun(type6tuv)",            KBD_LITHUANIAN				}, /*  Lithuanian6_usb */
	{ 6,   266,  "sun(type6tuv)",            KBD_LATVIAN				}, /*  Latvian6_usb */
	{ 6,   267,  "sun(type6tuv)",            KBD_FYRO_MACEDONIAN			}, /*  Macedonian6_usb */
	{ 6,   263,  "sun(type6tuv)",            KBD_MALTESE_47_KEY			}, /*  Malta_UK6_usb */
	{ 6,   262,  "sun(type6tuv)",            KBD_MALTESE_48_KEY			}, /*  Malta_US6_usb */
	{ 6,   21,   "sun(type6tuv)",            KBD_POLISH_214				}, /*  Polish6_usb */
	{ 6,   257,  "sun(type6tuv)",            KBD_SERBIAN_LATIN			}, /*  Serbia-And-Montenegro6_usb */
	{ 6,   256,  "sun(type6tuv)",            KBD_SLOVENIAN				}, /*  Slovenian6_usb */
	{ 6,   24,   "sun(type6tuv)",            KBD_SLOVAK				}, /*  Slovakian6_usb */
	{ 6,   3,    "sun(type6)",               KBD_CANADIAN_MULTILINGUAL_STANDARD	}, /*  Canada_Bi6_usb */
	{ 6,   272,  "sun(type6)",               KBD_PORTUGUESE_BRAZILIAN_ABNT		}  /*  Brazil6_usb */
};

int freerdp_get_solaris_keyboard_layout_and_type(int* type, int* layout)
{
	FILE* kbd;
	char* pch;
	char* beg;
	char* end;
	int rc = -1;
	char buffer[1024];
	/*
		Sample output for "kbd -t -l" :

		USB keyboard
		type=6
		layout=3 (0x03)
		delay(ms)=500
		rate(ms)=40
	*/
	*type = 0;
	*layout = 0;
	kbd = popen("kbd -t -l", "r");

	if (!kbd)
		return -1;

	while (fgets(buffer, sizeof(buffer), kbd) != NULL)
	{
		long val;

		if ((pch = strstr(buffer, "type=")) != NULL)
		{
			beg = pch + sizeof("type=") - 1;
			end = strchr(beg, '\n');
			end[0] = '\0';
			errno = 0;
			val = strtol(beg, NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				goto fail;

			*type = val;
		}
		else if ((pch = strstr(buffer, "layout=")) != NULL)
		{
			beg = pch + sizeof("layout=") - 1;
			end = strchr(beg, ' ');
			end[0] = '\0';
			errno = 0;
			val = strtol(beg, NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				goto fail;

			*layout = val;
		}
	}

	rc = 0;
fail:
	pclose(kbd);
	return rc;
}

DWORD freerdp_detect_solaris_keyboard_layout()
{
	int i;
	int type;
	int layout;

	if (freerdp_get_solaris_keyboard_layout_and_type(&type, &layout) < 0)
		return 0;

	for (i = 0; i < ARRAYSIZE(SOLARIS_KEYBOARD_TABLE); i++)
	{
		if (SOLARIS_KEYBOARD_TABLE[i].type == type)
		{
			if (SOLARIS_KEYBOARD_TABLE[i].layout == layout)
				return SOLARIS_KEYBOARD_TABLE[i].keyboardLayoutId;
		}
	}

	return 0;
}
