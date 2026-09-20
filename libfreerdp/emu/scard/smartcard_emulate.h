#pragma once

#include <freerdp/emulate/scard/smartcard_emulate.h>

WINPR_ATTR_NODISCARD
BOOL Emulate_SetupPin(SmartcardEmulationContext* context, const char* name, const char* pin);

WINPR_ATTR_NODISCARD
BOOL Emulate_IsPinValid(SmartcardEmulationContext* context, const char* name, const char* pin,
                        size_t bytelen, UINT16* remaining);

WINPR_ATTR_NODISCARD
BOOL Emulate_IsPinBlocked(SmartcardEmulationContext* context, const char* name);
