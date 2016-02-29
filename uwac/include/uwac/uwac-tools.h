/*
 * Copyright © 2015 David FORT <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef __UWAC_TOOLS_H_
#define __UWAC_TOOLS_H_

#include <stdbool.h>
#include <uwac/uwac.h>

/** @brief */
struct uwac_touch_point {
	uint32_t id;
	wl_fixed_t x, y;
};
typedef struct uwac_touch_point UwacTouchPoint;

struct uwac_touch_automata;
typedef struct uwac_touch_automata UwacTouchAutomata;

/**
 *
 * @param automata
 */
UWAC_API void UwacTouchAutomataInit(UwacTouchAutomata *automata);

/**
 *
 * @param automata
 */
UWAC_API void UwacTouchAutomataReset(UwacTouchAutomata *automata);


/**
 *
 * @param automata
 * @param event
 * @return
 */
UWAC_API bool UwacTouchAutomataInjectEvent(UwacTouchAutomata *automata, UwacEvent *event);

#endif /* __UWAC_TOOLS_H_ */
