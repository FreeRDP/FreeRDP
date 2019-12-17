/*
 * Automatically generated with scripts/TimeZones.csx
 */

#ifndef WINPR_TIME_ZONES_H_
#define WINPR_TIME_ZONES_H_

#include <winpr/wtypes.h>

struct _TIME_ZONE_RULE_ENTRY
{
	UINT64 TicksStart;
	UINT64 TicksEnd;
	INT32 DaylightDelta;
	SYSTEMTIME StandardDate;
	SYSTEMTIME DaylightDate;
};
typedef struct _TIME_ZONE_RULE_ENTRY TIME_ZONE_RULE_ENTRY;

struct _TIME_ZONE_ENTRY
{
	const char* Id;
	INT32 Bias;
	BOOL SupportsDST;
	const char* DisplayName;
	const char* StandardName;
	const char* DaylightName;
	const TIME_ZONE_RULE_ENTRY* RuleTable;
	UINT32 RuleTableCount;
};
typedef struct _TIME_ZONE_ENTRY TIME_ZONE_ENTRY;

extern const TIME_ZONE_ENTRY TimeZoneTable[];
extern const size_t TimeZoneTableNrElements;

#endif /* WINPR_TIME_ZONES_H_ */
