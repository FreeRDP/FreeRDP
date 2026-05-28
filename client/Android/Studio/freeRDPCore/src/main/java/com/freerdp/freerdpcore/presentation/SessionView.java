/*
   Android Session view

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.text.InputType;
import android.util.AttributeSet;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import androidx.annotation.NonNull;

import com.freerdp.freerdpcore.application.SessionState;
import com.freerdp.freerdpcore.utils.DoubleGestureDetector;
import com.freerdp.freerdpcore.utils.GestureDetector;

import java.util.Stack;

public class SessionView extends View
{
	public static final float MAX_SCALE_FACTOR = 3.0f;
	public static final float MIN_SCALE_FACTOR = 0.75f;
	private static final String TAG = "SessionView";
	private static final float SCALE_FACTOR_DELTA = 0.0001f;
	private static final float TOUCH_SCROLL_DELTA = 10.0f;
	private int width;
	private int height;
	private BitmapDrawable surface;
	private Stack<Rect> invalidRegions;
	private int touchPointerPaddingWidth = 0;
	private int touchPointerPaddingHeight = 0;
	private SessionViewListener sessionViewListener = null;
	// helpers for scaling gesture handling
	private float scaleFactor = 1.0f;
	private Matrix scaleMatrix;
	private Matrix invScaleMatrix;
	private RectF invalidRegionF;
	private GestureDetector gestureDetector;
	private SessionState currentSession;

	private int[] cursorPixels = null;
	private int cursorWidth = 0;
	private int cursorHeight = 0;
	private int cursorHotX = 0;
	private int cursorHotY = 0;

	// private static final String TAG = "FreeRDP.SessionView";
	private DoubleGestureDetector doubleGestureDetector;
	public SessionView(Context context)
	{
		super(context);
		initSessionView(context);
	}

	public SessionView(Context context, AttributeSet attrs)
	{
		super(context, attrs);
		initSessionView(context);
	}

	public SessionView(Context context, AttributeSet attrs, int defStyle)
	{
		super(context, attrs, defStyle);
		initSessionView(context);
	}

	private void initSessionView(Context context)
	{
		// Ensure the view is focusable so the soft keyboard can attach to it (required for API 30+)
		setFocusable(true);
		setFocusableInTouchMode(true);

		invalidRegions = new Stack<>();
		gestureDetector = new GestureDetector(context, new SessionGestureListener(), null, true);
		doubleGestureDetector =
		    new DoubleGestureDetector(context, null, new SessionDoubleGestureListener());

		scaleFactor = 1.0f;
		scaleMatrix = new Matrix();
		invScaleMatrix = new Matrix();
		invalidRegionF = new RectF();
	}

	/* External Mouse Hover */
	@Override public boolean onHoverEvent(MotionEvent event)
	{
		if (event.getAction() == MotionEvent.ACTION_HOVER_MOVE)
		{
			// Handle hover move event
			float x = event.getX();
			float y = event.getY();
			// Perform actions based on the hover position (x, y)
			MotionEvent mappedEvent = mapTouchEvent(event);
			sessionViewListener.onSessionViewMouseMove((int)mappedEvent.getX(),
			                                           (int)mappedEvent.getY());
			mappedEvent.recycle();
		}
		// Return true to indicate that you've handled the event
		return true;
	}

	public void setScaleGestureDetector(ScaleGestureDetector scaleGestureDetector)
	{
		doubleGestureDetector.setScaleGestureDetector(scaleGestureDetector);
	}

	public void setSessionViewListener(SessionViewListener sessionViewListener)
	{
		this.sessionViewListener = sessionViewListener;
	}

	public void addInvalidRegion(Rect invalidRegion)
	{
		// correctly transform invalid region depending on current scaling
		invalidRegionF.set(invalidRegion);
		scaleMatrix.mapRect(invalidRegionF);
		invalidRegionF.roundOut(invalidRegion);

		invalidRegions.add(invalidRegion);
	}

	public void invalidateRegion()
	{
		invalidate(invalidRegions.pop());
	}

	public void onSurfaceChange(SessionState session)
	{
		surface = session.getSurface();
		Bitmap bitmap = surface.getBitmap();
		width = bitmap.getWidth();
		height = bitmap.getHeight();
		surface.setBounds(0, 0, width, height);

		setMinimumWidth(width);
		setMinimumHeight(height);

		requestLayout();
		currentSession = session;
	}

	public float getZoom()
	{
		return scaleFactor;
	}

	public void setZoom(float factor)
	{
		scaleFactor = factor;
		scaleMatrix.setScale(scaleFactor, scaleFactor);
		invScaleMatrix.setScale(1.0f / scaleFactor, 1.0f / scaleFactor);

		if (cursorPixels != null)
			applyScaledCursor();

		requestLayout();
	}

	public boolean isAtMaxZoom()
	{
		return (scaleFactor > (MAX_SCALE_FACTOR - SCALE_FACTOR_DELTA));
	}

	public boolean isAtMinZoom()
	{
		return (scaleFactor < (MIN_SCALE_FACTOR + SCALE_FACTOR_DELTA));
	}

	public boolean zoomIn(float factor)
	{
		boolean res = true;
		scaleFactor += factor;
		if (scaleFactor > (MAX_SCALE_FACTOR - SCALE_FACTOR_DELTA))
		{
			scaleFactor = MAX_SCALE_FACTOR;
			res = false;
		}
		setZoom(scaleFactor);
		return res;
	}

	public boolean zoomOut(float factor)
	{
		boolean res = true;
		scaleFactor -= factor;
		if (scaleFactor < (MIN_SCALE_FACTOR + SCALE_FACTOR_DELTA))
		{
			scaleFactor = MIN_SCALE_FACTOR;
			res = false;
		}
		setZoom(scaleFactor);
		return res;
	}

	public void setTouchPointerPadding(int width, int height)
	{
		touchPointerPaddingWidth = width;
		touchPointerPaddingHeight = height;
		requestLayout();
	}

	public int getTouchPointerPaddingWidth()
	{
		return touchPointerPaddingWidth;
	}

	public int getTouchPointerPaddingHeight()
	{
		return touchPointerPaddingHeight;
	}

	@Override public void onMeasure(int widthMeasureSpec, int heightMeasureSpec)
	{
		Log.v(TAG, width + "x" + height);
		this.setMeasuredDimension((int)(width * scaleFactor) + touchPointerPaddingWidth,
		                          (int)(height * scaleFactor) + touchPointerPaddingHeight);
	}

	@Override public void onDraw(@NonNull Canvas canvas)
	{
		super.onDraw(canvas);

		canvas.save();
		canvas.concat(scaleMatrix);
		canvas.drawColor(Color.BLACK);
		if (surface != null)
		{
			surface.draw(canvas);
		}
		canvas.restore();
	}

	// perform mapping on the touch event's coordinates according to the current scaling
	private MotionEvent mapTouchEvent(MotionEvent event)
	{
		MotionEvent mappedEvent = MotionEvent.obtain(event);
		float[] coordinates = { mappedEvent.getX(), mappedEvent.getY() };
		invScaleMatrix.mapPoints(coordinates);
		mappedEvent.setLocation(coordinates[0], coordinates[1]);
		return mappedEvent;
	}

	// perform mapping on the double touch event's coordinates according to the current scaling
	private MotionEvent mapDoubleTouchEvent(MotionEvent event)
	{
		MotionEvent mappedEvent = MotionEvent.obtain(event);
		float[] coordinates = { (mappedEvent.getX(0) + mappedEvent.getX(1)) / 2,
			                    (mappedEvent.getY(0) + mappedEvent.getY(1)) / 2 };
		invScaleMatrix.mapPoints(coordinates);
		mappedEvent.setLocation(coordinates[0], coordinates[1]);
		return mappedEvent;
	}

	@Override public boolean onTouchEvent(MotionEvent event)
	{
		// Physical mouse events: bypass gesture detector entirely.
		// Buttons are handled in onGenericMotionEvent; hover moves in onHoverEvent.
		// Only ACTION_MOVE with a button held (drag) needs handling here.
		if (event.isFromSource(InputDevice.SOURCE_MOUSE))
		{
			int action = event.getActionMasked();
			if (action == MotionEvent.ACTION_MOVE && event.getButtonState() != 0)
			{
				MotionEvent mapped = mapTouchEvent(event);
				sessionViewListener.onSessionViewMouseMove((int)mapped.getX(), (int)mapped.getY());
				mapped.recycle();
				return true;
			}
			return true;
		}

		boolean res = gestureDetector.onTouchEvent(event);
		res |= doubleGestureDetector.onTouchEvent(event);
		return res;
	}

	// Handle all physical mouse buttons here; finger taps come via onSingleTapUp.
	@Override public boolean onGenericMotionEvent(MotionEvent event)
	{
		final boolean isPointer = event.isFromSource(InputDevice.SOURCE_CLASS_POINTER);
		if (!isPointer)
			return false;

		final boolean isMouse = event.isFromSource(InputDevice.SOURCE_MOUSE);
		int action = event.getActionMasked();

		if (isMouse && (action == MotionEvent.ACTION_BUTTON_PRESS ||
		                action == MotionEvent.ACTION_BUTTON_RELEASE))
		{
			boolean down = action == MotionEvent.ACTION_BUTTON_PRESS;
			MotionEvent mapped = mapTouchEvent(event);
			int x = (int)mapped.getX();
			int y = (int)mapped.getY();
			mapped.recycle();

			switch (event.getActionButton())
			{
				case MotionEvent.BUTTON_PRIMARY:
					if (down)
						sessionViewListener.onSessionViewBeginTouch();
					sessionViewListener.onSessionViewLeftTouch(x, y, down);
					if (!down)
						sessionViewListener.onSessionViewEndTouch();
					return true;
				case MotionEvent.BUTTON_SECONDARY:
					if (down)
						sessionViewListener.onSessionViewBeginTouch();
					sessionViewListener.onSessionViewRightTouch(x, y, down);
					return true;
				case MotionEvent.BUTTON_TERTIARY:
					sessionViewListener.onSessionViewMiddleTouch(x, y, down);
					return true;
				default:
					return true; // consume unknown buttons silently
			}
		}

		if (action == MotionEvent.ACTION_SCROLL)
		{
			float vScroll = event.getAxisValue(MotionEvent.AXIS_VSCROLL);
			float hScroll = event.getAxisValue(MotionEvent.AXIS_HSCROLL);
			if (vScroll != 0)
				sessionViewListener.onSessionViewScroll(vScroll > 0);
			if (hScroll != 0)
				sessionViewListener.onSessionViewHScroll(hScroll > 0);
			return true;
		}

		return false;
	}

	public interface SessionViewListener
	{
		void onSessionViewBeginTouch();

		void onSessionViewEndTouch();

		void onSessionViewLeftTouch(int x, int y, boolean down);

		void onSessionViewMiddleTouch(int x, int y, boolean down);

		void onSessionViewRightTouch(int x, int y, boolean down);

		void onSessionViewMove(int x, int y);

		void onSessionViewMouseMove(int x, int y);

		void onSessionViewScroll(boolean down);

		void onSessionViewHScroll(boolean right);
	}

	public void setRemoteCursor(int[] pixels, int width, int height, int hotX, int hotY)
	{
		if (pixels == null || width == 0 || height == 0)
		{
			cursorPixels = null;
			setPointerIcon(PointerIcon.getSystemIcon(getContext(), PointerIcon.TYPE_NULL));
			return;
		}
		cursorPixels = pixels;
		cursorWidth = width;
		cursorHeight = height;
		cursorHotX = hotX;
		cursorHotY = hotY;
		applyScaledCursor();
	}

	private void applyScaledCursor()
	{
		int scaledWidth = Math.max(1, (int)(cursorWidth * scaleFactor));
		int scaledHeight = Math.max(1, (int)(cursorHeight * scaleFactor));
		Bitmap bm =
		    Bitmap.createBitmap(cursorPixels, cursorWidth, cursorHeight, Bitmap.Config.ARGB_8888);
		Bitmap scaled = Bitmap.createScaledBitmap(bm, scaledWidth, scaledHeight, true);
		PointerIcon icon =
		    PointerIcon.create(scaled, cursorHotX * scaleFactor, cursorHotY * scaleFactor);
		setPointerIcon(icon);
	}

	public void setDefaultCursor()
	{
		setPointerIcon(PointerIcon.getSystemIcon(getContext(), PointerIcon.TYPE_ARROW));
	}

	private class SessionGestureListener extends GestureDetector.SimpleOnGestureListener
	{
		boolean longPressInProgress = false;

		public boolean onDown(MotionEvent e)
		{
			return true;
		}

		public boolean onUp(MotionEvent e)
		{
			sessionViewListener.onSessionViewEndTouch();
			return true;
		}

		public void onLongPress(MotionEvent e)
		{
			MotionEvent mappedEvent = mapTouchEvent(e);
			sessionViewListener.onSessionViewBeginTouch();
			sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(),
			                                           (int)mappedEvent.getY(), true);
			longPressInProgress = true;
		}

		public void onLongPressUp(MotionEvent e)
		{
			MotionEvent mappedEvent = mapTouchEvent(e);
			sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(),
			                                           (int)mappedEvent.getY(), false);
			longPressInProgress = false;
			sessionViewListener.onSessionViewEndTouch();
		}

		public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY)
		{
			if (longPressInProgress)
			{
				MotionEvent mappedEvent = mapTouchEvent(e2);
				sessionViewListener.onSessionViewMove((int)mappedEvent.getX(),
				                                      (int)mappedEvent.getY());
				return true;
			}

			return false;
		}

		public boolean onDoubleTap(MotionEvent e)
		{
			// send 2nd click for double click
			MotionEvent mappedEvent = mapTouchEvent(e);
			sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(),
			                                           (int)mappedEvent.getY(), true);
			sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(),
			                                           (int)mappedEvent.getY(), false);
			return true;
		}

		public boolean onSingleTapUp(MotionEvent e)
		{
			// Physical mouse buttons are handled via ACTION_BUTTON_PRESS in onGenericMotionEvent.
			// If buttonState is non-zero this event came from a physical mouse button
			if (e.getButtonState() != 0)
				return false;

			// Finger touch -> left click
			MotionEvent mappedEvent = mapTouchEvent(e);
			sessionViewListener.onSessionViewBeginTouch();
			sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(),
			                                           (int)mappedEvent.getY(), true);
			sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(),
			                                           (int)mappedEvent.getY(), false);
			sessionViewListener.onSessionViewEndTouch();
			return true;
		}
	}

	private class SessionDoubleGestureListener
	    implements DoubleGestureDetector.OnDoubleGestureListener
	{
		private MotionEvent prevEvent = null;

		public boolean onDoubleTouchDown(MotionEvent e)
		{
			sessionViewListener.onSessionViewBeginTouch();
			prevEvent = MotionEvent.obtain(e);
			return true;
		}

		public boolean onDoubleTouchUp(MotionEvent e)
		{
			if (prevEvent != null)
			{
				prevEvent.recycle();
				prevEvent = null;
			}
			sessionViewListener.onSessionViewEndTouch();
			return true;
		}

		public boolean onDoubleTouchScroll(MotionEvent e1, MotionEvent e2)
		{
			// calc if user scrolled up or down (or if any scrolling happened at all)
			float deltaY = e2.getY() - prevEvent.getY();
			if (deltaY > TOUCH_SCROLL_DELTA)
			{
				sessionViewListener.onSessionViewScroll(true);
				prevEvent.recycle();
				prevEvent = MotionEvent.obtain(e2);
			}
			else if (deltaY < -TOUCH_SCROLL_DELTA)
			{
				sessionViewListener.onSessionViewScroll(false);
				prevEvent.recycle();
				prevEvent = MotionEvent.obtain(e2);
			}
			return true;
		}

		public boolean onDoubleTouchSingleTap(MotionEvent e)
		{
			// send single click
			MotionEvent mappedEvent = mapDoubleTouchEvent(e);
			sessionViewListener.onSessionViewRightTouch((int)mappedEvent.getX(),
			                                            (int)mappedEvent.getY(), true);
			sessionViewListener.onSessionViewRightTouch((int)mappedEvent.getX(),
			                                            (int)mappedEvent.getY(), false);
			return true;
		}
	}

	@Override public InputConnection onCreateInputConnection(EditorInfo outAttrs)
	{
		outAttrs.actionLabel = null;
		outAttrs.inputType = InputType.TYPE_NULL;
		outAttrs.imeOptions = EditorInfo.IME_ACTION_NONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI |
		                      EditorInfo.IME_FLAG_NO_FULLSCREEN;
		return new BaseInputConnection(this, false);
	}
}
