/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Time Zone Redirection
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#include <freerdp/utils/time.h>
#include <freerdp/utils/memory.h>

#include <freerdp/locale/timezone.h>

/* Time Zone Redirection table generated with TimeZones.cs script */

struct _SYSTEM_TIME_ENTRY
{
	uint16 wYear;
	uint16 wMonth;
	uint16 wDayOfWeek;
	uint16 wDay;
	uint16 wHour;
	uint16 wMinute;
	uint16 wSecond;
	uint16 wMilliseconds;
};
typedef struct _SYSTEM_TIME_ENTRY SYSTEM_TIME_ENTRY;

struct _TIME_ZONE_RULE_ENTRY
{
	uint64 TicksStart;
	uint64 TicksEnd;
	sint32 DaylightDelta;
	SYSTEM_TIME_ENTRY StandardDate;
	SYSTEM_TIME_ENTRY DaylightDate;
};
typedef struct _TIME_ZONE_RULE_ENTRY TIME_ZONE_RULE_ENTRY;

struct _TIME_ZONE_ENTRY
{
	const char* Id;
	uint32 Bias;
	boolean SupportsDST;
	const char* DisplayName;
	const char* StandardName;
	const char* DaylightName;
	TIME_ZONE_RULE_ENTRY* RuleTable;
	uint32 RuleTableCount;
};
typedef struct _TIME_ZONE_ENTRY TIME_ZONE_ENTRY;

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_3[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_4[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_5[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_7[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_8[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_10[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_11[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_14[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_15[] =
{
	{ 633031488000000000ULL, 632716992000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_17[] =
{
	{ 633663072000000000ULL, 288000000000ULL, 60, { 0, 3, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 3, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635240736000000000ULL, 634926240000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635556096000000000ULL, 635241600000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635871456000000000ULL, 635556960000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636187680000000000ULL, 635872320000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636503040000000000ULL, 636188544000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 9, 6, 1, 23, 59 }, },
	{ 636818400000000000ULL, 636503904000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637133760000000000ULL, 636819264000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637449984000000000ULL, 637134624000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 637450848000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_18[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_19[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 2, 0, 1, 2, 0 }, { 0, 11, 0, 1, 0, 0 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 2, 0, 1, 0, 0 }, { 0, 10, 0, 1, 0, 0 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 2, 0, 1, 0, 0 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635240736000000000ULL, 634926240000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635556096000000000ULL, 635241600000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635871456000000000ULL, 635556960000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636187680000000000ULL, 635872320000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636503040000000000ULL, 636188544000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636818400000000000ULL, 636503904000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637133760000000000ULL, 636819264000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637449984000000000ULL, 637134624000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637765344000000000ULL, 637450848000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638080704000000000ULL, 637766208000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638396064000000000ULL, 638081568000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638712288000000000ULL, 638396928000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639027648000000000ULL, 638713152000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639343008000000000ULL, 639028512000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639658368000000000ULL, 639343872000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639974592000000000ULL, 639659232000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640289952000000000ULL, 639975456000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640605312000000000ULL, 640290816000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640920672000000000ULL, 640606176000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641236896000000000ULL, 640921536000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641552256000000000ULL, 641237760000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641867616000000000ULL, 641553120000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642182976000000000ULL, 641868480000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642499200000000000ULL, 642183840000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642814560000000000ULL, 642500064000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 643129920000000000ULL, 642815424000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 643445280000000000ULL, 643130784000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 643446144000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_21[] =
{
	{ 633346848000000000ULL, 288000000000ULL, 60, { 0, 3, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 3, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 3, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 4, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 5, 6, 1, 23, 59 }, { 0, 8, 6, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 634610016000000000ULL, 60, { 0, 3, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_22[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 0, 1 }, { 0, 4, 0, 1, 0, 1 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 11, 0, 1, 0, 1 }, { 0, 3, 0, 1, 0, 1 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 11, 0, 1, 0, 1 }, { 0, 3, 0, 1, 0, 1 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 11, 0, 1, 0, 1 }, { 0, 3, 0, 1, 0, 1 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 11, 0, 1, 0, 1 }, { 0, 3, 0, 1, 0, 1 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 0, 1 }, },
	{ 3155378400000000000ULL, 634610016000000000ULL, 60, { 0, 11, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_23[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 2, 0, 1, 2, 0 }, { 0, 11, 0, 1, 0, 0 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 2, 0, 1, 0, 0 }, { 0, 10, 0, 1, 0, 0 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 2, 0, 1, 0, 0 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635240736000000000ULL, 634926240000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635556096000000000ULL, 635241600000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635871456000000000ULL, 635556960000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636187680000000000ULL, 635872320000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636503040000000000ULL, 636188544000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636818400000000000ULL, 636503904000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637133760000000000ULL, 636819264000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637449984000000000ULL, 637134624000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637765344000000000ULL, 637450848000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638080704000000000ULL, 637766208000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638396064000000000ULL, 638081568000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638712288000000000ULL, 638396928000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639027648000000000ULL, 638713152000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639343008000000000ULL, 639028512000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639658368000000000ULL, 639343872000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639974592000000000ULL, 639659232000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640289952000000000ULL, 639975456000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640605312000000000ULL, 640290816000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640920672000000000ULL, 640606176000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641236896000000000ULL, 640921536000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641552256000000000ULL, 641237760000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641867616000000000ULL, 641553120000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642182976000000000ULL, 641868480000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642499200000000000ULL, 642183840000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642814560000000000ULL, 642500064000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 643129920000000000ULL, 642815424000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 643445280000000000ULL, 643130784000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 643446144000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_24[] =
{
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 1, 1, 1, 0, 0 }, { 0, 12, 0, 1, 0, 0 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 3, 0, 1, 0, 0 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 3, 6, 1, 23, 59 }, { 0, 1, 4, 1, 0, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_26[] =
{
	{ 633663072000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 635240736000000000ULL, 634926240000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 635556096000000000ULL, 635241600000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 635871456000000000ULL, 635556960000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 636187680000000000ULL, 635872320000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 636503040000000000ULL, 636188544000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 636818400000000000ULL, 636503904000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 637133760000000000ULL, 636819264000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 637449984000000000ULL, 637134624000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, },
	{ 3155378400000000000ULL, 637450848000000000ULL, 60, { 0, 10, 6, 1, 23, 0 }, { 0, 3, 6, 1, 22, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_27[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 3, 0, 1, 2, 0 }, { 0, 9, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 3, 0, 1, 2, 0 }, { 0, 10, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_28[] =
{
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 1, 6, 1, 0, 0 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635240736000000000ULL, 634926240000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635556096000000000ULL, 635241600000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 635871456000000000ULL, 635556960000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636187680000000000ULL, 635872320000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636503040000000000ULL, 636188544000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 636818400000000000ULL, 636503904000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637133760000000000ULL, 636819264000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637449984000000000ULL, 637134624000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 637765344000000000ULL, 637450848000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638080704000000000ULL, 637766208000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638396064000000000ULL, 638081568000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 638712288000000000ULL, 638396928000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639027648000000000ULL, 638713152000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639343008000000000ULL, 639028512000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639658368000000000ULL, 639343872000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 639974592000000000ULL, 639659232000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640289952000000000ULL, 639975456000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640605312000000000ULL, 640290816000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 640920672000000000ULL, 640606176000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641236896000000000ULL, 640921536000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641552256000000000ULL, 641237760000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 641867616000000000ULL, 641553120000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642182976000000000ULL, 641868480000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642499200000000000ULL, 642183840000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 642814560000000000ULL, 642500064000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 643129920000000000ULL, 642815424000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 643445280000000000ULL, 643130784000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 643446144000000000ULL, 60, { 0, 2, 6, 1, 23, 59 }, { 0, 10, 6, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_30[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_31[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_33[] =
{
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 8, 0, 1, 23, 59 }, { 0, 5, 6, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 8, 4, 1, 23, 59 }, { 0, 5, 0, 1, 23, 59 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 8, 6, 1, 23, 59 }, { 0, 5, 6, 1, 23, 59 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 7, 6, 1, 23, 59 }, { 0, 4, 6, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_35[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 3, 0, 1, 1, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_37[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_38[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_39[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_40[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_42[] =
{
	{ 634293792000000000ULL, 288000000000ULL, -60, { 0, 9, 0, 1, 2, 0 }, { 0, 4, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 634294656000000000ULL, 60, { 0, 4, 0, 1, 2, 0 }, { 0, 9, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_43[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 9, 5, 1, 1, 0 }, { 0, 3, 4, 1, 0, 0 }, },
	{ 3155378400000000000ULL, 633032352000000000ULL, 60, { 0, 10, 5, 1, 1, 0 }, { 0, 3, 4, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_44[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 4, 0 }, { 0, 3, 0, 1, 3, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_45[] =
{
	{ 633978432000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 0, 0 }, { 0, 3, 0, 1, 0, 0 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 635240736000000000ULL, 634926240000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 635556096000000000ULL, 635241600000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 635871456000000000ULL, 635556960000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 636187680000000000ULL, 635872320000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 636503040000000000ULL, 636188544000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 636818400000000000ULL, 636503904000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 637133760000000000ULL, 636819264000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 637449984000000000ULL, 637134624000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 637450848000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_46[] =
{
	{ 632716128000000000ULL, 288000000000ULL, 60, { 0, 9, 4, 1, 23, 59 }, { 0, 4, 5, 1, 0, 0 }, },
	{ 633031488000000000ULL, 632716992000000000ULL, 60, { 0, 9, 4, 1, 23, 59 }, { 0, 4, 5, 1, 0, 0 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 9, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 8, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 8, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 9, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_47[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 9, 3, 1, 23, 59 }, { 0, 3, 5, 1, 23, 59 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 11, 4, 1, 23, 59 }, { 0, 3, 4, 1, 23, 59 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 10, 5, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 3, 4, 1, 23, 59 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 3, 4, 1, 23, 59 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 635240736000000000ULL, 634926240000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 635556096000000000ULL, 635241600000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 635871456000000000ULL, 635556960000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, },
	{ 636187680000000000ULL, 635872320000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 3, 4, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 636188544000000000ULL, 60, { 0, 10, 4, 1, 23, 59 }, { 0, 4, 4, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_49[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 4, 0 }, { 0, 3, 0, 1, 3, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_50[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 4, 0 }, { 0, 3, 0, 1, 3, 0 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 10, 0, 1, 4, 0 }, { 0, 3, 1, 1, 3, 0 }, },
	{ 3155378400000000000ULL, 634610016000000000ULL, 60, { 0, 10, 0, 1, 4, 0 }, { 0, 3, 0, 1, 3, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_51[] =
{
	{ 632716128000000000ULL, 632401632000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 5, 1, 2, 0 }, },
	{ 633031488000000000ULL, 632716992000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 5, 1, 2, 0 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 635240736000000000ULL, 634926240000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 635556096000000000ULL, 635241600000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 635871456000000000ULL, 635556960000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 636187680000000000ULL, 635872320000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 5, 1, 2, 0 }, },
	{ 636503040000000000ULL, 636188544000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 636818400000000000ULL, 636503904000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 637133760000000000ULL, 636819264000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 637449984000000000ULL, 637134624000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 637765344000000000ULL, 637450848000000000ULL, 60, { 0, 9, 0, 1, 2, 0 }, { 0, 3, 5, 1, 2, 0 }, },
	{ 638080704000000000ULL, 637766208000000000ULL, 60, { 0, 10, 0, 1, 2, 0 }, { 0, 4, 5, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_52[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_53[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 4, 0 }, { 0, 4, 0, 1, 3, 0 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 10, 1, 1, 4, 0 }, { 0, 4, 0, 1, 3, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_54[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_57[] =
{
	{ 632716128000000000ULL, 288000000000ULL, 60, { 0, 9, 2, 1, 2, 0 }, { 0, 3, 0, 1, 2, 0 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 9, 6, 1, 23, 59 }, { 0, 3, 4, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 633663936000000000ULL, 60, { 0, 9, 1, 1, 23, 59 }, { 0, 3, 6, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_59[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 5, 0 }, { 0, 3, 0, 1, 4, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_60[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_61[] =
{
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 1, 2, 1, 0, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 3, 0, 1, 2, 0 }, { 0, 1, 4, 1, 0, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_63[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_65[] =
{
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 10, 5, 1, 23, 59 }, { 0, 5, 6, 1, 23, 59 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 10, 6, 1, 23, 59 }, { 0, 4, 2, 1, 23, 59 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_71[] =
{
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 12, 4, 1, 23, 59 }, { 0, 6, 5, 1, 23, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_72[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_75[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_77[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_79[] =
{
	{ 633031488000000000ULL, 632716992000000000ULL, 60, { 0, 1, 0, 1, 0, 0 }, { 0, 12, 0, 1, 2, 0 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 633663072000000000ULL, 633347712000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 1, 4, 1, 0, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_82[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_85[] =
{
	{ 633346848000000000ULL, 288000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633347712000000000ULL, 60, { 0, 4, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_88[] =
{
	{ 633346848000000000ULL, 288000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633347712000000000ULL, 60, { 0, 4, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_90[] =
{
	{ 633346848000000000ULL, 288000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633347712000000000ULL, 60, { 0, 4, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_91[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_93[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_94[] =
{
	{ 633031488000000000ULL, 288000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 633346848000000000ULL, 633032352000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 9, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 633347712000000000ULL, 60, { 0, 4, 0, 1, 3, 0 }, { 0, 9, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_96[] =
{
	{ 633978432000000000ULL, 633663936000000000ULL, 60, { 0, 1, 4, 1, 0, 0 }, { 0, 11, 0, 1, 2, 0 }, },
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 634609152000000000ULL, 634294656000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 634925376000000000ULL, 634610016000000000ULL, 60, { 0, 1, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, },
	{ 3155378400000000000ULL, 634926240000000000ULL, 60, { 0, 3, 0, 1, 3, 0 }, { 0, 10, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_97[] =
{
	{ 634293792000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_98[] =
{
	{ 3155378400000000000ULL, 288000000000ULL, 60, { 0, 10, 0, 1, 3, 0 }, { 0, 3, 0, 1, 2, 0 }, }
};

static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_100[] =
{
	{ 634293792000000000ULL, 633979296000000000ULL, 60, { 0, 1, 5, 1, 0, 0 }, { 0, 9, 6, 1, 23, 59 }, },
	{ 3155378400000000000ULL, 634294656000000000ULL, 60, { 0, 4, 0, 1, 1, 0 }, { 0, 9, 0, 1, 0, 0 }, }
};

static const TIME_ZONE_ENTRY TimeZoneTable[] =
{
	{
		"Dateline Standard Time", 1440, false, "Dateline Standard Time",
		"Dateline Standard Time", "Dateline Standard Time",
		NULL, 0
	},
	{
		"UTC-11", 1380, false, "UTC-11",
		"UTC-11", "UTC-11",
		NULL, 0
	},
	{
		"Hawaiian Standard Time", 1320, false, "Hawaiian Standard Time",
		"Hawaiian Standard Time", "Hawaiian Standard Time",
		NULL, 0
	},
	{
		"Alaskan Standard Time", 1260, true, "Alaskan Standard Time",
		"Alaskan Standard Time", "Alaskan Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_3, 2
	},
	{
		"Pacific Standard Time (Mexico)", 1200, true, "Pacific Standard Time (Mexico)",
		"Pacific Standard Time (Mexico)", "Pacific Standard Time (Mexico)",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_4, 1
	},
	{
		"Pacific Standard Time", 1200, true, "Pacific Standard Time",
		"Pacific Standard Time", "Pacific Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_5, 2
	},
	{
		"US Mountain Standard Time", 1140, false, "US Mountain Standard Time",
		"US Mountain Standard Time", "US Mountain Standard Time",
		NULL, 0
	},
	{
		"Mountain Standard Time (Mexico)", 1140, true, "Mountain Standard Time (Mexico)",
		"Mountain Standard Time (Mexico)", "Mountain Standard Time (Mexico)",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_7, 1
	},
	{
		"Mountain Standard Time", 1140, true, "Mountain Standard Time",
		"Mountain Standard Time", "Mountain Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_8, 2
	},
	{
		"Central America Standard Time", 1080, false, "Central America Standard Time",
		"Central America Standard Time", "Central America Standard Time",
		NULL, 0
	},
	{
		"Central Standard Time", 1080, true, "Central Standard Time",
		"Central Standard Time", "Central Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_10, 2
	},
	{
		"Central Standard Time (Mexico)", 1080, true, "Central Standard Time (Mexico)",
		"Central Standard Time (Mexico)", "Central Standard Time (Mexico)",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_11, 1
	},
	{
		"Canada Central Standard Time", 1080, false, "Canada Central Standard Time",
		"Canada Central Standard Time", "Canada Central Standard Time",
		NULL, 0
	},
	{
		"SA Pacific Standard Time", 1020, false, "SA Pacific Standard Time",
		"SA Pacific Standard Time", "SA Pacific Standard Time",
		NULL, 0
	},
	{
		"Eastern Standard Time", 1020, true, "Eastern Standard Time",
		"Eastern Standard Time", "Eastern Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_14, 2
	},
	{
		"US Eastern Standard Time", 1020, true, "US Eastern Standard Time",
		"US Eastern Standard Time", "US Eastern Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_15, 2
	},
	{
		"Venezuela Standard Time", 930, false, "Venezuela Standard Time",
		"Venezuela Standard Time", "Venezuela Standard Time",
		NULL, 0
	},
	{
		"Paraguay Standard Time", 960, true, "Paraguay Standard Time",
		"Paraguay Standard Time", "Paraguay Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_17, 14
	},
	{
		"Atlantic Standard Time", 960, true, "Atlantic Standard Time",
		"Atlantic Standard Time", "Atlantic Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_18, 2
	},
	{
		"Central Brazilian Standard Time", 960, true, "Central Brazilian Standard Time",
		"Central Brazilian Standard Time", "Central Brazilian Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_19, 35
	},
	{
		"SA Western Standard Time", 960, false, "SA Western Standard Time",
		"SA Western Standard Time", "SA Western Standard Time",
		NULL, 0
	},
	{
		"Pacific SA Standard Time", 960, true, "Pacific SA Standard Time",
		"Pacific SA Standard Time", "Pacific SA Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_21, 6
	},
	{
		"Newfoundland Standard Time", 870, true, "Newfoundland Standard Time",
		"Newfoundland Standard Time", "Newfoundland Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_22, 7
	},
	{
		"E. South America Standard Time", 900, true, "E. South America Standard Time",
		"E. South America Standard Time", "E. South America Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_23, 35
	},
	{
		"Argentina Standard Time", 900, true, "Argentina Standard Time",
		"Argentina Standard Time", "Argentina Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_24, 3
	},
	{
		"SA Eastern Standard Time", 900, false, "SA Eastern Standard Time",
		"SA Eastern Standard Time", "SA Eastern Standard Time",
		NULL, 0
	},
	{
		"Greenland Standard Time", 900, true, "Greenland Standard Time",
		"Greenland Standard Time", "Greenland Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_26, 14
	},
	{
		"Montevideo Standard Time", 900, true, "Montevideo Standard Time",
		"Montevideo Standard Time", "Montevideo Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_27, 2
	},
	{
		"Bahia Standard Time", 900, true, "Bahia Standard Time",
		"Bahia Standard Time", "Bahia Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_28, 30
	},
	{
		"UTC-02", 840, false, "UTC-02",
		"UTC-02", "UTC-02",
		NULL, 0
	},
	{
		"Mid-Atlantic Standard Time", 840, true, "Mid-Atlantic Standard Time",
		"Mid-Atlantic Standard Time", "Mid-Atlantic Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_30, 1
	},
	{
		"Azores Standard Time", 780, true, "Azores Standard Time",
		"Azores Standard Time", "Azores Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_31, 1
	},
	{
		"Cape Verde Standard Time", 780, false, "Cape Verde Standard Time",
		"Cape Verde Standard Time", "Cape Verde Standard Time",
		NULL, 0
	},
	{
		"Morocco Standard Time", 0, true, "Morocco Standard Time",
		"Morocco Standard Time", "Morocco Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_33, 4
	},
	{
		"UTC", 0, false, "UTC",
		"Coordinated Universal Time", "Coordinated Universal Time",
		NULL, 0
	},
	{
		"GMT Standard Time", 0, true, "GMT Standard Time",
		"GMT Standard Time", "GMT Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_35, 1
	},
	{
		"Greenwich Standard Time", 0, false, "Greenwich Standard Time",
		"Greenwich Standard Time", "Greenwich Standard Time",
		NULL, 0
	},
	{
		"W. Europe Standard Time", 60, true, "W. Europe Standard Time",
		"W. Europe Standard Time", "W. Europe Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_37, 1
	},
	{
		"Central Europe Standard Time", 60, true, "Central Europe Standard Time",
		"Central Europe Standard Time", "Central Europe Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_38, 1
	},
	{
		"Romance Standard Time", 60, true, "Romance Standard Time",
		"Romance Standard Time", "Romance Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_39, 1
	},
	{
		"Central European Standard Time", 60, true, "Central European Standard Time",
		"Central European Standard Time", "Central European Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_40, 1
	},
	{
		"W. Central Africa Standard Time", 60, false, "W. Central Africa Standard Time",
		"W. Central Africa Standard Time", "W. Central Africa Standard Time",
		NULL, 0
	},
	{
		"Namibia Standard Time", 60, true, "Namibia Standard Time",
		"Namibia Standard Time", "Namibia Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_42, 2
	},
	{
		"Jordan Standard Time", 120, true, "Jordan Standard Time",
		"Jordan Standard Time", "Jordan Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_43, 2
	},
	{
		"GTB Standard Time", 120, true, "GTB Standard Time",
		"GTB Standard Time", "GTB Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_44, 1
	},
	{
		"Middle East Standard Time", 120, true, "Middle East Standard Time",
		"Middle East Standard Time", "Middle East Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_45, 13
	},
	{
		"Egypt Standard Time", 120, true, "Egypt Standard Time",
		"Egypt Standard Time", "Egypt Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_46, 6
	},
	{
		"Syria Standard Time", 120, true, "Syria Standard Time",
		"Syria Standard Time", "Syria Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_47, 12
	},
	{
		"South Africa Standard Time", 120, false, "South Africa Standard Time",
		"South Africa Standard Time", "South Africa Standard Time",
		NULL, 0
	},
	{
		"FLE Standard Time", 120, true, "FLE Standard Time",
		"FLE Standard Time", "FLE Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_49, 1
	},
	{
		"Turkey Standard Time", 120, true, "Turkey Standard Time",
		"Turkey Standard Time", "Turkey Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_50, 3
	},
	{
		"Israel Standard Time", 120, true, "Israel Standard Time",
		"Jerusalem Standard Time", "Jerusalem Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_51, 18
	},
	{
		"E. Europe Standard Time", 120, true, "E. Europe Standard Time",
		"E. Europe Standard Time", "E. Europe Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_52, 1
	},
	{
		"Arabic Standard Time", 180, true, "Arabic Standard Time",
		"Arabic Standard Time", "Arabic Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_53, 2
	},
	{
		"Kaliningrad Standard Time", 180, true, "Kaliningrad Standard Time",
		"Kaliningrad Standard Time", "Kaliningrad Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_54, 1
	},
	{
		"Arab Standard Time", 180, false, "Arab Standard Time",
		"Arab Standard Time", "Arab Standard Time",
		NULL, 0
	},
	{
		"E. Africa Standard Time", 180, false, "E. Africa Standard Time",
		"E. Africa Standard Time", "E. Africa Standard Time",
		NULL, 0
	},
	{
		"Iran Standard Time", 210, true, "Iran Standard Time",
		"Iran Standard Time", "Iran Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_57, 3
	},
	{
		"Arabian Standard Time", 240, false, "Arabian Standard Time",
		"Arabian Standard Time", "Arabian Standard Time",
		NULL, 0
	},
	{
		"Azerbaijan Standard Time", 240, true, "Azerbaijan Standard Time",
		"Azerbaijan Standard Time", "Azerbaijan Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_59, 1
	},
	{
		"Russian Standard Time", 240, true, "Russian Standard Time",
		"Russian Standard Time", "Russian Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_60, 1
	},
	{
		"Mauritius Standard Time", 240, true, "Mauritius Standard Time",
		"Mauritius Standard Time", "Mauritius Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_61, 2
	},
	{
		"Georgian Standard Time", 240, false, "Georgian Standard Time",
		"Georgian Standard Time", "Georgian Standard Time",
		NULL, 0
	},
	{
		"Caucasus Standard Time", 240, true, "Caucasus Standard Time",
		"Caucasus Standard Time", "Caucasus Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_63, 1
	},
	{
		"Afghanistan Standard Time", 270, false, "Afghanistan Standard Time",
		"Afghanistan Standard Time", "Afghanistan Standard Time",
		NULL, 0
	},
	{
		"Pakistan Standard Time", 300, true, "Pakistan Standard Time",
		"Pakistan Standard Time", "Pakistan Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_65, 2
	},
	{
		"West Asia Standard Time", 300, false, "West Asia Standard Time",
		"West Asia Standard Time", "West Asia Standard Time",
		NULL, 0
	},
	{
		"India Standard Time", 330, false, "India Standard Time",
		"India Standard Time", "India Standard Time",
		NULL, 0
	},
	{
		"Sri Lanka Standard Time", 330, false, "Sri Lanka Standard Time",
		"Sri Lanka Standard Time", "Sri Lanka Standard Time",
		NULL, 0
	},
	{
		"Nepal Standard Time", 345, false, "Nepal Standard Time",
		"Nepal Standard Time", "Nepal Standard Time",
		NULL, 0
	},
	{
		"Central Asia Standard Time", 360, false, "Central Asia Standard Time",
		"Central Asia Standard Time", "Central Asia Standard Time",
		NULL, 0
	},
	{
		"Bangladesh Standard Time", 360, true, "Bangladesh Standard Time",
		"Bangladesh Standard Time", "Bangladesh Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_71, 1
	},
	{
		"Ekaterinburg Standard Time", 360, true, "Ekaterinburg Standard Time",
		"Ekaterinburg Standard Time", "Ekaterinburg Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_72, 1
	},
	{
		"Myanmar Standard Time", 390, false, "Myanmar Standard Time",
		"Myanmar Standard Time", "Myanmar Standard Time",
		NULL, 0
	},
	{
		"SE Asia Standard Time", 420, false, "SE Asia Standard Time",
		"SE Asia Standard Time", "SE Asia Standard Time",
		NULL, 0
	},
	{
		"N. Central Asia Standard Time", 420, true, "N. Central Asia Standard Time",
		"N. Central Asia Standard Time", "N. Central Asia Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_75, 1
	},
	{
		"China Standard Time", 480, false, "China Standard Time",
		"China Standard Time", "China Standard Time",
		NULL, 0
	},
	{
		"North Asia Standard Time", 480, true, "North Asia Standard Time",
		"North Asia Standard Time", "North Asia Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_77, 1
	},
	{
		"Singapore Standard Time", 480, false, "Singapore Standard Time",
		"Malay Peninsula Standard Time", "Malay Peninsula Standard Time",
		NULL, 0
	},
	{
		"W. Australia Standard Time", 480, true, "W. Australia Standard Time",
		"W. Australia Standard Time", "W. Australia Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_79, 4
	},
	{
		"Taipei Standard Time", 480, false, "Taipei Standard Time",
		"Taipei Standard Time", "Taipei Standard Time",
		NULL, 0
	},
	{
		"Ulaanbaatar Standard Time", 480, false, "Ulaanbaatar Standard Time",
		"Ulaanbaatar Standard Time", "Ulaanbaatar Standard Time",
		NULL, 0
	},
	{
		"North Asia East Standard Time", 540, true, "North Asia East Standard Time",
		"North Asia East Standard Time", "North Asia East Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_82, 1
	},
	{
		"Tokyo Standard Time", 540, false, "Tokyo Standard Time",
		"Tokyo Standard Time", "Tokyo Standard Time",
		NULL, 0
	},
	{
		"Korea Standard Time", 540, false, "Korea Standard Time",
		"Korea Standard Time", "Korea Standard Time",
		NULL, 0
	},
	{
		"Cen. Australia Standard Time", 570, true, "Cen. Australia Standard Time",
		"Cen. Australia Standard Time", "Cen. Australia Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_85, 2
	},
	{
		"AUS Central Standard Time", 570, false, "AUS Central Standard Time",
		"AUS Central Standard Time", "AUS Central Standard Time",
		NULL, 0
	},
	{
		"E. Australia Standard Time", 600, false, "E. Australia Standard Time",
		"E. Australia Standard Time", "E. Australia Standard Time",
		NULL, 0
	},
	{
		"AUS Eastern Standard Time", 600, true, "AUS Eastern Standard Time",
		"AUS Eastern Standard Time", "AUS Eastern Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_88, 2
	},
	{
		"West Pacific Standard Time", 600, false, "West Pacific Standard Time",
		"West Pacific Standard Time", "West Pacific Standard Time",
		NULL, 0
	},
	{
		"Tasmania Standard Time", 600, true, "Tasmania Standard Time",
		"Tasmania Standard Time", "Tasmania Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_90, 2
	},
	{
		"Yakutsk Standard Time", 600, true, "Yakutsk Standard Time",
		"Yakutsk Standard Time", "Yakutsk Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_91, 1
	},
	{
		"Central Pacific Standard Time", 660, false, "Central Pacific Standard Time",
		"Central Pacific Standard Time", "Central Pacific Standard Time",
		NULL, 0
	},
	{
		"Vladivostok Standard Time", 660, true, "Vladivostok Standard Time",
		"Vladivostok Standard Time", "Vladivostok Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_93, 1
	},
	{
		"New Zealand Standard Time", 720, true, "New Zealand Standard Time",
		"New Zealand Standard Time", "New Zealand Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_94, 3
	},
	{
		"UTC+12", 720, false, "UTC+12",
		"UTC+12", "UTC+12",
		NULL, 0
	},
	{
		"Fiji Standard Time", 720, true, "Fiji Standard Time",
		"Fiji Standard Time", "Fiji Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_96, 5
	},
	{
		"Magadan Standard Time", 720, true, "Magadan Standard Time",
		"Magadan Standard Time", "Magadan Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_97, 1
	},
	{
		"Kamchatka Standard Time", 720, true, "Kamchatka Standard Time",
		"Kamchatka Standard Time", "Kamchatka Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_98, 1
	},
	{
		"Tonga Standard Time", 780, false, "Tonga Standard Time",
		"Tonga Standard Time", "Tonga Standard Time",
		NULL, 0
	},
	{
		"Samoa Standard Time", 780, true, "Samoa Standard Time",
		"Samoa Standard Time", "Samoa Standard Time",
		(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_100, 2
	}
};

struct _WINDOWS_TZID_ENTRY
{
	const char* windows;
	const char* tzid;
};
typedef struct _WINDOWS_TZID_ENTRY WINDOWS_TZID_ENTRY;

const WINDOWS_TZID_ENTRY WindowsTimeZoneIdTable[] =
{
	{ "Afghanistan Standard Time", "Asia/Kabul" },
	{ "Alaskan Standard Time", "America/Anchorage America/Juneau "
			"America/Nome America/Sitka America/Yakutat" },
	{ "Alaskan Standard Time", "America/Anchorage" },
	{ "Arab Standard Time", "Asia/Aden" },
	{ "Arab Standard Time", "Asia/Bahrain" },
	{ "Arab Standard Time", "Asia/Kuwait" },
	{ "Arab Standard Time", "Asia/Qatar" },
	{ "Arab Standard Time", "Asia/Riyadh" },
	{ "Arabian Standard Time", "Asia/Dubai" },
	{ "Arabian Standard Time", "Asia/Muscat" },
	{ "Arabian Standard Time", "Etc/GMT-4" },
	{ "Arabic Standard Time", "Asia/Baghdad" },
	{ "Argentina Standard Time", "America/Buenos_Aires America/Argentina/La_Rioja "
			"America/Argentina/Rio_Gallegos America/Argentina/Salta "
			"America/Argentina/San_Juan America/Argentina/San_Luis "
			"America/Argentina/Tucuman America/Argentina/Ushuaia America/Catamarca "
			"America/Cordoba America/Jujuy America/Mendoza" },
	{ "Argentina Standard Time", "America/Buenos_Aires" },
	{ "Atlantic Standard Time", "America/Halifax America/Glace_Bay "
			"America/Goose_Bay America/Moncton" },
	{ "Atlantic Standard Time", "America/Halifax" },
	{ "Atlantic Standard Time", "America/Thule" },
	{ "Atlantic Standard Time", "Atlantic/Bermuda" },
	{ "AUS Central Standard Time", "Australia/Darwin" },
	{ "AUS Eastern Standard Time", "Australia/Sydney Australia/Melbourne" },
	{ "AUS Eastern Standard Time", "Australia/Sydney" },
	{ "Azerbaijan Standard Time", "Asia/Baku" },
	{ "Azores Standard Time", "America/Scoresbysund" },
	{ "Azores Standard Time", "Atlantic/Azores" },
	{ "Bahia Standard Time", "America/Bahia" },
	{ "Bangladesh Standard Time", "Asia/Dhaka" },
	{ "Bangladesh Standard Time", "Asia/Thimphu" },
	{ "Canada Central Standard Time", "America/Regina America/Swift_Current" },
	{ "Canada Central Standard Time", "America/Regina" },
	{ "Cape Verde Standard Time", "Atlantic/Cape_Verde" },
	{ "Cape Verde Standard Time", "Etc/GMT+1" },
	{ "Caucasus Standard Time", "Asia/Yerevan" },
	{ "Cen. Australia Standard Time", "Australia/Adelaide Australia/Broken_Hill" },
	{ "Cen. Australia Standard Time", "Australia/Adelaide" },
	{ "Central America Standard Time", "America/Belize" },
	{ "Central America Standard Time", "America/Costa_Rica" },
	{ "Central America Standard Time", "America/El_Salvador" },
	{ "Central America Standard Time", "America/Guatemala" },
	{ "Central America Standard Time", "America/Managua" },
	{ "Central America Standard Time", "America/Tegucigalpa" },
	{ "Central America Standard Time", "Etc/GMT+6" },
	{ "Central America Standard Time", "Pacific/Galapagos" },
	{ "Central Asia Standard Time", "Antarctica/Vostok" },
	{ "Central Asia Standard Time", "Asia/Almaty Asia/Qyzylorda" },
	{ "Central Asia Standard Time", "Asia/Almaty" },
	{ "Central Asia Standard Time", "Asia/Bishkek" },
	{ "Central Asia Standard Time", "Etc/GMT-6" },
	{ "Central Asia Standard Time", "Indian/Chagos" },
	{ "Central Brazilian Standard Time", "America/Cuiaba America/Campo_Grande" },
	{ "Central Brazilian Standard Time", "America/Cuiaba" },
	{ "Central Europe Standard Time", "Europe/Belgrade" },
	{ "Central Europe Standard Time", "Europe/Bratislava" },
	{ "Central Europe Standard Time", "Europe/Budapest" },
	{ "Central Europe Standard Time", "Europe/Ljubljana" },
	{ "Central Europe Standard Time", "Europe/Podgorica" },
	{ "Central Europe Standard Time", "Europe/Prague" },
	{ "Central Europe Standard Time", "Europe/Tirane" },
	{ "Central European Standard Time", "Europe/Sarajevo" },
	{ "Central European Standard Time", "Europe/Skopje" },
	{ "Central European Standard Time", "Europe/Warsaw" },
	{ "Central European Standard Time", "Europe/Zagreb" },
	{ "Central Pacific Standard Time", "Antarctica/Macquarie" },
	{ "Central Pacific Standard Time", "Etc/GMT-11" },
	{ "Central Pacific Standard Time", "Pacific/Efate" },
	{ "Central Pacific Standard Time", "Pacific/Guadalcanal" },
	{ "Central Pacific Standard Time", "Pacific/Noumea" },
	{ "Central Pacific Standard Time", "Pacific/Ponape Pacific/Kosrae" },
	{ "Central Standard Time (Mexico)", "America/Mexico_City America/Bahia_Banderas "
			"America/Cancun America/Merida America/Monterrey" },
	{ "Central Standard Time (Mexico)", "America/Mexico_City" },
	{ "Central Standard Time", "America/Chicago America/Indiana/Knox "
			"America/Indiana/Tell_City America/Menominee "
			"America/North_Dakota/Beulah America/North_Dakota/Center "
			"America/North_Dakota/New_Salem" },
	{ "Central Standard Time", "America/Chicago" },
	{ "Central Standard Time", "America/Matamoros" },
	{ "Central Standard Time", "America/Winnipeg America/Rainy_River "
			"America/Rankin_Inlet America/Resolute" },
	{ "Central Standard Time", "CST6CDT" },
	{ "China Standard Time", "Asia/Hong_Kong" },
	{ "China Standard Time", "Asia/Macau" },
	{ "China Standard Time", "Asia/Shanghai Asia/Chongqing Asia/Harbin Asia/Kashgar Asia/Urumqi" },
	{ "China Standard Time", "Asia/Shanghai" },
	{ "Dateline Standard Time", "Etc/GMT+12" },
	{ "E. Africa Standard Time", "Africa/Addis_Ababa" },
	{ "E. Africa Standard Time", "Africa/Asmera" },
	{ "E. Africa Standard Time", "Africa/Dar_es_Salaam" },
	{ "E. Africa Standard Time", "Africa/Djibouti" },
	{ "E. Africa Standard Time", "Africa/Juba" },
	{ "E. Africa Standard Time", "Africa/Kampala" },
	{ "E. Africa Standard Time", "Africa/Khartoum" },
	{ "E. Africa Standard Time", "Africa/Mogadishu" },
	{ "E. Africa Standard Time", "Africa/Nairobi" },
	{ "E. Africa Standard Time", "Antarctica/Syowa" },
	{ "E. Africa Standard Time", "Etc/GMT-3" },
	{ "E. Africa Standard Time", "Indian/Antananarivo" },
	{ "E. Africa Standard Time", "Indian/Comoro" },
	{ "E. Africa Standard Time", "Indian/Mayotte" },
	{ "E. Australia Standard Time", "Australia/Brisbane Australia/Lindeman" },
	{ "E. Australia Standard Time", "Australia/Brisbane" },
	{ "E. Europe Standard Time", "Asia/Nicosia" },
	{ "E. South America Standard Time", "America/Sao_Paulo" },
	{ "Eastern Standard Time", "America/Grand_Turk" },
	{ "Eastern Standard Time", "America/Nassau" },
	{ "Eastern Standard Time", "America/New_York America/Detroit "
			"America/Indiana/Petersburg America/Indiana/Vincennes "
			"America/Indiana/Winamac America/Kentucky/Monticello America/Louisville" },
	{ "Eastern Standard Time", "America/New_York" },
	{ "Eastern Standard Time", "America/Toronto America/Iqaluit America/Montreal "
			"America/Nipigon America/Pangnirtung America/Thunder_Bay" },
	{ "Eastern Standard Time", "EST5EDT" },
	{ "Egypt Standard Time", "Africa/Cairo" },
	{ "Egypt Standard Time", "Asia/Gaza Asia/Hebron" },
	{ "Ekaterinburg Standard Time", "Asia/Yekaterinburg" },
	{ "Fiji Standard Time", "Pacific/Fiji" },
	{ "FLE Standard Time", "Europe/Helsinki" },
	{ "FLE Standard Time", "Europe/Kiev Europe/Simferopol Europe/Uzhgorod Europe/Zaporozhye" },
	{ "FLE Standard Time", "Europe/Kiev" },
	{ "FLE Standard Time", "Europe/Mariehamn" },
	{ "FLE Standard Time", "Europe/Riga" },
	{ "FLE Standard Time", "Europe/Sofia" },
	{ "FLE Standard Time", "Europe/Tallinn" },
	{ "FLE Standard Time", "Europe/Vilnius" },
	{ "Georgian Standard Time", "Asia/Tbilisi" },
	{ "GMT Standard Time", "Atlantic/Canary" },
	{ "GMT Standard Time", "Atlantic/Faeroe" },
	{ "GMT Standard Time", "Europe/Dublin" },
	{ "GMT Standard Time", "Europe/Guernsey" },
	{ "GMT Standard Time", "Europe/Isle_of_Man" },
	{ "GMT Standard Time", "Europe/Jersey" },
	{ "GMT Standard Time", "Europe/Lisbon Atlantic/Madeira" },
	{ "GMT Standard Time", "Europe/London" },
	{ "Greenland Standard Time", "America/Godthab" },
	{ "Greenwich Standard Time", "Africa/Abidjan" },
	{ "Greenwich Standard Time", "Africa/Accra" },
	{ "Greenwich Standard Time", "Africa/Bamako" },
	{ "Greenwich Standard Time", "Africa/Banjul" },
	{ "Greenwich Standard Time", "Africa/Bissau" },
	{ "Greenwich Standard Time", "Africa/Conakry" },
	{ "Greenwich Standard Time", "Africa/Dakar" },
	{ "Greenwich Standard Time", "Africa/El_Aaiun" },
	{ "Greenwich Standard Time", "Africa/Freetown" },
	{ "Greenwich Standard Time", "Africa/Lome" },
	{ "Greenwich Standard Time", "Africa/Monrovia" },
	{ "Greenwich Standard Time", "Africa/Nouakchott" },
	{ "Greenwich Standard Time", "Africa/Ouagadougou" },
	{ "Greenwich Standard Time", "Africa/Sao_Tome" },
	{ "Greenwich Standard Time", "Atlantic/Reykjavik" },
	{ "Greenwich Standard Time", "Atlantic/St_Helena" },
	{ "GTB Standard Time", "Europe/Athens" },
	{ "GTB Standard Time", "Europe/Bucharest" },
	{ "GTB Standard Time", "Europe/Chisinau" },
	{ "Hawaiian Standard Time", "Etc/GMT+10" },
	{ "Hawaiian Standard Time", "Pacific/Fakaofo" },
	{ "Hawaiian Standard Time", "Pacific/Honolulu" },
	{ "Hawaiian Standard Time", "Pacific/Johnston" },
	{ "Hawaiian Standard Time", "Pacific/Rarotonga" },
	{ "Hawaiian Standard Time", "Pacific/Tahiti" },
	{ "India Standard Time", "Asia/Calcutta" },
	{ "Iran Standard Time", "Asia/Tehran" },
	{ "Israel Standard Time", "Asia/Jerusalem" },
	{ "Jordan Standard Time", "Asia/Amman" },
	{ "Kaliningrad Standard Time", "Europe/Kaliningrad" },
	{ "Kaliningrad Standard Time", "Europe/Minsk" },
	{ "Korea Standard Time", "Asia/Pyongyang" },
	{ "Korea Standard Time", "Asia/Seoul" },
	{ "Magadan Standard Time", "Asia/Magadan Asia/Anadyr Asia/Kamchatka" },
	{ "Magadan Standard Time", "Asia/Magadan" },
	{ "Mauritius Standard Time", "Indian/Mahe" },
	{ "Mauritius Standard Time", "Indian/Mauritius" },
	{ "Mauritius Standard Time", "Indian/Reunion" },
	{ "Middle East Standard Time", "Asia/Beirut" },
	{ "Montevideo Standard Time", "America/Montevideo" },
	{ "Morocco Standard Time", "Africa/Casablanca" },
	{ "Mountain Standard Time (Mexico)", "America/Chihuahua America/Mazatlan" },
	{ "Mountain Standard Time (Mexico)", "America/Chihuahua" },
	{ "Mountain Standard Time", "America/Denver America/Boise America/Shiprock" },
	{ "Mountain Standard Time", "America/Denver" },
	{ "Mountain Standard Time", "America/Edmonton "
			"America/Cambridge_Bay America/Inuvik America/Yellowknife" },
	{ "Mountain Standard Time", "America/Ojinaga" },
	{ "Mountain Standard Time", "MST7MDT" },
	{ "Myanmar Standard Time", "Asia/Rangoon" },
	{ "Myanmar Standard Time", "Indian/Cocos" },
	{ "N. Central Asia Standard Time", "Asia/Novosibirsk Asia/Novokuznetsk Asia/Omsk" },
	{ "N. Central Asia Standard Time", "Asia/Novosibirsk" },
	{ "Namibia Standard Time", "Africa/Windhoek" },
	{ "Nepal Standard Time", "Asia/Katmandu" },
	{ "New Zealand Standard Time", "Antarctica/South_Pole Antarctica/McMurdo" },
	{ "New Zealand Standard Time", "Pacific/Auckland" },
	{ "Newfoundland Standard Time", "America/St_Johns" },
	{ "North Asia East Standard Time", "Asia/Irkutsk" },
	{ "North Asia Standard Time", "Asia/Krasnoyarsk" },
	{ "Pacific SA Standard Time", "America/Santiago" },
	{ "Pacific SA Standard Time", "Antarctica/Palmer" },
	{ "Pacific Standard Time (Mexico)", "America/Santa_Isabel" },
	{ "Pacific Standard Time", "America/Los_Angeles" },
	{ "Pacific Standard Time", "America/Tijuana" },
	{ "Pacific Standard Time", "America/Vancouver America/Dawson America/Whitehorse" },
	{ "Pacific Standard Time", "PST8PDT" },
	{ "Pakistan Standard Time", "Asia/Karachi" },
	{ "Paraguay Standard Time", "America/Asuncion" },
	{ "Romance Standard Time", "Europe/Brussels" },
	{ "Romance Standard Time", "Europe/Copenhagen" },
	{ "Romance Standard Time", "Europe/Madrid Africa/Ceuta" },
	{ "Romance Standard Time", "Europe/Paris" },
	{ "Russian Standard Time", "Europe/Moscow Europe/Samara Europe/Volgograd" },
	{ "Russian Standard Time", "Europe/Moscow" },
	{ "SA Eastern Standard Time", "America/Cayenne" },
	{ "SA Eastern Standard Time", "America/Fortaleza America/Araguaina "
			"America/Belem America/Maceio America/Recife America/Santarem" },
	{ "SA Eastern Standard Time", "America/Paramaribo" },
	{ "SA Eastern Standard Time", "Antarctica/Rothera" },
	{ "SA Eastern Standard Time", "Etc/GMT+3" },
	{ "SA Pacific Standard Time", "America/Bogota" },
	{ "SA Pacific Standard Time", "America/Cayman" },
	{ "SA Pacific Standard Time", "America/Coral_Harbour" },
	{ "SA Pacific Standard Time", "America/Guayaquil" },
	{ "SA Pacific Standard Time", "America/Jamaica" },
	{ "SA Pacific Standard Time", "America/Lima" },
	{ "SA Pacific Standard Time", "America/Panama" },
	{ "SA Pacific Standard Time", "America/Port-au-Prince" },
	{ "SA Pacific Standard Time", "Etc/GMT+5" },
	{ "SA Western Standard Time", "America/Anguilla" },
	{ "SA Western Standard Time", "America/Antigua" },
	{ "SA Western Standard Time", "America/Aruba" },
	{ "SA Western Standard Time", "America/Barbados" },
	{ "SA Western Standard Time", "America/Blanc-Sablon" },
	{ "SA Western Standard Time", "America/Curacao" },
	{ "SA Western Standard Time", "America/Dominica" },
	{ "SA Western Standard Time", "America/Grenada" },
	{ "SA Western Standard Time", "America/Guadeloupe" },
	{ "SA Western Standard Time", "America/Guyana" },
	{ "SA Western Standard Time", "America/La_Paz" },
	{ "SA Western Standard Time", "America/Manaus America/Boa_Vista "
			"America/Eirunepe America/Porto_Velho America/Rio_Branco" },
	{ "SA Western Standard Time", "America/Marigot" },
	{ "SA Western Standard Time", "America/Martinique" },
	{ "SA Western Standard Time", "America/Montserrat" },
	{ "SA Western Standard Time", "America/Port_of_Spain" },
	{ "SA Western Standard Time", "America/Puerto_Rico" },
	{ "SA Western Standard Time", "America/Santo_Domingo" },
	{ "SA Western Standard Time", "America/St_Barthelemy" },
	{ "SA Western Standard Time", "America/St_Kitts" },
	{ "SA Western Standard Time", "America/St_Lucia" },
	{ "SA Western Standard Time", "America/St_Thomas" },
	{ "SA Western Standard Time", "America/St_Vincent" },
	{ "SA Western Standard Time", "America/Tortola" },
	{ "SA Western Standard Time", "Etc/GMT+4" },
	{ "Samoa Standard Time", "Pacific/Apia" },
	{ "SE Asia Standard Time", "Antarctica/Davis" },
	{ "SE Asia Standard Time", "Asia/Bangkok" },
	{ "SE Asia Standard Time", "Asia/Hovd" },
	{ "SE Asia Standard Time", "Asia/Jakarta Asia/Pontianak" },
	{ "SE Asia Standard Time", "Asia/Phnom_Penh" },
	{ "SE Asia Standard Time", "Asia/Saigon" },
	{ "SE Asia Standard Time", "Asia/Vientiane" },
	{ "SE Asia Standard Time", "Etc/GMT-7" },
	{ "SE Asia Standard Time", "Indian/Christmas" },
	{ "Singapore Standard Time", "Asia/Brunei" },
	{ "Singapore Standard Time", "Asia/Kuala_Lumpur Asia/Kuching" },
	{ "Singapore Standard Time", "Asia/Makassar" },
	{ "Singapore Standard Time", "Asia/Manila" },
	{ "Singapore Standard Time", "Asia/Singapore" },
	{ "Singapore Standard Time", "Etc/GMT-8" },
	{ "South Africa Standard Time", "Africa/Blantyre" },
	{ "South Africa Standard Time", "Africa/Bujumbura" },
	{ "South Africa Standard Time", "Africa/Gaborone" },
	{ "South Africa Standard Time", "Africa/Harare" },
	{ "South Africa Standard Time", "Africa/Johannesburg" },
	{ "South Africa Standard Time", "Africa/Kigali" },
	{ "South Africa Standard Time", "Africa/Lubumbashi" },
	{ "South Africa Standard Time", "Africa/Lusaka" },
	{ "South Africa Standard Time", "Africa/Maputo" },
	{ "South Africa Standard Time", "Africa/Maseru" },
	{ "South Africa Standard Time", "Africa/Mbabane" },
	{ "South Africa Standard Time", "Africa/Tripoli" },
	{ "South Africa Standard Time", "Etc/GMT-2" },
	{ "Sri Lanka Standard Time", "Asia/Colombo" },
	{ "Syria Standard Time", "Asia/Damascus" },
	{ "Taipei Standard Time", "Asia/Taipei" },
	{ "Tasmania Standard Time", "Australia/Hobart Australia/Currie" },
	{ "Tasmania Standard Time", "Australia/Hobart" },
	{ "Tokyo Standard Time", "Asia/Dili" },
	{ "Tokyo Standard Time", "Asia/Jayapura" },
	{ "Tokyo Standard Time", "Asia/Tokyo" },
	{ "Tokyo Standard Time", "Etc/GMT-9" },
	{ "Tokyo Standard Time", "Pacific/Palau" },
	{ "Tonga Standard Time", "Etc/GMT-13" },
	{ "Tonga Standard Time", "Pacific/Enderbury" },
	{ "Tonga Standard Time", "Pacific/Tongatapu" },
	{ "Turkey Standard Time", "Europe/Istanbul" },
	{ "Ulaanbaatar Standard Time", "Asia/Ulaanbaatar Asia/Choibalsan" },
	{ "Ulaanbaatar Standard Time", "Asia/Ulaanbaatar" },
	{ "US Eastern Standard Time", "America/Indianapolis "
			"America/Indiana/Marengo America/Indiana/Vevay" },
	{ "US Eastern Standard Time", "America/Indianapolis" },
	{ "US Mountain Standard Time", "America/Dawson_Creek" },
	{ "US Mountain Standard Time", "America/Hermosillo" },
	{ "US Mountain Standard Time", "America/Phoenix" },
	{ "US Mountain Standard Time", "Etc/GMT+7" },
	{ "UTC", "America/Danmarkshavn" },
	{ "UTC", "Etc/GMT" },
	{ "UTC+12", "Etc/GMT-12" },
	{ "UTC+12", "Pacific/Funafuti" },
	{ "UTC+12", "Pacific/Majuro Pacific/Kwajalein" },
	{ "UTC+12", "Pacific/Nauru" },
	{ "UTC+12", "Pacific/Tarawa" },
	{ "UTC+12", "Pacific/Wake" },
	{ "UTC+12", "Pacific/Wallis" },
	{ "UTC-02", "America/Noronha" },
	{ "UTC-02", "Atlantic/South_Georgia" },
	{ "UTC-02", "Etc/GMT+2" },
	{ "UTC-11", "Etc/GMT+11" },
	{ "UTC-11", "Pacific/Midway" },
	{ "UTC-11", "Pacific/Niue" },
	{ "UTC-11", "Pacific/Pago_Pago" },
	{ "Venezuela Standard Time", "America/Caracas" },
	{ "Vladivostok Standard Time", "Asia/Vladivostok Asia/Sakhalin" },
	{ "Vladivostok Standard Time", "Asia/Vladivostok" },
	{ "W. Australia Standard Time", "Antarctica/Casey" },
	{ "W. Australia Standard Time", "Australia/Perth" },
	{ "W. Central Africa Standard Time", "Africa/Algiers" },
	{ "W. Central Africa Standard Time", "Africa/Bangui" },
	{ "W. Central Africa Standard Time", "Africa/Brazzaville" },
	{ "W. Central Africa Standard Time", "Africa/Douala" },
	{ "W. Central Africa Standard Time", "Africa/Kinshasa" },
	{ "W. Central Africa Standard Time", "Africa/Lagos" },
	{ "W. Central Africa Standard Time", "Africa/Libreville" },
	{ "W. Central Africa Standard Time", "Africa/Luanda" },
	{ "W. Central Africa Standard Time", "Africa/Malabo" },
	{ "W. Central Africa Standard Time", "Africa/Ndjamena" },
	{ "W. Central Africa Standard Time", "Africa/Niamey" },
	{ "W. Central Africa Standard Time", "Africa/Porto-Novo" },
	{ "W. Central Africa Standard Time", "Africa/Tunis" },
	{ "W. Central Africa Standard Time", "Etc/GMT-1" },
	{ "W. Europe Standard Time", "Arctic/Longyearbyen" },
	{ "W. Europe Standard Time", "Europe/Amsterdam" },
	{ "W. Europe Standard Time", "Europe/Andorra" },
	{ "W. Europe Standard Time", "Europe/Berlin" },
	{ "W. Europe Standard Time", "Europe/Gibraltar" },
	{ "W. Europe Standard Time", "Europe/Luxembourg" },
	{ "W. Europe Standard Time", "Europe/Malta" },
	{ "W. Europe Standard Time", "Europe/Monaco" },
	{ "W. Europe Standard Time", "Europe/Oslo" },
	{ "W. Europe Standard Time", "Europe/Rome" },
	{ "W. Europe Standard Time", "Europe/San_Marino" },
	{ "W. Europe Standard Time", "Europe/Stockholm" },
	{ "W. Europe Standard Time", "Europe/Vaduz" },
	{ "W. Europe Standard Time", "Europe/Vatican" },
	{ "W. Europe Standard Time", "Europe/Vienna" },
	{ "W. Europe Standard Time", "Europe/Zurich" },
	{ "West Asia Standard Time", "Antarctica/Mawson" },
	{ "West Asia Standard Time", "Asia/Ashgabat" },
	{ "West Asia Standard Time", "Asia/Dushanbe" },
	{ "West Asia Standard Time", "Asia/Oral Asia/Aqtau Asia/Aqtobe" },
	{ "West Asia Standard Time", "Asia/Tashkent Asia/Samarkand" },
	{ "West Asia Standard Time", "Asia/Tashkent" },
	{ "West Asia Standard Time", "Etc/GMT-5" },
	{ "West Asia Standard Time", "Indian/Kerguelen" },
	{ "West Asia Standard Time", "Indian/Maldives" },
	{ "West Pacific Standard Time", "Antarctica/DumontDUrville" },
	{ "West Pacific Standard Time", "Etc/GMT-10" },
	{ "West Pacific Standard Time", "Pacific/Guam" },
	{ "West Pacific Standard Time", "Pacific/Port_Moresby" },
	{ "West Pacific Standard Time", "Pacific/Saipan" },
	{ "West Pacific Standard Time", "Pacific/Truk" },
	{ "Yakutsk Standard Time", "Asia/Yakutsk" }
};

char* freerdp_get_unix_timezone_identifier()
{
	FILE* fp;
	char* tz_env;
	size_t length;
	char* tzid = NULL;

	tz_env = getenv("TZ");

	if (tz_env != NULL)
	{
		tzid = xstrdup(tz_env);
		return tzid;
	}

	fp = fopen("/etc/timezone", "r");

	if (fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		if (length < 2)
		{
			fclose(fp) ;
			return NULL;
		}

		tzid = (char*) xmalloc(length + 1);
		fread(tzid, length, 1, fp);
		tzid[length] = '\0';

		if (tzid[length - 1] == '\n')
			tzid[length - 1] = '\0';

		fclose(fp) ;
	}

	return tzid;
}

boolean freerdp_match_unix_timezone_identifier_with_list(const char* tzid, const char* list)
{
	char* p;
	char* list_copy;

	list_copy = xstrdup(list);

	p = strtok(list_copy, " ");

	while (p != NULL)
	{
		if (strcmp(p, tzid) == 0)
		{
			xfree(list_copy);
			return true;
		}

		p = strtok(NULL, " ");
	}

	xfree(list_copy);

	return false;
}

TIME_ZONE_ENTRY* freerdp_detect_windows_time_zone(uint32 bias)
{
	int i, j;
	char* tzid;
	TIME_ZONE_ENTRY* timezone;

	tzid = freerdp_get_unix_timezone_identifier();

	if (tzid == NULL)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(TimeZoneTable); i++)
	{
		if (bias == TimeZoneTable[i].Bias)
		{
			for (j = 0; j < ARRAY_SIZE(WindowsTimeZoneIdTable); j++)
			{
				if (strcmp(TimeZoneTable[i].Id, WindowsTimeZoneIdTable[j].windows) != 0)
					continue;

				if (freerdp_match_unix_timezone_identifier_with_list(tzid, WindowsTimeZoneIdTable[j].tzid))
				{
					timezone = (TIME_ZONE_ENTRY*) xmalloc(sizeof(TIME_ZONE_ENTRY));
					memcpy((void*) timezone, (void*) &TimeZoneTable[i], sizeof(TIME_ZONE_ENTRY));
					xfree(tzid);
					return timezone;
				}
			}
		}
	}

	xfree(tzid);
	return NULL;
}

TIME_ZONE_RULE_ENTRY* freerdp_get_current_time_zone_rule(TIME_ZONE_RULE_ENTRY* rules, uint32 count)
{
	int i;
	uint64 windows_time;

	windows_time = freerdp_windows_gmtime();

	for (i = 0; i < (int) count; i++)
	{
		if ((rules[i].TicksStart <= windows_time) && (windows_time >= rules[i].TicksEnd))
		{
			return &rules[i];
		}
	}

	return NULL;
}

void freerdp_time_zone_detect(TIME_ZONE_INFO* clientTimeZone)
{
	time_t t;
	struct tm* local_time;
	TIME_ZONE_ENTRY* tz;

	time(&t);
	local_time = localtime(&t);

#ifdef HAVE_TM_GMTOFF
	if (local_time->tm_gmtoff >= 0)
		clientTimeZone->bias = (uint32) (-1 * local_time->tm_gmtoff / 60);
	else
		clientTimeZone->bias = (uint32) ((-1 * local_time->tm_gmtoff) / 60);
#elif sun
	if (local_time->tm_isdst > 0)
		clientTimeZone->bias = (uint32) (altzone / 3600);
	else
		clientTimeZone->bias = (uint32) (timezone / 3600);
#else
	clientTimeZone->bias = 0;
#endif

	if (local_time->tm_isdst > 0)
	{
		clientTimeZone->standardBias = clientTimeZone->bias - 60;
		clientTimeZone->daylightBias = clientTimeZone->bias;
	}
	else
	{
		clientTimeZone->standardBias = clientTimeZone->bias;
		clientTimeZone->daylightBias = clientTimeZone->bias + 60;
	}

	tz = freerdp_detect_windows_time_zone(clientTimeZone->bias);

	if (tz!= NULL)
	{
		clientTimeZone->bias = tz->Bias;
		sprintf(clientTimeZone->standardName, "%s", tz->StandardName);
		sprintf(clientTimeZone->daylightName, "%s", tz->DaylightName);

		if ((tz->SupportsDST) && (tz->RuleTableCount > 0))
		{
			TIME_ZONE_RULE_ENTRY* rule;
			rule = freerdp_get_current_time_zone_rule(tz->RuleTable, tz->RuleTableCount);

			if (rule != NULL)
			{
				clientTimeZone->standardBias = 0;
				clientTimeZone->daylightBias = rule->DaylightDelta;

				clientTimeZone->standardDate.wYear = rule->StandardDate.wYear;
				clientTimeZone->standardDate.wMonth = rule->StandardDate.wMonth;
				clientTimeZone->standardDate.wDayOfWeek = rule->StandardDate.wDayOfWeek;
				clientTimeZone->standardDate.wDay = rule->StandardDate.wDay;
				clientTimeZone->standardDate.wHour = rule->StandardDate.wHour;
				clientTimeZone->standardDate.wMinute = rule->StandardDate.wMinute;
				clientTimeZone->standardDate.wSecond = rule->StandardDate.wSecond;
				clientTimeZone->standardDate.wMilliseconds = rule->StandardDate.wMilliseconds;

				clientTimeZone->daylightDate.wYear = rule->DaylightDate.wYear;
				clientTimeZone->daylightDate.wMonth = rule->DaylightDate.wMonth;
				clientTimeZone->daylightDate.wDayOfWeek = rule->DaylightDate.wDayOfWeek;
				clientTimeZone->daylightDate.wDay = rule->DaylightDate.wDay;
				clientTimeZone->daylightDate.wHour = rule->DaylightDate.wHour;
				clientTimeZone->daylightDate.wMinute = rule->DaylightDate.wMinute;
				clientTimeZone->daylightDate.wSecond = rule->DaylightDate.wSecond;
				clientTimeZone->daylightDate.wMilliseconds = rule->DaylightDate.wMilliseconds;
			}
		}

		xfree(tz);
	}
	else
	{
		/* could not detect timezone, fallback to using GMT */
		sprintf(clientTimeZone->standardName, "%s", "GMT Standard Time");
		sprintf(clientTimeZone->daylightName, "%s", "GMT Daylight Time");
	}
}
