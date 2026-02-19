#pragma once

#include <winpr/wtypes.h>

extern BOOL fsig_handlers_registered;

void fsig_lock(void);
void fsig_unlock(void);
void fsig_term_handler(int signum);
