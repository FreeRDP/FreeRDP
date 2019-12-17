/*
 * Automatically generated with scripts/update-windows-zones.py
 */
#ifndef WINPR_WINDOWS_ZONES_H_
#define WINPR_WINDOWS_ZONES_H_

#include <winpr/wtypes.h>

struct _WINDOWS_TZID_ENTRY
{
	const char* windows;
	const char* tzid;
};
typedef struct _WINDOWS_TZID_ENTRY WINDOWS_TZID_ENTRY;

extern const WINDOWS_TZID_ENTRY WindowsTimeZoneIdTable[];
extern const size_t WindowsTimeZoneIdTableNrElements;

#endif /* WINPR_WINDOWS_ZONES_H_ */
