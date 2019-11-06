/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MacFreeRDP
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#import "Keyboard.h"

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDManager.h>

struct _APPLE_KEYBOARD_DESC
{
	uint32_t ProductId;
	enum APPLE_KEYBOARD_TYPE Type;
};
typedef struct _APPLE_KEYBOARD_DESC APPLE_KEYBOARD_DESC;

/* VendorID: 0x05AC (Apple, Inc.) */

static const APPLE_KEYBOARD_DESC APPLE_KEYBOARDS[] = {
	{ 0x200, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x201, APPLE_KEYBOARD_TYPE_ANSI }, /* USB Keyboard [Alps or Logitech, M2452] */
	{ 0x202, APPLE_KEYBOARD_TYPE_ANSI }, /* Keyboard [ALPS] */
	{ 0x203, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x204, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x205, APPLE_KEYBOARD_TYPE_ANSI }, /* Extended Keyboard [Mitsumi] */
	{ 0x206, APPLE_KEYBOARD_TYPE_ANSI }, /* Extended Keyboard [Mitsumi] */
	{ 0x207, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x208, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x209, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x20A, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x20B, APPLE_KEYBOARD_TYPE_ANSI }, /* Pro Keyboard [Mitsumi, A1048/US layout] */
	{ 0x20C, APPLE_KEYBOARD_TYPE_ANSI }, /* Extended Keyboard [Mitsumi] */
	{ 0x20D, APPLE_KEYBOARD_TYPE_ANSI }, /* Pro Keyboard [Mitsumi, A1048/JIS layout] */
	{ 0x20E, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x20F, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x210, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x211, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x212, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x213, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x214, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x215, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x216, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x217, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x218, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x219, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x21A, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x21B, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x21C, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x21D, APPLE_KEYBOARD_TYPE_ANSI }, /* Aluminum Mini Keyboard (ANSI) */
	{ 0x21E, APPLE_KEYBOARD_TYPE_ISO },  /* Aluminum Mini Keyboard (ISO) */
	{ 0x21F, APPLE_KEYBOARD_TYPE_JIS },  /* Aluminum Mini Keyboard (JIS) */
	{ 0x220, APPLE_KEYBOARD_TYPE_ANSI }, /* Aluminum Keyboard (ANSI) */
	{ 0x221, APPLE_KEYBOARD_TYPE_JIS },  /* Aluminum Keyboard (JIS) */
	{ 0x222, APPLE_KEYBOARD_TYPE_JIS },  /* Aluminum Keyboard (JIS) */
	{ 0x223, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x224, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x225, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x226, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x227, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x228, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x229, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (MacBook Pro) (ANSI) */
	{ 0x22A, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (MacBook Pro) (ISO) */
	{ 0x22B, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (MacBook Pro) (JIS) */
	{ 0x22C, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x22D, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x22E, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x22F, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x230, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (MacBook Pro 4,1) (ANSI) */
	{ 0x231, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (MacBook Pro 4,1) (ISO) */
	{ 0x232, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (MacBook Pro 4,1) (JIS) */
	{ 0x233, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x234, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x235, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x236, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x237, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x238, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x239, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x23A, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x23B, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x23C, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x23D, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x23E, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x23F, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x240, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x241, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x242, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x243, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x244, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x245, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x246, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x247, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x248, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x249, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x24A, APPLE_KEYBOARD_TYPE_ISO }, /* Internal Keyboard/Trackpad (MacBook Air) (ISO) */
	{ 0x24B, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x24C, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x24D, APPLE_KEYBOARD_TYPE_ISO }, /* Internal Keyboard/Trackpad (MacBook Air) (ISO) */
	{ 0x24E, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x24F, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x250, APPLE_KEYBOARD_TYPE_ISO }, /* Aluminium Keyboard (ISO) */
	{ 0x251, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x252, APPLE_KEYBOARD_TYPE_ANSI }, /* Internal Keyboard/Trackpad (ANSI) */
	{ 0x253, APPLE_KEYBOARD_TYPE_ISO },  /* Internal Keyboard/Trackpad (ISO) */
	{ 0x254, APPLE_KEYBOARD_TYPE_JIS },  /* Internal Keyboard/Trackpad (JIS) */
	{ 0x255, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x256, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x257, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x258, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x259, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x25A, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x25B, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x25C, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x25D, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x25E, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x25F, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x260, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x261, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x262, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x263, APPLE_KEYBOARD_TYPE_ANSI }, /* Apple Internal Keyboard / Trackpad (MacBook Retina) */
	{ 0x264, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x265, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x266, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x267, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x268, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x269, APPLE_KEYBOARD_TYPE_ANSI },
	{ 0x26A, APPLE_KEYBOARD_TYPE_ANSI }
};

static enum APPLE_KEYBOARD_TYPE mac_identify_keyboard_type(uint32_t vendorID, uint32_t productID)
{
	enum APPLE_KEYBOARD_TYPE type = APPLE_KEYBOARD_TYPE_ANSI;

	if (vendorID != 0x05AC) /* Apple, Inc. */
		return type;

	if ((productID < 0x200) || (productID > 0x26A))
		return type;

	type = APPLE_KEYBOARDS[productID - 0x200].Type;
	return type;
}

enum APPLE_KEYBOARD_TYPE mac_detect_keyboard_type(void)
{
	CFSetRef deviceCFSetRef = NULL;
	IOHIDDeviceRef inIOHIDDeviceRef = NULL;
	IOHIDManagerRef tIOHIDManagerRef = NULL;
	IOHIDDeviceRef *tIOHIDDeviceRefs = nil;
	enum APPLE_KEYBOARD_TYPE type = APPLE_KEYBOARD_TYPE_ANSI;
	tIOHIDManagerRef = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

	if (!tIOHIDManagerRef)
		return type;

	IOHIDManagerSetDeviceMatching(tIOHIDManagerRef, NULL);
	IOReturn tIOReturn = IOHIDManagerOpen(tIOHIDManagerRef, kIOHIDOptionsTypeNone);

	if (noErr != tIOReturn)
		return type;

	deviceCFSetRef = IOHIDManagerCopyDevices(tIOHIDManagerRef);

	if (!deviceCFSetRef)
		return type;

	CFIndex deviceIndex, deviceCount = CFSetGetCount(deviceCFSetRef);
	tIOHIDDeviceRefs = malloc(sizeof(IOHIDDeviceRef) * deviceCount);

	if (!tIOHIDDeviceRefs)
		return type;

	CFSetGetValues(deviceCFSetRef, (const void **)tIOHIDDeviceRefs);
	CFRelease(deviceCFSetRef);
	deviceCFSetRef = NULL;

	for (deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
	{
		CFTypeRef tCFTypeRef;
		uint32_t vendorID = 0;
		uint32_t productID = 0;
		uint32_t countryCode = 0;
		enum APPLE_KEYBOARD_TYPE ltype;

		if (!tIOHIDDeviceRefs[deviceIndex])
			continue;

		inIOHIDDeviceRef = tIOHIDDeviceRefs[deviceIndex];
		tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDVendorIDKey));

		if (tCFTypeRef)
			CFNumberGetValue((CFNumberRef)tCFTypeRef, kCFNumberSInt32Type, &vendorID);

		tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDProductIDKey));

		if (tCFTypeRef)
			CFNumberGetValue((CFNumberRef)tCFTypeRef, kCFNumberSInt32Type, &productID);

		tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDCountryCodeKey));

		if (tCFTypeRef)
			CFNumberGetValue((CFNumberRef)tCFTypeRef, kCFNumberSInt32Type, &countryCode);

		ltype = mac_identify_keyboard_type(vendorID, productID);

		if (ltype != APPLE_KEYBOARD_TYPE_ANSI)
		{
			type = ltype;
			break;
		}
	}

	free(tIOHIDDeviceRefs);

	if (deviceCFSetRef)
	{
		CFRelease(deviceCFSetRef);
		deviceCFSetRef = NULL;
	}

	if (tIOHIDManagerRef)
		CFRelease(tIOHIDManagerRef);

	return type;
}
