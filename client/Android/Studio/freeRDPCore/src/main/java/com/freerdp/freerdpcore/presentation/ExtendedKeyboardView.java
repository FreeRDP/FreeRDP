/*
   Android Extended On-screen Keyboard

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.content.Context;
import android.content.res.ColorStateList;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.HapticFeedbackConstants;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.content.ContextCompat;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.utils.KeyboardMapper;

// One keyboard: a persistent modifier/action bar plus a collapsible, paged key panel.
// Keys carry an @integer/keycode_* value and are fed straight to KeyboardMapper via the listener.
public class ExtendedKeyboardView extends LinearLayout
{
	public static final int PAGE_SPECIAL = 0;
	public static final int PAGE_NUM = 1;

	public interface Listener
	{
		void onKey(int keycode);

		void onKeyLock(int keycode);

		int getModifierState(int keycode);

		void onExpandedChanged(boolean expanded);
	}

	// The key fill only says whether the modifier is engaged; armed vs locked is the lamp's job.
	private static final class ModifierKey
	{
		private final View key;
		private final View led;

		ModifierKey(View key, View led)
		{
			this.key = key;
			this.led = led;
		}

		void setState(boolean on, boolean locked)
		{
			key.setActivated(on || locked);
			led.setActivated(on || locked);
			led.setSelected(locked);
		}
	}

	private Listener listener;
	private View panel;
	private View bar;
	private TextView expandKey;
	private final View[] pages = new View[2];
	private final SparseArray<ModifierKey> modifierKeys = new SparseArray<>();
	private final Handler repeatHandler = new Handler(Looper.getMainLooper());
	private int currentPage = PAGE_SPECIAL;
	private boolean expanded = false;
	private int insetLeft = 0;
	private int insetRight = 0;
	private int insetBottom = 0;

	public ExtendedKeyboardView(Context context)
	{
		this(context, null);
	}

	public ExtendedKeyboardView(Context context, AttributeSet attrs)
	{
		super(context, attrs);
		setOrientation(VERTICAL);
		build();
	}

	public void setListener(Listener l)
	{
		this.listener = l;
	}

	private int code(int resId)
	{
		return getResources().getInteger(resId);
	}

	private int px(int dimenId)
	{
		return getResources().getDimensionPixelSize(dimenId);
	}

	private void build()
	{
		bar = buildBar();
		addView(bar, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
		                                           LayoutParams.WRAP_CONTENT));

		panel = buildPanel();
		panel.setVisibility(GONE);
		addView(panel, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
		                                             LayoutParams.WRAP_CONTENT));
	}

	private View buildBar()
	{
		LinearLayout row = new LinearLayout(getContext());
		row.setOrientation(HORIZONTAL);
		row.setBackgroundColor(ContextCompat.getColor(getContext(), R.color.kbd_bar_bg));
		int barPad = px(R.dimen.kbd_bar_pad);
		row.setPadding(barPad, barPad, barPad, barPad);

		int[] codes = { R.integer.keycode_esc,          R.integer.keycode_tab,
			            R.integer.keycode_toggle_shift, R.integer.keycode_toggle_ctrl,
			            R.integer.keycode_toggle_win,   R.integer.keycode_toggle_alt,
			            R.integer.keycode_delete };
		String[] labels = { "Esc", "Tab", "Shift", "Ctrl", "Win", "Alt", "Del" };
		boolean[] mods = { false, false, true, true, true, true, false };

		for (int i = 0; i < codes.length; i++)
		{
			int keycode = code(codes[i]);
			// modifiers latch, so they get a lamp instead of auto-repeat
			View key =
			    codes[i] == R.integer.keycode_toggle_win
			        ? makeIconKey(keycode, R.drawable.ic_win_logo,
			                      getContext().getString(R.string.kbd_win_key), !mods[i], 1.0f)
			        : makeKey(keycode, labels[i], !mods[i], 1.0f);
			row.addView(mods[i] ? withIndicator(key, keycode) : key);
		}

		expandKey = makeKey(0, "▼", false, 1.0f);
		expandKey.setContentDescription(getContext().getString(R.string.menu_ext_keyboard));
		expandKey.setOnClickListener(v -> setExpanded(!expanded));
		tintAsChrome(expandKey);
		row.addView(expandKey);

		return row;
	}

	private View buildPanel()
	{
		LinearLayout p = new LinearLayout(getContext());
		p.setOrientation(VERTICAL);
		p.setBackgroundColor(ContextCompat.getColor(getContext(), R.color.kbd_panel_bg));

		FrameLayout pageHost = new FrameLayout(getContext());
		pages[PAGE_SPECIAL] = buildSpecialPage();
		pages[PAGE_NUM] = buildNumPage();
		for (View pg : pages)
			pageHost.addView(pg, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT,
			                                                  LayoutParams.WRAP_CONTENT));
		p.addView(pageHost, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
		                                                  LayoutParams.WRAP_CONTENT));

		selectPage(PAGE_SPECIAL);
		return p;
	}

	private View buildSpecialPage()
	{
		LinearLayout page = new LinearLayout(getContext());
		page.setOrientation(VERTICAL);
		page.addView(
		    gridRow(new int[] { R.integer.keycode_F1, R.integer.keycode_F2, R.integer.keycode_F3,
		                        R.integer.keycode_F4, R.integer.keycode_F5, R.integer.keycode_F6 },
		            new String[] { "F1", "F2", "F3", "F4", "F5", "F6" }));
		page.addView(gridRow(new int[] { R.integer.keycode_F7, R.integer.keycode_F8,
		                                 R.integer.keycode_F9, R.integer.keycode_F10,
		                                 R.integer.keycode_F11, R.integer.keycode_F12 },
		                     new String[] { "F7", "F8", "F9", "F10", "F11", "F12" }));
		page.addView(gridRow(new int[] { R.integer.keycode_insert, R.integer.keycode_home,
		                                 R.integer.keycode_print, R.integer.keycode_pgup,
		                                 R.integer.keycode_up, R.integer.keycode_pgdn },
		                     new String[] { "Ins", "Home", "PrtSc", "PgUp", "↑", "PgDn" }));
		// the page-switch key rides the normal keycode path: KeyboardMapper turns it into a
		// switchKeyboard() callback that lands back here as selectPage()
		LinearLayout row4 =
		    gridRow(new int[] { R.integer.keycode_numpad_keyboard, R.integer.keycode_end,
		                        R.integer.keycode_menu, R.integer.keycode_left,
		                        R.integer.keycode_down, R.integer.keycode_right },
		            new String[] { "⇆ 123", "End", "Menu", "←", "↓", "→" });
		tintAsChrome((TextView)row4.getChildAt(0));
		page.addView(row4);
		return page;
	}

	private View buildNumPage()
	{
		LinearLayout page = new LinearLayout(getContext());
		page.setOrientation(VERTICAL);
		page.addView(gridRow(
		    new int[] { R.integer.keycode_numpad_left_paren, R.integer.keycode_numpad_right_paren,
		                R.integer.keycode_numpad_7, R.integer.keycode_numpad_8,
		                R.integer.keycode_numpad_9, R.integer.keycode_numpad_subtract },
		    new String[] { "(", ")", "7", "8", "9", "−" }));
		page.addView(
		    gridRow(new int[] { R.integer.keycode_numpad_divide, R.integer.keycode_numpad_multiply,
		                        R.integer.keycode_numpad_4, R.integer.keycode_numpad_5,
		                        R.integer.keycode_numpad_6, R.integer.keycode_numpad_add },
		            new String[] { "/", "*", "4", "5", "6", "+" }));
		page.addView(gridRow(new int[] { R.integer.keycode_numpad_equals, R.integer.keycode_comma,
		                                 R.integer.keycode_numpad_1, R.integer.keycode_numpad_2,
		                                 R.integer.keycode_numpad_3, R.integer.keycode_backspace },
		                     new String[] { "=", ",", "1", "2", "3", "⌫" }));
		int gap = px(R.dimen.kbd_key_gap);
		LinearLayout row4 =
		    gridRow(new int[] { R.integer.keycode_specialkeys_keyboard, R.integer.keycode_numpad_0,
		                        R.integer.keycode_numpad_comma, R.integer.keycode_numpad_enter },
		            new String[] { "⇆ Fn", "0", ".", "↵" }, new float[] { 1.0f, 3.0f, 1.0f, 1.0f },
		            new int[] { 0, 4 * gap, 0, 0 });
		tintAsChrome((TextView)row4.getChildAt(0));
		page.addView(row4);
		return page;
	}

	// Keys that drive the keyboard rather than send input. Marking them by hue leaves the grey
	// scale to mean one thing only: how far a key is from its resting state.
	private void tintAsChrome(TextView k)
	{
		k.setBackgroundTintList(
		    ColorStateList.valueOf(ContextCompat.getColor(getContext(), R.color.kbd_key_chrome)));
		k.setTextColor(ContextCompat.getColor(getContext(), R.color.kbd_key_chrome_text));
	}

	private LinearLayout gridRow(int[] codeRes, String[] labels)
	{
		float[] weights = new float[codeRes.length];
		for (int i = 0; i < weights.length; i++)
			weights[i] = 1.0f;
		return gridRow(codeRes, labels, weights, new int[codeRes.length]);
	}

	// baseWidths[i] is the fixed part of a key's width; the weight distributes the remainder.
	private LinearLayout gridRow(int[] codeRes, String[] labels, float[] weights, int[] baseWidths)
	{
		LinearLayout row = new LinearLayout(getContext());
		row.setOrientation(HORIZONTAL);
		for (int i = 0; i < codeRes.length; i++)
		{
			TextView k = makeKey(code(codeRes[i]), labels[i], true, weights[i]);
			k.getLayoutParams().width = baseWidths[i];
			row.addView(k);
		}
		return row;
	}

	private TextView makeKey(int keycode, String label, boolean repeatable, float weight)
	{
		TextView k = new TextView(getContext());
		k.setText(label);
		k.setGravity(Gravity.CENTER);
		k.setTextColor(ContextCompat.getColor(getContext(), R.color.kbd_text));
		k.setTextSize(TypedValue.COMPLEX_UNIT_PX,
		              getResources().getDimension(R.dimen.kbd_key_text));
		styleKey(k, keycode, repeatable, weight);
		return k;
	}

	private ImageView makeIconKey(int keycode, int iconRes, String description, boolean repeatable,
	                              float weight)
	{
		ImageView k = new ImageView(getContext());
		k.setImageResource(iconRes);
		k.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
		k.setContentDescription(description);
		// tint like a label so an icon key matches the text keys beside it in either theme
		k.setImageTintList(
		    ColorStateList.valueOf(ContextCompat.getColor(getContext(), R.color.kbd_text)));
		styleKey(k, keycode, repeatable, weight);
		return k;
	}

	private void styleKey(View k, int keycode, boolean repeatable, float weight)
	{
		k.setBackgroundResource(R.drawable.bg_kbd_key);
		k.setBackgroundTintList(
		    ContextCompat.getColorStateList(getContext(), R.color.kbd_key_tint));
		k.setClickable(true);
		k.setFocusable(true);
		LinearLayout.LayoutParams lp =
		    new LinearLayout.LayoutParams(0, px(R.dimen.kbd_key_height), weight);
		int gap = px(R.dimen.kbd_key_gap);
		lp.setMargins(gap, gap, gap, gap);
		k.setLayoutParams(lp);
		k.setOnClickListener(v -> {
			if (listener != null)
				listener.onKey(keycode);
		});
		if (repeatable)
			k.setOnLongClickListener(v -> {
				startRepeat(v, keycode);
				return true;
			});
	}

	// Wraps a latching modifier so it carries an indicator lamp in its leading top corner.
	private View withIndicator(View key, int keycode)
	{
		FrameLayout holder = new FrameLayout(getContext());
		holder.setLayoutParams(key.getLayoutParams());
		key.setLayoutParams(
		    new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
		key.setOnLongClickListener(v -> {
			v.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
			if (listener != null)
				listener.onKeyLock(keycode);
			return true;
		});
		holder.addView(key);

		View led = new View(getContext());
		led.setBackgroundResource(R.drawable.bg_kbd_led);
		int size = px(R.dimen.kbd_led_size);
		int inset = px(R.dimen.kbd_led_inset);
		FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(size, size);
		lp.gravity = Gravity.TOP | Gravity.START;
		lp.setMargins(inset, inset, 0, 0);
		holder.addView(led, lp);

		modifierKeys.put(keycode, new ModifierKey(key, led));
		return holder;
	}

	// The pressed state terminates the loop, so a cancelled gesture or a detached view stops it.
	private void startRepeat(View k, int keycode)
	{
		repeatHandler.removeCallbacksAndMessages(null);
		repeatHandler.post(new Runnable() {
			@Override public void run()
			{
				if (!k.isPressed() || !isAttachedToWindow())
					return;
				if (listener != null)
					listener.onKey(keycode);
				repeatHandler.postDelayed(this, ViewConfiguration.getKeyRepeatDelay());
			}
		});
	}

	public void selectPage(int index)
	{
		currentPage = index;
		for (int i = 0; i < pages.length; i++)
			pages[i].setVisibility(i == index ? VISIBLE : GONE);
	}

	public void setInsets(int left, int right, int bottom)
	{
		this.insetLeft = left;
		this.insetRight = right;
		this.insetBottom = bottom;
		applyInsets();
	}

	private void applyInsets()
	{
		int barPad = px(R.dimen.kbd_bar_pad);
		if (bar != null)
		{
			// collapsed, the bar is the bottom-most child and has to clear the nav bar itself
			int barBottom = expanded ? barPad : barPad + insetBottom;
			bar.setPadding(insetLeft + barPad, barPad, insetRight + barPad, barBottom);
		}
		if (panel != null)
			panel.setPadding(insetLeft, 0, insetRight, insetBottom);
	}

	public void setExpanded(boolean expand)
	{
		setExpanded(expand, true);
	}

	public void setExpanded(boolean expand, boolean notify)
	{
		if (expanded == expand)
			return;
		expanded = expand;
		panel.setVisibility(expand ? VISIBLE : GONE);
		expandKey.setText(expand ? "▲" : "▼");
		applyInsets();
		if (notify && listener != null)
			listener.onExpandedChanged(expand);
	}

	public boolean isExpanded()
	{
		return expanded;
	}

	// Re-reads every modifier's state from the listener and repaints key and lamp.
	public void refreshModifiers()
	{
		if (listener == null)
			return;
		for (int i = 0; i < modifierKeys.size(); i++)
		{
			int state = listener.getModifierState(modifierKeys.keyAt(i));
			modifierKeys.valueAt(i).setState(state == KeyboardMapper.KEYSTATE_ON,
			                                 state == KeyboardMapper.KEYSTATE_LOCKED);
		}
	}

	@Override protected void onConfigurationChanged(android.content.res.Configuration newConfig)
	{
		super.onConfigurationChanged(newConfig);

		// the key dimensions are orientation dependent, so rebuild and restore
		int page = currentPage;
		boolean wasExpanded = expanded;

		removeAllViews();
		modifierKeys.clear();
		build();

		// build() starts out collapsed: without clearing the flag setExpanded() sees no change
		// and leaves the freshly built panel hidden
		expanded = false;
		selectPage(page);
		setExpanded(wasExpanded, false);
		applyInsets();
		refreshModifiers();
	}

	@Override protected void onDetachedFromWindow()
	{
		repeatHandler.removeCallbacksAndMessages(null);
		super.onDetachedFromWindow();
	}
}
