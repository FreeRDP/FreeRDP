/*
 * Automatically generated with scripts/update-windows-zones.py
 */
#ifndef WINPR_WINDOWS_ZONES_H_
#define WINPR_WINDOWS_ZONES_H_

#include <winpr/wtypes.h>

typedef struct
{
	const char* tzid;
	const char* windows;
} WINDOWS_TZID_ENTRY;

extern const WINDOWS_TZID_ENTRY WindowsZones[];
extern const size_t WindowsZonesNrElements;

#endif /* WINPR_WINDOWS_ZONES_H_ */
