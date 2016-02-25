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

#include <wayland-util.h>
#include <string.h>
#include <uwac/uwac-tools.h>

/** @brief */
struct uwac_touch_automata {
	struct wl_array tp;
};

void UwacTouchAutomataInit(UwacTouchAutomata *automata) {
	wl_array_init(&automata->tp);
}

void UwacTouchAutomataReset(UwacTouchAutomata *automata) {
	automata->tp.size = 0;
}

bool UwacTouchAutomataInjectEvent(UwacTouchAutomata *automata, UwacEvent *event) {

	UwacTouchPoint *tp;

	switch (event->type) {
	case UWAC_EVENT_TOUCH_FRAME_BEGIN:
		break;

	case UWAC_EVENT_TOUCH_UP: {
		UwacTouchUp *touchUp = &event->touchUp;
		int toMove = automata->tp.size - sizeof(UwacTouchPoint);

		wl_array_for_each(tp, &automata->tp) {
			if (tp->id == touchUp->id) {
				if (toMove)
					memmove(tp, tp+1, toMove);
				return true;
			}

			toMove -= sizeof(UwacTouchPoint);
		}
		break;
	}

	case UWAC_EVENT_TOUCH_DOWN: {
		UwacTouchDown *touchDown = &event->touchDown;

		wl_array_for_each(tp, &automata->tp) {
			if (tp->id == touchDown->id) {
				tp->x = touchDown->x;
				tp->y = touchDown->y;
				return true;
			}
		}

		tp = wl_array_add(&automata->tp, sizeof(UwacTouchPoint));
		if (!tp)
			return false;

		tp->id = touchDown->id;
		tp->x = touchDown->x;
		tp->y = touchDown->y;
		break;
	}

	case UWAC_EVENT_TOUCH_FRAME_END:
		break;

	default:
		break;
	}

	return true;
}
