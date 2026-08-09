/*
   Android Mouse Input Mapping

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import android.content.Context;

import com.freerdp.freerdpcore.presentation.ApplicationSettingsActivity;

public class Mouse
{

	private final static int PTRFLAGS_LBUTTON = 0x1000;
	private final static int PTRFLAGS_RBUTTON = 0x2000;
	private final static int PTRFLAGS_MBUTTON = 0x4000;

	private final static int PTRFLAGS_DOWN = 0x8000;
	private final static int PTRFLAGS_MOVE = 0x0800;

	private final static int PTRFLAGS_WHEEL = 0x0200;
	private final static int PTRFLAGS_WHEEL_NEGATIVE = 0x0100;
	private final static int PTRFLAGS_HWHEEL = 0x0400;
	public final static int WHEEL_DELTA = 0x0078; // 120 rotation units = one notch

	public static int getLeftButtonEvent(Context context, boolean down)
	{
		if (ApplicationSettingsActivity.getSwapMouseButtons(context))
			return (PTRFLAGS_RBUTTON | (down ? PTRFLAGS_DOWN : 0));
		else
			return (PTRFLAGS_LBUTTON | (down ? PTRFLAGS_DOWN : 0));
	}

	public static int getRightButtonEvent(Context context, boolean down)
	{
		if (ApplicationSettingsActivity.getSwapMouseButtons(context))
			return (PTRFLAGS_LBUTTON | (down ? PTRFLAGS_DOWN : 0));
		else
			return (PTRFLAGS_RBUTTON | (down ? PTRFLAGS_DOWN : 0));
	}

	public static int getMiddleButtonEvent(boolean down)
	{
		return (PTRFLAGS_MBUTTON | (down ? PTRFLAGS_DOWN : 0));
	}

	public static int getMoveEvent()
	{
		return PTRFLAGS_MOVE;
	}

	public static int getScrollEvent(Context context, boolean down)
	{
		return getScrollEvent(context, down ? -WHEEL_DELTA : WHEEL_DELTA);
	}

	// amount: signed wheel rotation units (positive = scroll up, negative = scroll down)
	public static int getScrollEvent(Context context, int amount)
	{
		if (ApplicationSettingsActivity.getInvertScrolling(context))
			amount = -amount;
		return wheelEvent(PTRFLAGS_WHEEL, amount);
	}

	public static int getHScrollEvent(Context context, boolean right)
	{
		return getHScrollEvent(context, right ? WHEEL_DELTA : -WHEEL_DELTA);
	}

	public static int getHScrollEvent(Context context, int amount)
	{
		if (ApplicationSettingsActivity.getInvertScrolling(context))
			amount = -amount;
		return wheelEvent(PTRFLAGS_HWHEEL, amount);
	}

	private static int wheelEvent(int wheelFlag, int amount)
	{
		int flags = wheelFlag;
		if (amount < 0)
			flags |= PTRFLAGS_WHEEL_NEGATIVE;
		return flags | (amount & 0xFF);
	}
}
