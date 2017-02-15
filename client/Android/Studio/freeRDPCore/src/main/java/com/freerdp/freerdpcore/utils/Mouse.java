/*
   Android Mouse Input Mapping

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import com.freerdp.freerdpcore.application.GlobalSettings;

public class Mouse {

	private final static int PTRFLAGS_LBUTTON = 0x1000;
	private final static int PTRFLAGS_RBUTTON = 0x2000;

	private final static int PTRFLAGS_DOWN = 0x8000;
	private final static int PTRFLAGS_MOVE = 0x0800;

	private final static int PTRFLAGS_WHEEL = 0x0200;
	private final static int PTRFLAGS_WHEEL_NEGATIVE = 0x0100;

	public static int getLeftButtonEvent(boolean down) {
		if(GlobalSettings.getSwapMouseButtons())
			return (PTRFLAGS_RBUTTON | (down ? PTRFLAGS_DOWN : 0));
		else
			return (PTRFLAGS_LBUTTON | (down ? PTRFLAGS_DOWN : 0));
	}
	
	public static int getRightButtonEvent(boolean down) {
		if(GlobalSettings.getSwapMouseButtons())
			return (PTRFLAGS_LBUTTON | (down ? PTRFLAGS_DOWN : 0));
		else
			return (PTRFLAGS_RBUTTON | (down ? PTRFLAGS_DOWN : 0));
	}
	
	public static int getMoveEvent() {
		return PTRFLAGS_MOVE;
	}
	
	public static int getScrollEvent(boolean down) {
		int flags = PTRFLAGS_WHEEL;
		
		// invert scrolling?
		if(GlobalSettings.getInvertScrolling())
			down = !down;
		
		if(down)
			flags |= (PTRFLAGS_WHEEL_NEGATIVE | 0x0088);
		else
			flags |= 0x0078;				
		return flags;
	}

}
