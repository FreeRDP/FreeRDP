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
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;

public class KeyboardAccessibilityService extends AccessibilityService
{
	@Override public boolean onKeyEvent(KeyEvent event)
	{
		SessionActivity session = SessionActivity.activeSession;
		if (session == null)
			return super.onKeyEvent(event);

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

		return session.handleKeyEvent(event);
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
	}
}
