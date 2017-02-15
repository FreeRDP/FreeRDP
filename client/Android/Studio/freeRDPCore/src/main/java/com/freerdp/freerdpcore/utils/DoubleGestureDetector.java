/*
   2 finger gesture detector

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import android.content.Context;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

import com.freerdp.freerdpcore.utils.GestureDetector.OnGestureListener;

public class DoubleGestureDetector {
	/**
     * The listener that is used to notify when gestures occur.
     * If you want to listen for all the different gestures then implement
     * this interface. If you only want to listen for a subset it might
     * be easier to extend {@link SimpleOnGestureListener}.
     */
    public interface OnDoubleGestureListener {

    	/**
    	 * Notified when a multi tap event starts
    	 */
    	boolean onDoubleTouchDown(MotionEvent e);

    	/**
    	 * Notified when a multi tap event ends
    	 */
    	boolean onDoubleTouchUp(MotionEvent e);

        /**
         * Notified when a tap occurs with the up {@link MotionEvent}
         * that triggered it.
         *
         * @param e The up motion event that completed the first tap
         * @return true if the event is consumed, else false
         */
        boolean onDoubleTouchSingleTap(MotionEvent e);

        /**
         * Notified when a scroll occurs with the initial on down {@link MotionEvent} and the
         * current move {@link MotionEvent}. The distance in x and y is also supplied for
         * convenience.
         *
         * @param e1 The first down motion event that started the scrolling.
         * @param e2 The move motion event that triggered the current onScroll.
         * @param distanceX The distance along the X axis that has been scrolled since the last
         *              call to onScroll. This is NOT the distance between {@code e1}
         *              and {@code e2}.
         * @param distanceY The distance along the Y axis that has been scrolled since the last
         *              call to onScroll. This is NOT the distance between {@code e1}
         *              and {@code e2}.
         * @return true if the event is consumed, else false
         */
        boolean onDoubleTouchScroll(MotionEvent e1, MotionEvent e2);
    }


    private int mPointerDistanceSquare;

	// timeout during that the second finger has to touch the screen before the double finger detection is cancelled
	private static final long DOUBLE_TOUCH_TIMEOUT = 100;

	// timeout during that an UP event will trigger a single double touch event
	private static final long SINGLE_DOUBLE_TOUCH_TIMEOUT = 1000;

    // constants for Message.what used by GestureHandler below
    private static final int TAP = 1;

	// different detection modes
	private static final int MODE_UNKNOWN = 0;
	private static final int MODE_PINCH_ZOOM = 1;
	private static final int MODE_SCROLL = 2;
	
    private final OnDoubleGestureListener mListener;

    private int mCurrentMode;
    private int mScrollDetectionScore;
    private static final int SCROLL_SCORE_TO_REACH = 20;
    
    private ScaleGestureDetector scaleGestureDetector;
    
    private boolean mCancelDetection;
    private boolean mDoubleInProgress;
    
    private GestureHandler mHandler;

    private MotionEvent mCurrentDownEvent;
    private MotionEvent mCurrentDoubleDownEvent;
    private MotionEvent mPreviousUpEvent;
    private MotionEvent mPreviousPointerUpEvent;
  
    private class GestureHandler extends Handler {
        GestureHandler() {
            super();
        }

        GestureHandler(Handler handler) {
            super(handler.getLooper());
        }

    }
    
    /**
     * Creates a GestureDetector with the supplied listener.
     * You may only use this constructor from a UI thread (this is the usual situation).
     * @see android.os.Handler#Handler()
     *
     * @param context the application's context
     * @param listener the listener invoked for all the callbacks, this must
     * not be null.
     *
     * @throws NullPointerException if {@code listener} is null.
     */
    public DoubleGestureDetector(Context context, Handler handler, OnDoubleGestureListener listener) {
        mListener = listener;
        init(context, handler);
    }

    private void init(Context context, Handler handler) {
        if (mListener == null) {
            throw new NullPointerException("OnGestureListener must not be null");
        }
        
        if(handler != null)
        	mHandler = new GestureHandler(handler);
        else
        	mHandler = new GestureHandler();
                
        // we use 1cm distance to decide between scroll and pinch zoom
        //  - first convert cm to inches
        //  - then multiply inches by dots per inch
        float distInches = 0.5f / 2.54f;
        float distPixelsX = distInches * context.getResources().getDisplayMetrics().xdpi; 
        float distPixelsY = distInches * context.getResources().getDisplayMetrics().ydpi; 
        
        mPointerDistanceSquare =   (int)(distPixelsX * distPixelsX + distPixelsY * distPixelsY);                
    }

    /**
     * Set scale gesture detector
     * @param scaleGestureDetector
     */    
	public void setScaleGestureDetector(ScaleGestureDetector scaleGestureDetector) {
		this.scaleGestureDetector = scaleGestureDetector;
	}
    
    /**
     * Analyzes the given motion event and if applicable triggers the
     * appropriate callbacks on the {@link OnGestureListener} supplied.
     *
     * @param ev The current motion event.
     * @return true if the {@link OnGestureListener} consumed the event,
     *              else false.
     */
    public boolean onTouchEvent(MotionEvent ev) 
    {
        boolean handled = false;
        final int action = ev.getAction();        
    	//dumpEvent(ev);

        switch (action & MotionEvent.ACTION_MASK) {
        case MotionEvent.ACTION_DOWN:
            if (mCurrentDownEvent != null)
                mCurrentDownEvent.recycle();

            mCurrentMode = MODE_UNKNOWN;
            mCurrentDownEvent = MotionEvent.obtain(ev);
            mCancelDetection = false;
            mDoubleInProgress = false;
            mScrollDetectionScore = 0;
            handled = true;
            break;

        case MotionEvent.ACTION_POINTER_UP:            
        	if(mPreviousPointerUpEvent != null)
        		mPreviousPointerUpEvent.recycle();
        	mPreviousPointerUpEvent = MotionEvent.obtain(ev);
            break;

        case MotionEvent.ACTION_POINTER_DOWN:
        	// more than 2 fingers down? cancel
        	// 2nd finger touched too late? cancel
        	if(ev.getPointerCount() > 2 || (ev.getEventTime() - mCurrentDownEvent.getEventTime()) > DOUBLE_TOUCH_TIMEOUT)
        	{
        		cancel();
        		break;
        	}
        	
        	// detection cancelled?
        	if(mCancelDetection)
        		break;

        	// double touch gesture in progress
        	mDoubleInProgress = true;
            if (mCurrentDoubleDownEvent != null)
                mCurrentDoubleDownEvent.recycle();
            mCurrentDoubleDownEvent = MotionEvent.obtain(ev);

            // set detection mode to unkown and send a TOUCH timeout event to detect single taps
            mCurrentMode = MODE_UNKNOWN;
            mHandler.sendEmptyMessageDelayed(TAP, SINGLE_DOUBLE_TOUCH_TIMEOUT);
                      
            handled |= mListener.onDoubleTouchDown(ev);
            break;

        case MotionEvent.ACTION_MOVE:

        	// detection cancelled or not active?
        	if(mCancelDetection || !mDoubleInProgress || ev.getPointerCount() != 2)
        		break;

        	// determine mode
        	if(mCurrentMode == MODE_UNKNOWN)
        	{
        		// did the pointer distance change? 
        		if(pointerDistanceChanged(mCurrentDoubleDownEvent, ev))
        		{
        			handled |= scaleGestureDetector.onTouchEvent(mCurrentDownEvent);
        			MotionEvent e = MotionEvent.obtain(ev);
        			e.setAction(mCurrentDoubleDownEvent.getAction());
        			handled |= scaleGestureDetector.onTouchEvent(e);
        			mCurrentMode = MODE_PINCH_ZOOM;
        			break;
        		}
        		else
        		{
        			mScrollDetectionScore++;
        			if(mScrollDetectionScore >= SCROLL_SCORE_TO_REACH)
        				mCurrentMode = MODE_SCROLL;
        		}
        	}

        	switch(mCurrentMode)
        	{
        		case MODE_PINCH_ZOOM:
        			if(scaleGestureDetector != null)
        				handled |= scaleGestureDetector.onTouchEvent(ev);
        			break;
        			
        		case MODE_SCROLL:
                    handled = mListener.onDoubleTouchScroll(mCurrentDownEvent, ev);
        			break;
        			        			
        		default:
        			handled = true;
        			break;
        	}
        	        	
            break;

        case MotionEvent.ACTION_UP:
        	// fingers were not removed equally? cancel
        	if(mPreviousPointerUpEvent != null && (ev.getEventTime() - mPreviousPointerUpEvent.getEventTime()) > DOUBLE_TOUCH_TIMEOUT)
        	{
        		mPreviousPointerUpEvent.recycle();
        		mPreviousPointerUpEvent = null;
        		cancel();
        		break;
        	}
        	
        	// detection cancelled or not active?
        	if(mCancelDetection || !mDoubleInProgress)
        		break;
        	
        	boolean hasTapEvent = mHandler.hasMessages(TAP);
            MotionEvent currentUpEvent = MotionEvent.obtain(ev);
            if (mCurrentMode == MODE_UNKNOWN && hasTapEvent) 
                handled = mListener.onDoubleTouchSingleTap(mCurrentDoubleDownEvent);
            else if(mCurrentMode == MODE_PINCH_ZOOM)
            	handled = scaleGestureDetector.onTouchEvent(ev);

            if (mPreviousUpEvent != null)
                mPreviousUpEvent.recycle();

            // Hold the event we obtained above - listeners may have changed the original.
            mPreviousUpEvent = currentUpEvent;
            handled |= mListener.onDoubleTouchUp(ev);
            break;
        
        case MotionEvent.ACTION_CANCEL:
            cancel();
            break;
        }
        
        if((action == MotionEvent.ACTION_MOVE) && handled == false)
        	handled = true;
        
        return handled;
    }

    private void cancel() {
    	mHandler.removeMessages(TAP);
    	mCurrentMode = MODE_UNKNOWN;
        mCancelDetection = true;
        mDoubleInProgress = false;
    }
    
    /*
	private void dumpEvent(MotionEvent event) {
		   String names[] = { "DOWN" , "UP" , "MOVE" , "CANCEL" , "OUTSIDE" ,
		      "POINTER_DOWN" , "POINTER_UP" , "7?" , "8?" , "9?" };
		   StringBuilder sb = new StringBuilder();
		   int action = event.getAction();
		   int actionCode = action & MotionEvent.ACTION_MASK;
		   sb.append("event ACTION_" ).append(names[actionCode]);
		   if (actionCode == MotionEvent.ACTION_POINTER_DOWN
		         || actionCode == MotionEvent.ACTION_POINTER_UP) {
		      sb.append("(pid " ).append(
		      action >> MotionEvent.ACTION_POINTER_ID_SHIFT);
		      sb.append(")" );
		   }
		   sb.append("[" );
		   for (int i = 0; i < event.getPointerCount(); i++) {
		      sb.append("#" ).append(i);
		      sb.append("(pid " ).append(event.getPointerId(i));
		      sb.append(")=" ).append((int) event.getX(i));
		      sb.append("," ).append((int) event.getY(i));
		      if (i + 1 < event.getPointerCount())
		         sb.append(";" );
		   }
		   sb.append("]" );
		   Log.d("DoubleDetector", sb.toString());
		}	
    */
   
    // returns true of the distance between the two pointers changed   
    private boolean pointerDistanceChanged(MotionEvent oldEvent, MotionEvent newEvent)
    {    	
       	int deltaX1 = Math.abs((int)oldEvent.getX(0) - (int)oldEvent.getX(1));
        int deltaX2 = Math.abs((int)newEvent.getX(0) - (int)newEvent.getX(1));
        int distXSquare = (deltaX2 - deltaX1) * (deltaX2 - deltaX1); 
        
       	int deltaY1 = Math.abs((int)oldEvent.getY(0) - (int)oldEvent.getY(1));
        int deltaY2 = Math.abs((int)newEvent.getY(0) - (int)newEvent.getY(1));
        int distYSquare = (deltaY2 - deltaY1) * (deltaY2 - deltaY1); 
     	   	
        return (distXSquare + distYSquare) > mPointerDistanceSquare;    	    		
    }

}
