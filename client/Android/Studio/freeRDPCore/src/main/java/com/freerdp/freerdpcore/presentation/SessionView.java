/*
   Android Session view

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.BitmapDrawable;

import java.util.*;

import com.freerdp.freerdpcore.application.SessionState;
import com.freerdp.freerdpcore.utils.DoubleGestureDetector;
import com.freerdp.freerdpcore.utils.GestureDetector;


public class SessionView extends View
{
	private static final String TAG = "SessionView";

	public interface SessionViewListener {
		abstract void onSessionViewBeginTouch();
		abstract void onSessionViewEndTouch();
		abstract void onSessionViewLeftTouch(int x, int y, boolean down);
		abstract void onSessionViewRightTouch(int x, int y, boolean down);
		abstract void onSessionViewMove(int x, int y);
		abstract void onSessionViewScroll(boolean down);
	}
	
	private int width;
	private int height;
	private BitmapDrawable surface;
	private Stack<Rect> invalidRegions;
	
	private int touchPointerPaddingWidth = 0;
	private int touchPointerPaddingHeight = 0;
	
	private SessionViewListener sessionViewListener = null;
	
	public static final float MAX_SCALE_FACTOR = 3.0f;
	public static final float MIN_SCALE_FACTOR = 1.0f;
	private static final float SCALE_FACTOR_DELTA = 0.0001f;

	private static final float TOUCH_SCROLL_DELTA = 10.0f;

	// helpers for scaling gesture handling
	private float scaleFactor = 1.0f;
	private Matrix scaleMatrix;
	private Matrix invScaleMatrix;
	private RectF invalidRegionF;

	//private static final String TAG = "FreeRDP.SessionView";

	private GestureDetector gestureDetector;
	private DoubleGestureDetector doubleGestureDetector;
	
	private class SessionGestureListener extends GestureDetector.SimpleOnGestureListener {
		boolean longPressInProgress = false;
		
		public boolean onDown(MotionEvent e) {
			return true;
		}
		
		public boolean onUp(MotionEvent e) {
        	sessionViewListener.onSessionViewEndTouch();
			return true;
		}
		
        public void onLongPress(MotionEvent e) {
        	MotionEvent mappedEvent = mapTouchEvent(e);
			sessionViewListener.onSessionViewBeginTouch();
        	sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(), (int)mappedEvent.getY(), true);
        	longPressInProgress = true;
        }

        public void onLongPressUp(MotionEvent e) {
        	MotionEvent mappedEvent = mapTouchEvent(e);
        	sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(), (int)mappedEvent.getY(), false);
        	longPressInProgress = false;
        	sessionViewListener.onSessionViewEndTouch();
        }

        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        	if(longPressInProgress)
        	{
            	MotionEvent mappedEvent = mapTouchEvent(e2);
            	sessionViewListener.onSessionViewMove((int)mappedEvent.getX(), (int)mappedEvent.getY());
                return true;
        	}
        	
        	return false;
        }

        public boolean onDoubleTap(MotionEvent e) {
        	// send 2nd click for double click
        	MotionEvent mappedEvent = mapTouchEvent(e);
        	sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(), (int)mappedEvent.getY(), true);
        	sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(), (int)mappedEvent.getY(), false);
            return true;
        }

        public boolean onSingleTapUp(MotionEvent e) {
        	// send single click
        	MotionEvent mappedEvent = mapTouchEvent(e);
			sessionViewListener.onSessionViewBeginTouch();
        	sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(), (int)mappedEvent.getY(), true);
        	sessionViewListener.onSessionViewLeftTouch((int)mappedEvent.getX(), (int)mappedEvent.getY(), false);
        	sessionViewListener.onSessionViewEndTouch();
            return true;
        }		
	}

	private class SessionDoubleGestureListener implements DoubleGestureDetector.OnDoubleGestureListener {
		private MotionEvent prevEvent = null;

		public boolean onDoubleTouchDown(MotionEvent e) {
			sessionViewListener.onSessionViewBeginTouch();
			prevEvent = MotionEvent.obtain(e);
			return true;
		}
		
		public boolean onDoubleTouchUp(MotionEvent e) {
			if(prevEvent != null)
			{
				prevEvent.recycle();
				prevEvent = null;
			}        	
			sessionViewListener.onSessionViewEndTouch();
			return true;
		}

		public boolean onDoubleTouchScroll(MotionEvent e1, MotionEvent e2) {
  			// calc if user scrolled up or down (or if any scrolling happened at all)
			float deltaY = e2.getY() - prevEvent.getY();
			if(deltaY > TOUCH_SCROLL_DELTA)
			{
				sessionViewListener.onSessionViewScroll(true);
				prevEvent.recycle();
				prevEvent = MotionEvent.obtain(e2);
			}
			else if(deltaY < -TOUCH_SCROLL_DELTA)
			{
				sessionViewListener.onSessionViewScroll(false);
				prevEvent.recycle();
				prevEvent = MotionEvent.obtain(e2);
			}
            return true;
        }

        public boolean onDoubleTouchSingleTap(MotionEvent e) {
        	// send single click
        	MotionEvent mappedEvent = mapDoubleTouchEvent(e);
        	sessionViewListener.onSessionViewRightTouch((int)mappedEvent.getX(), (int)mappedEvent.getY(), true);
        	sessionViewListener.onSessionViewRightTouch((int)mappedEvent.getX(), (int)mappedEvent.getY(), false);
            return true;
        }		
	}

	private void initSessionView(Context context)
	{		
		invalidRegions = new Stack<Rect>();
		gestureDetector = new GestureDetector(context, new SessionGestureListener(), null, true);
		doubleGestureDetector = new DoubleGestureDetector(context, null, new SessionDoubleGestureListener());
		
		scaleFactor = 1.0f;
		scaleMatrix = new Matrix();
		invScaleMatrix = new Matrix();
		invalidRegionF = new RectF();		
	}	
	
	public SessionView(Context context) {
		super(context);
		initSessionView(context);
	}

	public SessionView(Context context, AttributeSet attrs) {
		super(context, attrs);
		initSessionView(context);
	}

	public SessionView(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		initSessionView(context);
	}	
	
	public void setScaleGestureDetector(ScaleGestureDetector scaleGestureDetector) {
		doubleGestureDetector.setScaleGestureDetector(scaleGestureDetector);
	}

	public void setSessionViewListener(SessionViewListener sessionViewListener) {
		this.sessionViewListener = sessionViewListener;
	}
	
	public void addInvalidRegion(Rect invalidRegion) {
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
	}

	public void setZoom(float factor) {
		// calc scale matrix and inverse scale matrix (to correctly transform the view and moues coordinates)
		scaleFactor = factor;
		scaleMatrix.setScale(scaleFactor, scaleFactor);
		invScaleMatrix.setScale(1.0f / scaleFactor, 1.0f / scaleFactor);
				
		// update layout
		requestLayout();
	}	
	
	public float getZoom() {
		return scaleFactor;
	}
	
	public boolean isAtMaxZoom() {
		return (scaleFactor > (MAX_SCALE_FACTOR - SCALE_FACTOR_DELTA));
	}

	public boolean isAtMinZoom() {
		return (scaleFactor < (MIN_SCALE_FACTOR + SCALE_FACTOR_DELTA));
	}

	public boolean zoomIn(float factor) {
		boolean res = true;
		scaleFactor += factor;
		if(scaleFactor > (MAX_SCALE_FACTOR - SCALE_FACTOR_DELTA))
		{
			scaleFactor = MAX_SCALE_FACTOR;
			res = false;
		}
		setZoom(scaleFactor);
		return res;		
	}

	public boolean zoomOut(float factor) {
		boolean res = true;
		scaleFactor -= factor;
		if(scaleFactor < (MIN_SCALE_FACTOR + SCALE_FACTOR_DELTA))
		{
			scaleFactor = MIN_SCALE_FACTOR;
			res = false;
		}
		setZoom(scaleFactor);
		return res;				
	}

	public void setTouchPointerPadding(int widht, int height) {
		touchPointerPaddingWidth = widht;
		touchPointerPaddingHeight = height;
		requestLayout();
	}
	
	public int getTouchPointerPaddingWidth() {
		return touchPointerPaddingWidth;
	}
	
	public int getTouchPointerPaddingHeight() {
		return touchPointerPaddingHeight;
	}

	@Override
	public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		Log.v(TAG, width + "x" + height);
		this.setMeasuredDimension((int)(width * scaleFactor) + touchPointerPaddingWidth, (int)(height * scaleFactor) + touchPointerPaddingHeight);	
	}

	@Override
	public void onDraw(Canvas canvas)
	{
		super.onDraw(canvas);

		canvas.save();
		canvas.concat(scaleMatrix);
		surface.draw(canvas);
		canvas.restore();
	}

	// dirty hack: we call back to our activity and call onBackPressed as this doesn't reach us when the soft keyboard is shown ...
	@Override
	public boolean dispatchKeyEventPreIme(KeyEvent event) {
		if(event.getKeyCode() == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_DOWN)
			((SessionActivity)this.getContext()).onBackPressed();
		return super.dispatchKeyEventPreIme(event);
	}

	// perform mapping on the touch event's coordinates according to the current scaling
	private MotionEvent mapTouchEvent(MotionEvent event) {
		MotionEvent mappedEvent = MotionEvent.obtain(event);
		float[] coordinates = { mappedEvent.getX(), mappedEvent.getY() };
		invScaleMatrix.mapPoints(coordinates);
		mappedEvent.setLocation(coordinates[0], coordinates[1]);
		return mappedEvent;
	}

	// perform mapping on the double touch event's coordinates according to the current scaling
	private MotionEvent mapDoubleTouchEvent(MotionEvent event) {
		MotionEvent mappedEvent = MotionEvent.obtain(event);		
		float[] coordinates = { (mappedEvent.getX(0) + mappedEvent.getX(1)) / 2, (mappedEvent.getY(0) + mappedEvent.getY(1)) / 2 };
		invScaleMatrix.mapPoints(coordinates);
		mappedEvent.setLocation(coordinates[0], coordinates[1]);
		return mappedEvent;
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {				
		boolean res = gestureDetector.onTouchEvent(event);
		res |= doubleGestureDetector.onTouchEvent(event);			
		return res;
	}
	
}
