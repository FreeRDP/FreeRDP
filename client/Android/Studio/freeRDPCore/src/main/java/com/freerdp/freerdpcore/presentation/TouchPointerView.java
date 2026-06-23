/*
   Android Touch Pointer view

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;

import androidx.core.content.ContextCompat;
import androidx.core.widget.ImageViewCompat;

import com.freerdp.freerdpcore.R;

// Full-screen overlay hosting a draggable touch-pointer button cluster.
public class TouchPointerView extends FrameLayout
{
	private static final float SCROLL_DELTA = 10.0f;
	private static final int LONG_PRESS_MS = 500;

	private View cluster;
	private ImageView cursor;
	private ImageButton scrollButton;

	private TouchPointerListener listener = null;

	private float density;
	private int touchSlop;
	private boolean placed = false;

	// puck drag state
	private float downRawX, downRawY, startTransX, startTransY;
	private boolean dragging = false;
	private boolean holdDragging = false;

	// scroll state
	private float scrollLastRawY;
	private float scrollBaseHeight;
	private ValueAnimator pillAnimator;

	private int cursorTint;

	private final Handler uiHandler = new Handler(Looper.getMainLooper());
	private final Runnable longPress = () ->
	{
		if (!dragging && !holdDragging)
		{
			holdDragging = true;
			sendLeft(true);
		}
	};

	public TouchPointerView(Context context)
	{
		this(context, null);
	}

	public TouchPointerView(Context context, AttributeSet attrs)
	{
		this(context, attrs, 0);
	}

	public TouchPointerView(Context context, AttributeSet attrs, int defStyle)
	{
		super(context, attrs, defStyle);
		initTouchPointer(context);
	}

	private void initTouchPointer(Context context)
	{
		density = getResources().getDisplayMetrics().density;
		touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
		cursorTint = ContextCompat.getColor(context, R.color.tp_icon);
		setClipChildren(false);

		LayoutInflater.from(context).inflate(R.layout.touch_pointer, this, true);
		cluster = findViewById(R.id.tp_cluster);
		cursor = findViewById(R.id.tp_cursor);
		scrollButton = findViewById(R.id.tp_scroll);

		findViewById(R.id.tp_puck).setOnTouchListener((v, e) -> onPuckTouch(e));
		scrollButton.setOnTouchListener((v, e) -> onScrollTouch(e));

		findViewById(R.id.tp_close).setOnClickListener(v -> {
			if (listener != null)
				listener.onTouchPointerClose();
		});
		findViewById(R.id.tp_rclick).setOnClickListener(v -> {
			int[] h = hotspot();
			if (listener != null)
			{
				listener.onTouchPointerRightClick(h[0], h[1], true);
				listener.onTouchPointerRightClick(h[0], h[1], false);
			}
		});
		findViewById(R.id.tp_reset).setOnClickListener(v -> {
			if (listener != null)
				listener.onTouchPointerResetScrollZoom();
		});
		findViewById(R.id.tp_keyboard).setOnClickListener(v -> {
			if (listener != null)
				listener.onTouchPointerToggleKeyboard();
		});
		findViewById(R.id.tp_ext_keyboard).setOnClickListener(v -> {
			if (listener != null)
				listener.onTouchPointerToggleExtKeyboard();
		});
	}

	public void setTouchPointerListener(TouchPointerListener listener)
	{
		this.listener = listener;
	}

	public int getPointerWidth()
	{
		return cluster.getWidth() > 0
		    ? cluster.getWidth()
		    : getResources().getDimensionPixelSize(R.dimen.tp_cluster_size);
	}

	public int getPointerHeight()
	{
		return cluster.getHeight() > 0
		    ? cluster.getHeight()
		    : getResources().getDimensionPixelSize(R.dimen.tp_cluster_size);
	}

	public float[] getPointerPosition()
	{
		return new float[] { cluster.getX(), cluster.getY() };
	}

	// click hotspot == cursor tip == cluster top-left corner, in overlay coords
	private int[] hotspot()
	{
		return new int[] { (int)cluster.getX(), (int)cluster.getY() };
	}

	private void sendLeft(boolean down)
	{
		int[] h = hotspot();
		if (listener != null)
			listener.onTouchPointerLeftClick(h[0], h[1], down);
	}

	private void sendMove()
	{
		int[] h = hotspot();
		if (listener != null)
			listener.onTouchPointerMove(h[0], h[1]);
	}

	private void setClusterTranslation(float tx, float ty)
	{
		float maxX = getWidth() - cluster.getWidth();
		float maxY = getHeight() - cluster.getHeight();
		if (tx < 0)
			tx = 0;
		if (ty < 0)
			ty = 0;
		if (maxX > 0 && tx > maxX)
			tx = maxX;
		if (maxY > 0 && ty > maxY)
			ty = maxY;
		cluster.setTranslationX(tx);
		cluster.setTranslationY(ty);
	}

	@Override protected void onLayout(boolean changed, int l, int t, int r, int b)
	{
		super.onLayout(changed, l, t, r, b);
		if (!placed && getWidth() > 0 && cluster.getWidth() > 0)
		{
			placed = true;
			setClusterTranslation((getWidth() - cluster.getWidth()) / 2.0f,
			                      (getHeight() - cluster.getHeight()) / 2.0f);
		}
		else
		{
			setClusterTranslation(cluster.getTranslationX(), cluster.getTranslationY());
		}
	}

	private boolean onPuckTouch(MotionEvent e)
	{
		switch (e.getActionMasked())
		{
			case MotionEvent.ACTION_DOWN:
				downRawX = e.getRawX();
				downRawY = e.getRawY();
				startTransX = cluster.getTranslationX();
				startTransY = cluster.getTranslationY();
				dragging = false;
				holdDragging = false;
				uiHandler.postDelayed(longPress, LONG_PRESS_MS);
				return true;
			case MotionEvent.ACTION_MOVE:
			{
				float dx = e.getRawX() - downRawX;
				float dy = e.getRawY() - downRawY;
				if (!dragging && Math.hypot(dx, dy) > touchSlop)
				{
					dragging = true;
					if (!holdDragging)
						uiHandler.removeCallbacks(longPress);
				}
				if (dragging || holdDragging)
				{
					setClusterTranslation(startTransX + dx, startTransY + dy);
					sendMove();
				}
				return true;
			}
			case MotionEvent.ACTION_UP:
				uiHandler.removeCallbacks(longPress);
				if (holdDragging)
				{
					sendLeft(false);
					holdDragging = false;
				}
				else if (!dragging)
				{
					// tap -> left click (two quick taps register as a double-click)
					sendLeft(true);
					sendLeft(false);
				}
				if (listener != null)
					listener.onTouchPointerMoveEnd();
				return true;
			case MotionEvent.ACTION_CANCEL:
				uiHandler.removeCallbacks(longPress);
				if (holdDragging)
				{
					sendLeft(false);
					holdDragging = false;
				}
				if (listener != null)
					listener.onTouchPointerMoveEnd();
				return true;
		}
		return false;
	}

	private boolean onScrollTouch(MotionEvent e)
	{
		switch (e.getActionMasked())
		{
			case MotionEvent.ACTION_DOWN:
				scrollLastRawY = e.getRawY();
				scrollButton.setActivated(true);
				scrollButton.bringToFront();
				morphScroll(true);
				return true;
			case MotionEvent.ACTION_MOVE:
			{
				float dy = e.getRawY() - scrollLastRawY;
				if (dy > SCROLL_DELTA)
				{
					if (listener != null)
						listener.onTouchPointerScroll(true);
					scrollLastRawY = e.getRawY();
				}
				else if (dy < -SCROLL_DELTA)
				{
					if (listener != null)
						listener.onTouchPointerScroll(false);
					scrollLastRawY = e.getRawY();
				}
				return true;
			}
			case MotionEvent.ACTION_UP:
			case MotionEvent.ACTION_CANCEL:
				scrollButton.setActivated(false);
				morphScroll(false);
				return true;
		}
		return false;
	}

	// grow the scroll button into a tall pill (covering its neighbours) and back
	private void morphScroll(boolean expand)
	{
		if (scrollBaseHeight == 0)
			scrollBaseHeight = scrollButton.getHeight();
		float target = expand ? getResources().getDimensionPixelSize(R.dimen.tp_cluster_size)
		                      : scrollBaseHeight;
		if (pillAnimator != null)
			pillAnimator.cancel();
		pillAnimator = ValueAnimator.ofFloat(scrollButton.getHeight(), target);
		pillAnimator.setDuration(140);
		pillAnimator.addUpdateListener(a -> {
			float h = (float)a.getAnimatedValue();
			ViewGroup.LayoutParams lp = scrollButton.getLayoutParams();
			lp.height = Math.round(h);
			scrollButton.setLayoutParams(lp);
			scrollButton.setTranslationY(-(h - scrollBaseHeight) / 2.0f);
		});
		pillAnimator.start();
	}

	// Set the real remote cursor bitmap (null clears to the fallback); never recycled.
	public void setRemoteCursor(int[] pixels, int width, int height, int hotX, int hotY)
	{
		ViewGroup.LayoutParams lp = cursor.getLayoutParams();
		if (pixels == null || width <= 0 || height <= 0)
		{
			cursor.setImageResource(R.drawable.ic_cursor);
			ImageViewCompat.setImageTintList(cursor, ColorStateList.valueOf(cursorTint));
			int s = getResources().getDimensionPixelSize(R.dimen.tp_cursor_size);
			lp.width = s;
			lp.height = s;
			cursor.setLayoutParams(lp);
			cursor.setTranslationX(0);
			cursor.setTranslationY(0);
			return;
		}
		Bitmap bmp = Bitmap.createBitmap(pixels, width, height, Bitmap.Config.ARGB_8888);
		float scale = 40 * density / height;
		if (scale < 1.2f)
			scale = 1.2f;
		if (scale > 3.0f)
			scale = 3.0f;
		ImageViewCompat.setImageTintList(cursor, null);
		// filterBitmap=false -> nearest-neighbour scaling keeps the small cursor crisp
		BitmapDrawable bd = new BitmapDrawable(getResources(), bmp);
		bd.setFilterBitmap(false);
		cursor.setImageDrawable(bd);
		lp.width = Math.round(width * scale);
		lp.height = Math.round(height * scale);
		cursor.setLayoutParams(lp);
		// place the bitmap hotspot pixel on the cluster's top-left corner (0,0)
		cursor.setTranslationX(-hotX * scale);
		cursor.setTranslationY(-hotY * scale);
	}

	// touch pointer listener - triggered when an action field is hit
	public interface TouchPointerListener
	{
		void onTouchPointerClose();

		void onTouchPointerLeftClick(int x, int y, boolean down);

		void onTouchPointerRightClick(int x, int y, boolean down);

		void onTouchPointerMove(int x, int y);

		void onTouchPointerMoveEnd();

		void onTouchPointerScroll(boolean down);

		void onTouchPointerToggleKeyboard();

		void onTouchPointerToggleExtKeyboard();

		void onTouchPointerResetScrollZoom();
	}
}
