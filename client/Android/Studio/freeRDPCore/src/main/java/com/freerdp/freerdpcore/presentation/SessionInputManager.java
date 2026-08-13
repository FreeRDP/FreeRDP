/*
   Android Session Input Manager

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import com.freerdp.freerdpcore.services.LibFreeRDP;
import com.freerdp.freerdpcore.utils.KeyboardMapper;
import com.freerdp.freerdpcore.utils.Mouse;

public class SessionInputManager
    implements SessionView.SessionViewListener, TouchPointerView.TouchPointerListener,
               KeyboardMapper.KeyProcessingListener, ExtendedKeyboardView.Listener
{
	private static final String TAG = "FreeRDP.SessionInputManager";

	private static final int SCROLLING_TIMEOUT = 16;
	private static final int SCROLLING_DISTANCE = 12;
	private static final int SCROLLING_EDGE_MARGIN = 16;
	private static final int MAX_DISCARDED_MOVE_EVENTS = 3;
	private static final int SEND_MOVE_EVENT_TIMEOUT = 150;

	private static final int MSG_SEND_MOVE_EVENT = 1;
	private static final int MSG_SCROLLING_REQUESTED = 2;

	private final Context context;
	private final KeyboardMapper keyboardMapper;
	private final ScrollView2D scrollView;
	private final SessionView sessionView;
	private final TouchPointerView touchPointerView;
	private final ExtendedKeyboardView keyboard;
	private final PinchZoomListener pinchZoomListener = new PinchZoomListener();

	// Native FreeRDP instance handle. 0 until attachSession() is called (i.e. before connect).
	private long instance = 0;
	private Bitmap bitmap;
	private int screenWidth;
	private int screenHeight;
	private int discardedMoveEvents = 0;
	// we asked the IME to show; the window has not necessarily animated it in yet
	private boolean softInputRequested = false;
	// the IME reported a non-zero inset, i.e. it really is on screen
	private boolean softInputVisible = false;

	private final Handler handler;

	public SessionInputManager(Context context, ScrollView2D scrollView, SessionView sessionView,
	                           TouchPointerView touchPointerView, ExtendedKeyboardView keyboard)
	{
		this.context = context;
		this.scrollView = scrollView;
		this.sessionView = sessionView;
		this.touchPointerView = touchPointerView;
		this.keyboard = keyboard;
		this.handler = new InputHandler();

		this.keyboardMapper = new KeyboardMapper();
		this.keyboardMapper.init(context);

		keyboard.setListener(this);
	}

	// Binds this manager to a live FreeRDP session. Until called, all input events are dropped.
	public void attachSession(long instance, Bitmap surface)
	{
		this.instance = instance;
		this.bitmap = surface;
		keyboardMapper.reset(this);
	}

	// Called when the session bitmap is created or replaced (OnSettingsChanged / OnGraphicsResize).
	public void setBitmap(Bitmap bitmap)
	{
		this.bitmap = bitmap;
	}

	// Returns a listener that can be wired into a ScaleGestureDetector for pinch-to-zoom.
	public ScaleGestureDetector.OnScaleGestureListener getPinchZoomListener()
	{
		return pinchZoomListener;
	}

	// Called once the screen dimensions are known (onGlobalLayout) and on bindSession.
	public void setScreenSize(int width, int height)
	{
		this.screenWidth = width;
		this.screenHeight = height;
	}

	// Shows or hides the key bar together with the system IME.
	public void toggleKeyboard()
	{
		if (keyboard.getVisibility() == View.VISIBLE)
		{
			hideKeyboards();
		}
		else
		{
			keyboard.setExpanded(false, false);
			keyboard.setVisibility(View.VISIBLE);
			setSoftInputState(true);
		}
	}

	// Called from onPause and back-press handling.
	public void hideKeyboards()
	{
		keyboard.setExpanded(false, false);
		keyboard.setVisibility(View.GONE);
		setSoftInputState(false);
		keyboardMapper.clearlAllModifiers();
		// the IME dismiss animation may re-show the nav bar after the refresh above
		scrollView.post(this::refreshSystemBars);
	}

	// Returns true if the back press was consumed by the keyboard.
	public boolean handleKeyboardBack()
	{
		if (keyboard.getVisibility() != View.VISIBLE)
			return false;

		if (keyboard.isExpanded())
		{
			keyboard.setExpanded(false, false);
			// deliberately no IME here, so the next back press hides the bar as well
			refreshSystemBars();
			scrollView.requestApplyInsets();
			return true;
		}

		hideKeyboards();
		return true;
	}

	// True if the system soft keyboard (IME) is up or on its way up.
	public boolean isSoftInputActive()
	{
		return keyboard.getVisibility() == View.VISIBLE && (softInputRequested || softInputVisible);
	}

	// Fed from the window insets listener. The IME only counts as gone once it has been seen on
	// screen: the insets pass right after showSoftInput() still reports a zero inset.
	public void onImeVisibilityChanged(boolean visible)
	{
		if (visible == softInputVisible)
			return;
		softInputVisible = visible;
		if (!visible)
			softInputRequested = false;
		refreshSystemBars();
	}

	private void refreshSystemBars()
	{
		if (context instanceof SessionActivity)
			((SessionActivity)context).hideSystemBars();
	}

	private void setSoftInputState(boolean state)
	{
		softInputRequested = state;
		if (!state)
			softInputVisible = false;
		InputMethodManager mgr =
		    (InputMethodManager)context.getSystemService(Context.INPUT_METHOD_SERVICE);

		if (state)
		{
			sessionView.requestFocus();
			mgr.showSoftInput(sessionView, InputMethodManager.SHOW_IMPLICIT);
		}
		else
		{
			mgr.hideSoftInputFromWindow(sessionView.getWindowToken(), 0);
		}
		refreshSystemBars();
		scrollView.requestApplyInsets();
	}

	// Cancels any pending delayed-move events; called on connection failure / disconnect.
	public void cancelPendingEvents()
	{
		handler.removeMessages(MSG_SEND_MOVE_EVENT);
	}

	// Forwards a physical-mouse scroll event (e.g. external mouse wheel) into the session.
	public boolean onGenericMotionEvent(MotionEvent e)
	{
		if (instance == 0)
			return false;
		if (e.getAction() != MotionEvent.ACTION_SCROLL)
			return false;

		final float vScroll = e.getAxisValue(MotionEvent.AXIS_VSCROLL);
		if (vScroll < 0)
			LibFreeRDP.sendCursorEvent(instance, 0, 0, Mouse.getScrollEvent(context, false));
		else if (vScroll > 0)
			LibFreeRDP.sendCursorEvent(instance, 0, 0, Mouse.getScrollEvent(context, true));
		return true;
	}

	// Forwards an Android hardware-keyboard event into the session.
	public boolean onAndroidKeyEvent(KeyEvent event)
	{
		if (instance == 0)
			return false;
		return keyboardMapper.processAndroidKeyEvent(event);
	}

	// Handles a long-press on the BACK key by disconnecting the active session.
	// Returns true if the event was consumed.
	public boolean onAndroidKeyLongPress(int keyCode)
	{
		if (instance == 0)
			return false;
		if (keyCode == KeyEvent.KEYCODE_BACK)
		{
			LibFreeRDP.disconnect(instance);
			return true;
		}
		return false;
	}

	// If the "use back as Alt+F4" preference is enabled, sends Alt+F4 and returns true.
	public boolean handleBackAsAltF4()
	{
		if (instance == 0)
			return false;
		if (!ApplicationSettingsActivity.getUseBackAsAltf4(context))
			return false;
		keyboardMapper.sendAltF4();
		return true;
	}

	// Toggles touch-pointer overlay visibility (driven by the menu).
	public void toggleTouchPointer()
	{
		if (touchPointerView.getVisibility() == View.VISIBLE)
		{
			touchPointerView.setVisibility(View.INVISIBLE);
			sessionView.setTouchPointerPadding(0, 0);
		}
		else
		{
			touchPointerView.setVisibility(View.VISIBLE);
			sessionView.setTouchPointerPadding(touchPointerView.getPointerWidth(),
			                                   touchPointerView.getPointerHeight());
		}
	}

	// ****************************************************************************
	// Private helpers

	private void sendDelayedMoveEvent(int x, int y)
	{
		if (handler.hasMessages(MSG_SEND_MOVE_EVENT))
		{
			handler.removeMessages(MSG_SEND_MOVE_EVENT);
			discardedMoveEvents++;
		}
		else
			discardedMoveEvents = 0;

		if (discardedMoveEvents > MAX_DISCARDED_MOVE_EVENTS)
			LibFreeRDP.sendCursorEvent(instance, x, y, Mouse.getMoveEvent());
		else
			handler.sendMessageDelayed(Message.obtain(null, MSG_SEND_MOVE_EVENT, x, y),
			                           SEND_MOVE_EVENT_TIMEOUT);
	}

	private void cancelDelayedMoveEvent()
	{
		handler.removeMessages(MSG_SEND_MOVE_EVENT);
	}

	private Point mapScreenCoordToSessionCoord(int x, int y)
	{
		View container = scrollView.getChildCount() > 0 ? scrollView.getChildAt(0) : sessionView;
		int mappedX = (int)((float)(x - container.getLeft() + scrollView.getScrollX()) /
		                    sessionView.getZoom());
		int mappedY = (int)((float)(y - container.getTop() + scrollView.getScrollY()) /
		                    sessionView.getZoom());
		if (bitmap != null)
		{
			if (mappedX < 0)
				mappedX = 0;
			if (mappedY < 0)
				mappedY = 0;
			if (mappedX > bitmap.getWidth())
				mappedX = bitmap.getWidth();
			if (mappedY > bitmap.getHeight())
				mappedY = bitmap.getHeight();
		}
		return new Point(mappedX, mappedY);
	}

	// ****************************************************************************
	// SessionView.SessionViewListener

	@Override public void onSessionViewBeginTouch()
	{
		scrollView.setScrollEnabled(false);
	}

	@Override public void onSessionViewEndTouch()
	{
		scrollView.setScrollEnabled(true);
	}

	@Override public void onSessionViewLeftTouch(int x, int y, boolean down)
	{
		if (instance == 0)
			return;
		if (!down)
			cancelDelayedMoveEvent();
		LibFreeRDP.sendCursorEvent(instance, x, y, Mouse.getLeftButtonEvent(context, down));
	}

	@Override public void onSessionViewMiddleTouch(int x, int y, boolean down)
	{
		if (instance == 0)
			return;
		LibFreeRDP.sendCursorEvent(instance, x, y, Mouse.getMiddleButtonEvent(down));
	}

	@Override public void onSessionViewRightTouch(int x, int y, boolean down)
	{
		if (instance == 0)
			return;
		LibFreeRDP.sendCursorEvent(instance, x, y, Mouse.getRightButtonEvent(context, down));
	}

	@Override public void onSessionViewMove(int x, int y)
	{
		if (instance == 0)
			return;
		sendDelayedMoveEvent(x, y);
	}

	@Override public void onSessionViewMouseMove(int x, int y)
	{
		if (instance == 0)
			return;
		LibFreeRDP.sendCursorEvent(instance, x, y, Mouse.getMoveEvent());
	}

	@Override public void onSessionViewScroll(boolean down)
	{
		if (instance == 0)
			return;
		LibFreeRDP.sendCursorEvent(instance, 0, 0, Mouse.getScrollEvent(context, down));
	}

	@Override public void onSessionViewHScroll(boolean right)
	{
		if (instance == 0)
			return;
		LibFreeRDP.sendCursorEvent(instance, 0, 0, Mouse.getHScrollEvent(context, right));
	}

	// ****************************************************************************
	// TouchPointerView.TouchPointerListener

	@Override public void onTouchPointerClose()
	{
		touchPointerView.setVisibility(View.INVISIBLE);
		sessionView.setTouchPointerPadding(0, 0);
	}

	@Override public void onTouchPointerLeftClick(int x, int y, boolean down)
	{
		if (instance == 0)
			return;
		Point p = mapScreenCoordToSessionCoord(x, y);
		LibFreeRDP.sendCursorEvent(instance, p.x, p.y, Mouse.getLeftButtonEvent(context, down));
	}

	@Override public void onTouchPointerRightClick(int x, int y, boolean down)
	{
		if (instance == 0)
			return;
		Point p = mapScreenCoordToSessionCoord(x, y);
		LibFreeRDP.sendCursorEvent(instance, p.x, p.y, Mouse.getRightButtonEvent(context, down));
	}

	@Override public void onTouchPointerMove(int x, int y)
	{
		if (instance == 0)
			return;
		Point p = mapScreenCoordToSessionCoord(x, y);
		LibFreeRDP.sendCursorEvent(instance, p.x, p.y, Mouse.getMoveEvent());

		if (ApplicationSettingsActivity.getAutoScrollTouchPointer(context) &&
		    !handler.hasMessages(MSG_SCROLLING_REQUESTED))
		{
			handler.sendEmptyMessageDelayed(MSG_SCROLLING_REQUESTED, SCROLLING_TIMEOUT);
		}
	}

	@Override public void onTouchPointerMoveEnd()
	{
		handler.removeMessages(MSG_SCROLLING_REQUESTED);
	}

	@Override public void onTouchPointerScroll(boolean down)
	{
		if (instance == 0)
			return;
		LibFreeRDP.sendCursorEvent(instance, 0, 0, Mouse.getScrollEvent(context, down));
	}

	@Override public void onTouchPointerToggleKeyboard()
	{
		toggleKeyboard();
	}

	@Override public void onTouchPointerResetScrollZoom()
	{
		sessionView.setZoom(1.0f);
		scrollView.scrollTo(0, 0);
	}

	// ****************************************************************************
	// KeyboardMapper.KeyProcessingListener

	@Override public void processVirtualKey(int virtualKeyCode, boolean down)
	{
		if (instance == 0)
			return;
		LibFreeRDP.sendKeyEvent(instance, virtualKeyCode, down);
	}

	@Override public void processUnicodeKey(int unicodeKey)
	{
		if (instance == 0)
			return;
		if (LibFreeRDP.isUnicodeInputSupported(instance))
		{
			LibFreeRDP.sendUnicodeKeyEvent(instance, unicodeKey, true);
			LibFreeRDP.sendUnicodeKeyEvent(instance, unicodeKey, false);
		}
		else
			keyboardMapper.processUnicodeFallback(unicodeKey);
	}

	@Override public void switchKeyboard(int keyboardType)
	{
		switch (keyboardType)
		{
			case KeyboardMapper.KEYBOARD_TYPE_FUNCTIONKEYS:
				keyboard.selectPage(ExtendedKeyboardView.PAGE_SPECIAL);
				break;

			case KeyboardMapper.KEYBOARD_TYPE_NUMPAD:
				keyboard.selectPage(ExtendedKeyboardView.PAGE_NUM);
				break;

			default:
				break;
		}
	}

	@Override public void modifiersChanged()
	{
		keyboard.refreshModifiers();
	}

	// ****************************************************************************
	// ExtendedKeyboardView.Listener

	@Override public void onKey(int keycode)
	{
		keyboardMapper.processCustomKeyEvent(keycode);
	}

	@Override public void onKeyLock(int keycode)
	{
		keyboardMapper.processCustomKeyLock(keycode);
	}

	@Override public int getModifierState(int keycode)
	{
		return keyboardMapper.getModifierState(keycode);
	}

	@Override public void onExpandedChanged(boolean expanded)
	{
		// the expanded panel replaces the system IME; collapsing brings it back
		setSoftInputState(!expanded);
	}

	// ****************************************************************************
	// Internal delayed-event handler

	private class InputHandler extends Handler
	{
		InputHandler()
		{
			super(Looper.getMainLooper());
		}

		@Override public void handleMessage(Message msg)
		{
			switch (msg.what)
			{
				case MSG_SEND_MOVE_EVENT:
					if (instance == 0)
						break;
					LibFreeRDP.sendCursorEvent(instance, msg.arg1, msg.arg2, Mouse.getMoveEvent());
					break;

				case MSG_SCROLLING_REQUESTED:
				{
					int scrollX = 0;
					int scrollY = 0;
					float[] pointerPos = touchPointerView.getPointerPosition();
					final int ow = touchPointerView.getWidth();
					final int oh = touchPointerView.getHeight();
					final int pw = touchPointerView.getPointerWidth();
					final int ph = touchPointerView.getPointerHeight();

					if (pointerPos[0] >= ow - pw - SCROLLING_EDGE_MARGIN)
						scrollX = SCROLLING_DISTANCE;
					else if (pointerPos[0] <= SCROLLING_EDGE_MARGIN)
						scrollX = -SCROLLING_DISTANCE;

					if (pointerPos[1] >= oh - ph - SCROLLING_EDGE_MARGIN)
						scrollY = SCROLLING_DISTANCE;
					else if (pointerPos[1] <= SCROLLING_EDGE_MARGIN)
						scrollY = -SCROLLING_DISTANCE;

					scrollView.scrollBy(scrollX, scrollY);

					final int maxX = sessionView.getWidth() - scrollView.getWidth();
					final int maxY = sessionView.getHeight() - scrollView.getHeight();
					if ((scrollX < 0 && scrollView.getScrollX() <= 0) ||
					    (scrollX > 0 && scrollView.getScrollX() >= maxX))
						scrollX = 0;
					if ((scrollY < 0 && scrollView.getScrollY() <= 0) ||
					    (scrollY > 0 && scrollView.getScrollY() >= maxY))
						scrollY = 0;

					if (scrollX != 0 || scrollY != 0)
						handler.sendEmptyMessageDelayed(MSG_SCROLLING_REQUESTED, SCROLLING_TIMEOUT);
					break;
				}
			}
		}
	}

	// ****************************************************************************
	// Pinch-to-zoom listener (wired into SessionView's ScaleGestureDetector)

	private class PinchZoomListener extends ScaleGestureDetector.SimpleOnScaleGestureListener
	{
		private float scaleFactor = 1.0f;

		@Override public boolean onScaleBegin(ScaleGestureDetector detector)
		{
			scrollView.setScrollEnabled(false);
			return true;
		}

		@Override public boolean onScale(ScaleGestureDetector detector)
		{
			// calc scale factor
			scaleFactor *= detector.getScaleFactor();
			scaleFactor = Math.max(SessionView.MIN_SCALE_FACTOR,
			                       Math.min(scaleFactor, SessionView.MAX_SCALE_FACTOR));
			sessionView.setZoom(scaleFactor);

			if (!sessionView.isAtMinZoom() && !sessionView.isAtMaxZoom())
			{
				// transform scroll origin to the new zoom space
				float transOriginX = scrollView.getScrollX() * detector.getScaleFactor();
				float transOriginY = scrollView.getScrollY() * detector.getScaleFactor();

				// transform center point to the zoomed space
				float transCenterX =
				    (scrollView.getScrollX() + detector.getFocusX()) * detector.getScaleFactor();
				float transCenterY =
				    (scrollView.getScrollY() + detector.getFocusY()) * detector.getScaleFactor();

				// scroll by the difference between the distance of the
				// transformed center/origin point and their old distance
				// (focusX/Y)
				scrollView.scrollBy((int)((transCenterX - transOriginX) - detector.getFocusX()),
				                    (int)((transCenterY - transOriginY) - detector.getFocusY()));
			}

			return true;
		}

		@Override public void onScaleEnd(ScaleGestureDetector de)
		{
			scrollView.setScrollEnabled(true);
		}
	}
}
