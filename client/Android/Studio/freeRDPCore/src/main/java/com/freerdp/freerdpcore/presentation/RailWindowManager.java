/*
   Android RAIL/RemoteApp window overlay manager

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import java.util.HashMap;

class RailWindowManager
{
	private final Context context;
	private final FrameLayout container;
	private final SessionView sessionView;
	private final Handler ui = new Handler(Looper.getMainLooper());

	private final HashMap<Long, ImageView> windows = new HashMap<>();
	private final HashMap<Long, int[]> rects = new HashMap<>();
	private final HashMap<Long, Bitmap> bitmaps = new HashMap<>();
	private long[] zOrder = null;
	private long activeWindowId = 0;

	RailWindowManager(Context context, FrameLayout container, SessionView sessionView)
	{
		this.context = context;
		this.container = container;
		this.sessionView = sessionView;
		sessionView.setOnZoomChangedListener(zoom -> repositionAll());
	}

	void onWindowUpdate(long windowId, int width, int height, int[] pixels)
	{
		if (pixels == null)
			return;
		ui.post(() -> handleWindowUpdate(windowId, width, height, pixels));
	}

	void onWindowMove(long windowId, int x, int y, int w, int h)
	{
		ui.post(() -> handleWindowMove(windowId, x, y, w, h));
	}

	void onWindowHide(long windowId)
	{
		ui.post(() -> {
			ImageView iv = windows.get(windowId);
			if (iv != null)
				iv.setVisibility(View.INVISIBLE);
		});
	}

	void onWindowDestroy(long windowId)
	{
		ui.post(() -> handleWindowDestroy(windowId));
	}

	void onSessionEnd()
	{
		ui.post(() -> {
			clearWindows();
			sessionView.setRailMode(false);
		});
	}

	void onMonitoredDesktop(long[] windowIds, long active)
	{
		ui.post(() -> handleMonitoredDesktop(windowIds, active));
	}

	// Removes all overlays; must be called on the UI thread.
	void clear()
	{
		clearWindows();
	}

	private void handleWindowUpdate(long windowId, int width, int height, int[] pixels)
	{
		if (container == null)
			return;
		// Reuse the per-window bitmap; reallocate only on size change.
		Bitmap bm = bitmaps.get(windowId);
		boolean newBitmap = bm == null || bm.getWidth() != width || bm.getHeight() != height;
		if (newBitmap)
		{
			bm = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
			bitmaps.put(windowId, bm);
		}
		bm.setPixels(pixels, 0, width, 0, 0, width, height);
		ImageView iv = windows.get(windowId);
		if (iv == null)
		{
			iv = new ImageView(context);
			iv.setScaleType(ImageView.ScaleType.FIT_XY);
			iv.setVisibility(View.INVISIBLE);
			container.addView(iv);
			windows.put(windowId, iv);
			// Order may have arrived before this view existed; restack now.
			restack();
			newBitmap = true;
		}
		if (newBitmap)
		{
			iv.setImageBitmap(bm);
			ViewGroup.LayoutParams lp = iv.getLayoutParams();
			lp.width = width;
			lp.height = height;
			iv.requestLayout();
		}
		else
		{
			iv.invalidate();
		}
		if (rects.containsKey(windowId))
		{
			position(iv, windowId);
			iv.setVisibility(View.VISIBLE);
			sessionView.setRailMode(true);
		}
	}

	private void handleWindowMove(long windowId, int x, int y, int w, int h)
	{
		if (x != -1 || y != -1)
			rects.put(windowId, new int[] { x, y, w, h });
		ImageView iv = windows.get(windowId);
		if (iv != null)
		{
			if (w > 0 && h > 0)
			{
				ViewGroup.LayoutParams lp = iv.getLayoutParams();
				if (lp.width != w || lp.height != h)
				{
					lp.width = w;
					lp.height = h;
					iv.requestLayout();
				}
			}
			position(iv, windowId);
			iv.setVisibility(View.VISIBLE);
			sessionView.setRailMode(true);
		}
	}

	private void handleWindowDestroy(long windowId)
	{
		rects.remove(windowId);
		bitmaps.remove(windowId);
		ImageView iv = windows.remove(windowId);
		if (iv != null && container != null)
			container.removeView(iv);
	}

	private void handleMonitoredDesktop(long[] windowIds, long active)
	{
		if (windowIds != null)
		{
			zOrder = windowIds;
			if (windowIds.length > 0)
				activeWindowId = windowIds[0];
		}
		if (active != 0)
			activeWindowId = active;
		restack();
	}

	// Match client stacking to the server order (input is routed server-side by coordinate).
	private void restack()
	{
		if (container == null)
			return;
		if (zOrder != null)
			for (int i = zOrder.length - 1; i >= 0; i--)
			{
				ImageView iv = windows.get(zOrder[i]);
				if (iv != null)
					iv.bringToFront();
			}
		if (activeWindowId != 0 &&
		    (zOrder == null || zOrder.length == 0 || zOrder[0] != activeWindowId))
		{
			ImageView iv = windows.get(activeWindowId);
			if (iv != null)
				iv.bringToFront();
		}
	}

	private void repositionAll()
	{
		for (HashMap.Entry<Long, ImageView> entry : windows.entrySet())
			position(entry.getValue(), entry.getKey());
	}

	private void position(ImageView iv, long windowId)
	{
		int[] rect = rects.get(windowId);
		if (rect == null)
			return;
		float zoom = sessionView != null ? sessionView.getZoom() : 1.0f;
		iv.setPivotX(0);
		iv.setPivotY(0);
		iv.setScaleX(zoom);
		iv.setScaleY(zoom);
		iv.setX(rect[0] * zoom);
		iv.setY(rect[1] * zoom);
	}

	private void clearWindows()
	{
		if (container != null)
			for (ImageView iv : windows.values())
				container.removeView(iv);
		windows.clear();
		rects.clear();
		bitmaps.clear();
		zOrder = null;
		activeWindowId = 0;
	}
}
