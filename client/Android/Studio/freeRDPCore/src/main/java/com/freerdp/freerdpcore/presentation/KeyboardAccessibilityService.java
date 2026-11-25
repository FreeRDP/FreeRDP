/*
   Physical Keyboard Accessibility Service

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
 */

package com.freerdp.freerdpcore.presentation;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.ViewConfiguration;
import android.view.accessibility.AccessibilityEvent;

public class KeyboardAccessibilityService extends AccessibilityService
{
	/* Android synthesizes key repeat when it dispatches a key to a window. Keys this service
	   consumes never reach one, so a held key arrives as a single down and up: repeat it here,
	   with the platform's own timings, until the matching up. */
	private final Handler repeatHandler = new Handler(Looper.getMainLooper());
	private KeyEvent repeatEvent;
	private final Runnable repeatRunnable = new Runnable() {
		@Override public void run()
		{
			SessionActivity session = SessionActivity.activeSession;
			if (repeatEvent == null || session == null)
			{
				stopRepeat();
				return;
			}
			repeatEvent = KeyEvent.changeTimeRepeat(repeatEvent, SystemClock.uptimeMillis(),
			                                        repeatEvent.getRepeatCount() + 1);
			session.handleKeyEvent(repeatEvent);
			repeatHandler.postDelayed(this, ViewConfiguration.getKeyRepeatDelay());
		}
	};

	@Override public boolean onKeyEvent(KeyEvent event)
	{
		SessionActivity session = SessionActivity.activeSession;
		if (session == null)
		{
			stopRepeat();
			return super.onKeyEvent(event);
		}

		InputDevice device = event.getDevice();
		if (device != null &&
		    (device.getSources() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD)
			return super.onKeyEvent(event);

		switch (event.getKeyCode())
		{
			case KeyEvent.KEYCODE_VOLUME_UP:
			case KeyEvent.KEYCODE_VOLUME_DOWN:
			case KeyEvent.KEYCODE_POWER:
				return super.onKeyEvent(event);
		}

		/* Some tablets map the physical ESC key to KEYCODE_BACK (scancode 1).*/
		if (event.getScanCode() == 1 && event.getKeyCode() != KeyEvent.KEYCODE_ESCAPE)
		{
			event =
			    new KeyEvent(event.getDownTime(), event.getEventTime(), event.getAction(),
			                 KeyEvent.KEYCODE_ESCAPE, event.getRepeatCount(), event.getMetaState());
		}

		boolean handled = session.handleKeyEvent(event);
		trackRepeat(event, handled);
		return handled;
	}

	private void trackRepeat(KeyEvent event, boolean handled)
	{
		switch (event.getAction())
		{
			case KeyEvent.ACTION_DOWN:
				/* A new key ends the previous one's repeat, as it does on the platform. A down
				   that already carries a repeat count is being repeated by someone else. */
				stopRepeat();
				if (handled && event.getRepeatCount() == 0 && isRepeatable(event.getKeyCode()))
				{
					repeatEvent = event;
					repeatHandler.postDelayed(repeatRunnable,
					                          ViewConfiguration.getKeyRepeatTimeout());
				}
				break;
			case KeyEvent.ACTION_UP:
				if (repeatEvent != null && repeatEvent.getKeyCode() == event.getKeyCode())
					stopRepeat();
				break;
			default:
				break;
		}
	}

	private static boolean isRepeatable(int keyCode)
	{
		switch (keyCode)
		{
			case KeyEvent.KEYCODE_CAPS_LOCK:
			case KeyEvent.KEYCODE_NUM_LOCK:
			case KeyEvent.KEYCODE_SCROLL_LOCK:
				return false;
			default:
				return !KeyEvent.isModifierKey(keyCode);
		}
	}

	private void stopRepeat()
	{
		repeatHandler.removeCallbacks(repeatRunnable);
		repeatEvent = null;
	}

	@Override public void onServiceConnected()
	{
		AccessibilityServiceInfo info = new AccessibilityServiceInfo();
		info.packageNames = new String[] { getApplicationContext().getPackageName() };
		info.eventTypes = AccessibilityEvent.TYPES_ALL_MASK;
		info.notificationTimeout = 100;
		info.flags = AccessibilityServiceInfo.FLAG_REQUEST_FILTER_KEY_EVENTS;
		info.feedbackType = AccessibilityServiceInfo.FEEDBACK_GENERIC;
		setServiceInfo(info);
	}

	@Override public void onAccessibilityEvent(AccessibilityEvent event)
	{
	}

	@Override public void onInterrupt()
	{
		stopRepeat();
	}

	@Override public void onDestroy()
	{
		stopRepeat();
		super.onDestroy();
	}
}
