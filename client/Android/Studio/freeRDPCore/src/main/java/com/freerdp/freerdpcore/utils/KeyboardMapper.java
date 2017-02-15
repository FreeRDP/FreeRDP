/*
   Android Keyboard Mapping

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/


package com.freerdp.freerdpcore.utils;

import com.freerdp.freerdpcore.R;

import android.content.Context;
import android.view.KeyEvent;

public class KeyboardMapper
{
	public static final int KEYBOARD_TYPE_FUNCTIONKEYS = 1;
	public static final int KEYBOARD_TYPE_NUMPAD = 2;
	public static final int KEYBOARD_TYPE_CURSOR = 3;

	// defines key states for modifier keys - locked means on and no auto-release if an other key is pressed
	public static final int KEYSTATE_ON = 1;
	public static final int KEYSTATE_LOCKED = 2;
	public static final int KEYSTATE_OFF = 3;

	// interface that gets called for input handling
	public interface KeyProcessingListener {
		abstract void processVirtualKey(int virtualKeyCode, boolean down);
		abstract void processUnicodeKey(int unicodeKey);
		abstract void switchKeyboard(int keyboardType);
		abstract void modifiersChanged();
	}

	private KeyProcessingListener listener = null;

	private static int[] keymapAndroid;
	private static int[] keymapExt;
	private static boolean initialized = false;

	final static int VK_LBUTTON = 0x01;
	final static int VK_RBUTTON = 0x02;
	final static int VK_CANCEL = 0x03;
	final static int VK_MBUTTON = 0x04;
	final static int VK_XBUTTON1 = 0x05;
	final static int VK_XBUTTON2 = 0x06;
	final static int VK_BACK = 0x08;
	final static int VK_TAB	 = 0x09;
	final static int VK_CLEAR = 0x0C;
	final static int VK_RETURN = 0x0D;
	final static int VK_SHIFT = 0x10;
	final static int VK_CONTROL = 0x11;
	final static int VK_MENU = 0x12;
	final static int VK_PAUSE = 0x13;
	final static int VK_CAPITAL = 0x14;
	final static int VK_KANA = 0x15;
	final static int VK_HANGUEL = 0x15;
	final static int VK_HANGUL = 0x15;
	final static int VK_JUNJA = 0x17;
	final static int VK_FINAL = 0x18;
	final static int VK_HANJA = 0x19;
	final static int VK_KANJI = 0x19;
	final static int VK_ESCAPE = 0x1B;
	final static int VK_CONVERT = 0x1C;
	final static int VK_NONCONVERT = 0x1D;
	final static int VK_ACCEPT = 0x1E;
	final static int VK_MODECHANGE = 0x1F;
	final static int VK_SPACE = 0x20;
	final static int VK_PRIOR = 0x21;
	final static int VK_NEXT = 0x22;
	final static int VK_END	 = 0x23;
	final static int VK_HOME = 0x24;
	final static int VK_LEFT = 0x25;
	final static int VK_UP = 0x26;
	final static int VK_RIGHT = 0x27;
	final static int VK_DOWN = 0x28;
	final static int VK_SELECT = 0x29;
	final static int VK_PRINT = 0x2A;
	final static int VK_EXECUTE = 0x2B;
	final static int VK_SNAPSHOT = 0x2C;
	final static int VK_INSERT = 0x2D;
	final static int VK_DELETE = 0x2E;
	final static int VK_HELP = 0x2F;
	final static int VK_KEY_0 = 0x30;
	final static int VK_KEY_1 = 0x31;
	final static int VK_KEY_2 = 0x32;
	final static int VK_KEY_3 = 0x33;
	final static int VK_KEY_4 = 0x34;
	final static int VK_KEY_5 = 0x35;
	final static int VK_KEY_6 = 0x36;
	final static int VK_KEY_7 = 0x37;
	final static int VK_KEY_8 = 0x38;
	final static int VK_KEY_9 = 0x39;
	final static int VK_KEY_A = 0x41;
	final static int VK_KEY_B = 0x42;
	final static int VK_KEY_C = 0x43;
	final static int VK_KEY_D = 0x44;
	final static int VK_KEY_E = 0x45;
	final static int VK_KEY_F = 0x46;
	final static int VK_KEY_G = 0x47;
	final static int VK_KEY_H = 0x48;
	final static int VK_KEY_I = 0x49;
	final static int VK_KEY_J = 0x4A;
	final static int VK_KEY_K = 0x4B;
	final static int VK_KEY_L = 0x4C;
	final static int VK_KEY_M = 0x4D;
	final static int VK_KEY_N = 0x4E;
	final static int VK_KEY_O = 0x4F;
	final static int VK_KEY_P = 0x50;
	final static int VK_KEY_Q = 0x51;
	final static int VK_KEY_R = 0x52;
	final static int VK_KEY_S = 0x53;
	final static int VK_KEY_T = 0x54;
	final static int VK_KEY_U = 0x55;
	final static int VK_KEY_V = 0x56;
	final static int VK_KEY_W = 0x57;
	final static int VK_KEY_X = 0x58;
	final static int VK_KEY_Y = 0x59;
	final static int VK_KEY_Z = 0x5A;
	final static int VK_LWIN = 0x5B;
	final static int VK_RWIN = 0x5C;
	final static int VK_APPS = 0x5D;
	final static int VK_SLEEP = 0x5F;
	final static int VK_NUMPAD0	= 0x60;
	final static int VK_NUMPAD1	= 0x61;
	final static int VK_NUMPAD2	= 0x62;
	final static int VK_NUMPAD3	= 0x63;
	final static int VK_NUMPAD4	= 0x64;
	final static int VK_NUMPAD5	= 0x65;
	final static int VK_NUMPAD6	= 0x66;
	final static int VK_NUMPAD7	= 0x67;
	final static int VK_NUMPAD8	= 0x68;
	final static int VK_NUMPAD9	= 0x69;
	final static int VK_MULTIPLY = 0x6A;
	final static int VK_ADD = 0x6B;
	final static int VK_SEPARATOR = 0x6C;
	final static int VK_SUBTRACT = 0x6D;
	final static int VK_DECIMAL = 0x6E;
	final static int VK_DIVIDE = 0x6F;
	final static int VK_F1 = 0x70;
	final static int VK_F2 = 0x71;
	final static int VK_F3 = 0x72;
	final static int VK_F4 = 0x73;
	final static int VK_F5 = 0x74;
	final static int VK_F6 = 0x75;
	final static int VK_F7 = 0x76;
	final static int VK_F8 = 0x77;
	final static int VK_F9 = 0x78;
	final static int VK_F10 = 0x79;
	final static int VK_F11 = 0x7A;
	final static int VK_F12 = 0x7B;
	final static int VK_F13 = 0x7C;
	final static int VK_F14 = 0x7D;
	final static int VK_F15 = 0x7E;
	final static int VK_F16 = 0x7F;
	final static int VK_F17 = 0x80;
	final static int VK_F18 = 0x81;
	final static int VK_F19 = 0x82;
	final static int VK_F20 = 0x83;
	final static int VK_F21 = 0x84;
	final static int VK_F22 = 0x85;
	final static int VK_F23 = 0x86;
	final static int VK_F24 = 0x87;
	final static int VK_NUMLOCK = 0x90;
	final static int VK_SCROLL = 0x91;
	final static int VK_LSHIFT = 0xA0;
	final static int VK_RSHIFT = 0xA1;
	final static int VK_LCONTROL = 0xA2;
	final static int VK_RCONTROL = 0xA3;
	final static int VK_LMENU = 0xA4;
	final static int VK_RMENU = 0xA5;
	final static int VK_BROWSER_BACK = 0xA6;
	final static int VK_BROWSER_FORWARD = 0xA7;
	final static int VK_BROWSER_REFRESH = 0xA8;
	final static int VK_BROWSER_STOP = 0xA9;
	final static int VK_BROWSER_SEARCH = 0xAA;
	final static int VK_BROWSER_FAVORITES = 0xAB;
	final static int VK_BROWSER_HOME = 0xAC;
	final static int VK_VOLUME_MUTE = 0xAD;
	final static int VK_VOLUME_DOWN = 0xAE;
	final static int VK_VOLUME_UP = 0xAF;
	final static int VK_MEDIA_NEXT_TRACK = 0xB0;
	final static int VK_MEDIA_PREV_TRACK = 0xB1;
	final static int VK_MEDIA_STOP = 0xB2;
	final static int VK_MEDIA_PLAY_PAUSE = 0xB3;
	final static int VK_LAUNCH_MAIL = 0xB4;
	final static int VK_LAUNCH_MEDIA_SELECT = 0xB5;
	final static int VK_LAUNCH_APP1 = 0xB6;
	final static int VK_LAUNCH_APP2 = 0xB7;
	final static int VK_OEM_1 = 0xBA;
	final static int VK_OEM_PLUS = 0xBB;
	final static int VK_OEM_COMMA = 0xBC;
	final static int VK_OEM_MINUS = 0xBD;
	final static int VK_OEM_PERIOD = 0xBE;
	final static int VK_OEM_2 = 0xBF;
	final static int VK_OEM_3 = 0xC0;
	final static int VK_ABNT_C1 = 0xC1;
	final static int VK_ABNT_C2 = 0xC2;
	final static int VK_OEM_4 = 0xDB;
	final static int VK_OEM_5 = 0xDC;
	final static int VK_OEM_6 = 0xDD;
	final static int VK_OEM_7 = 0xDE;
	final static int VK_OEM_8 = 0xDF;
	final static int VK_OEM_102 = 0xE2;
	final static int VK_PROCESSKEY = 0xE5;
	final static int VK_PACKET = 0xE7;
	final static int VK_ATTN = 0xF6;
	final static int VK_CRSEL = 0xF7;
	final static int VK_EXSEL = 0xF8;
	final static int VK_EREOF = 0xF9;
	final static int VK_PLAY = 0xFA;
	final static int VK_ZOOM = 0xFB;
	final static int VK_NONAME = 0xFC;
	final static int VK_PA1	= 0xFD;
	final static int VK_OEM_CLEAR = 0xFE;
	final static int VK_UNICODE = 0x80000000;
	final static int VK_EXT_KEY = 0x00000100;

	// key codes to switch between custom keyboard 
	private final static int EXTKEY_KBFUNCTIONKEYS = 0x1100; 
	private final static int EXTKEY_KBNUMPAD = 0x1101; 
	private final static int EXTKEY_KBCURSOR = 0x1102; 

	// this flag indicates if we got a VK or a unicode character in our translation map 
	private static final int KEY_FLAG_UNICODE = 0x80000000;

	// this flag indicates if the key is a toggle key (remains down when pressed and goes up if pressed again)
	private static final int KEY_FLAG_TOGGLE = 0x40000000;

	private boolean shiftPressed = false;
	private boolean ctrlPressed = false;
	private boolean altPressed = false;
	private boolean winPressed = false;
	
    private long lastModifierTime;
    private int lastModifierKeyCode = -1;

    private boolean isShiftLocked = false;
    private boolean isCtrlLocked = false;
    private boolean isAltLocked = false;
    private boolean isWinLocked = false;
    
	public void init(Context context)
	{
		if(initialized == true)
			return;

		keymapAndroid = new int[256];

		keymapAndroid[KeyEvent.KEYCODE_0] = VK_KEY_0;
		keymapAndroid[KeyEvent.KEYCODE_1] = VK_KEY_1;
		keymapAndroid[KeyEvent.KEYCODE_2] = VK_KEY_2;
		keymapAndroid[KeyEvent.KEYCODE_3] = VK_KEY_3;
		keymapAndroid[KeyEvent.KEYCODE_4] = VK_KEY_4;
		keymapAndroid[KeyEvent.KEYCODE_5] = VK_KEY_5;
		keymapAndroid[KeyEvent.KEYCODE_6] = VK_KEY_6;
		keymapAndroid[KeyEvent.KEYCODE_7] = VK_KEY_7;
		keymapAndroid[KeyEvent.KEYCODE_8] = VK_KEY_8;
		keymapAndroid[KeyEvent.KEYCODE_9] = VK_KEY_9;

		keymapAndroid[KeyEvent.KEYCODE_A] = VK_KEY_A;
		keymapAndroid[KeyEvent.KEYCODE_B] = VK_KEY_B;
		keymapAndroid[KeyEvent.KEYCODE_C] = VK_KEY_C;
		keymapAndroid[KeyEvent.KEYCODE_D] = VK_KEY_D;
		keymapAndroid[KeyEvent.KEYCODE_E] = VK_KEY_E;
		keymapAndroid[KeyEvent.KEYCODE_F] = VK_KEY_F;
		keymapAndroid[KeyEvent.KEYCODE_G] = VK_KEY_G;
		keymapAndroid[KeyEvent.KEYCODE_H] = VK_KEY_H;
		keymapAndroid[KeyEvent.KEYCODE_I] = VK_KEY_I;
		keymapAndroid[KeyEvent.KEYCODE_J] = VK_KEY_J;
		keymapAndroid[KeyEvent.KEYCODE_K] = VK_KEY_K;
		keymapAndroid[KeyEvent.KEYCODE_L] = VK_KEY_L;
		keymapAndroid[KeyEvent.KEYCODE_M] = VK_KEY_M;
		keymapAndroid[KeyEvent.KEYCODE_N] = VK_KEY_N;
		keymapAndroid[KeyEvent.KEYCODE_O] = VK_KEY_O;
		keymapAndroid[KeyEvent.KEYCODE_P] = VK_KEY_P;
		keymapAndroid[KeyEvent.KEYCODE_Q] = VK_KEY_Q;
		keymapAndroid[KeyEvent.KEYCODE_R] = VK_KEY_R;
		keymapAndroid[KeyEvent.KEYCODE_S] = VK_KEY_S;
		keymapAndroid[KeyEvent.KEYCODE_T] = VK_KEY_T;
		keymapAndroid[KeyEvent.KEYCODE_U] = VK_KEY_U;
		keymapAndroid[KeyEvent.KEYCODE_V] = VK_KEY_V;
		keymapAndroid[KeyEvent.KEYCODE_W] = VK_KEY_W;
		keymapAndroid[KeyEvent.KEYCODE_X] = VK_KEY_X;
		keymapAndroid[KeyEvent.KEYCODE_Y] = VK_KEY_Y;
		keymapAndroid[KeyEvent.KEYCODE_Z] = VK_KEY_Z;

		keymapAndroid[KeyEvent.KEYCODE_DEL] = VK_BACK;
		keymapAndroid[KeyEvent.KEYCODE_ENTER] = VK_RETURN;
		keymapAndroid[KeyEvent.KEYCODE_SPACE] = VK_SPACE;
//		keymapAndroid[KeyEvent.KEYCODE_SHIFT_LEFT] = VK_LSHIFT;
//		keymapAndroid[KeyEvent.KEYCODE_SHIFT_RIGHT] = VK_RSHIFT;

//		keymapAndroid[KeyEvent.KEYCODE_DPAD_DOWN] = VK_DOWN;
//		keymapAndroid[KeyEvent.KEYCODE_DPAD_LEFT] = VK_LEFT;
//		keymapAndroid[KeyEvent.KEYCODE_DPAD_RIGHT] = VK_RIGHT;
//		keymapAndroid[KeyEvent.KEYCODE_DPAD_UP] = VK_UP;

//		keymapAndroid[KeyEvent.KEYCODE_COMMA] = VK_OEM_COMMA;
//		keymapAndroid[KeyEvent.KEYCODE_PERIOD] = VK_OEM_PERIOD;
//		keymapAndroid[KeyEvent.KEYCODE_MINUS] = VK_OEM_MINUS;
//		keymapAndroid[KeyEvent.KEYCODE_PLUS] = VK_OEM_PLUS;

//		keymapAndroid[KeyEvent.KEYCODE_ALT_LEFT] = VK_LMENU;
//		keymapAndroid[KeyEvent.KEYCODE_ALT_RIGHT] = VK_RMENU;
		
//		keymapAndroid[KeyEvent.KEYCODE_AT] = (KEY_FLAG_UNICODE | 64);
//		keymapAndroid[KeyEvent.KEYCODE_APOSTROPHE] = (KEY_FLAG_UNICODE | 39);
//		keymapAndroid[KeyEvent.KEYCODE_BACKSLASH] = (KEY_FLAG_UNICODE | 92);
//		keymapAndroid[KeyEvent.KEYCODE_COMMA] = (KEY_FLAG_UNICODE | 44);
//		keymapAndroid[KeyEvent.KEYCODE_EQUALS] = (KEY_FLAG_UNICODE | 61);
//		keymapAndroid[KeyEvent.KEYCODE_GRAVE] = (KEY_FLAG_UNICODE | 96);		
//		keymapAndroid[KeyEvent.KEYCODE_LEFT_BRACKET] = (KEY_FLAG_UNICODE | 91);
//		keymapAndroid[KeyEvent.KEYCODE_RIGHT_BRACKET] = (KEY_FLAG_UNICODE | 93);
//		keymapAndroid[KeyEvent.KEYCODE_MINUS] = (KEY_FLAG_UNICODE | 45);
//		keymapAndroid[KeyEvent.KEYCODE_PERIOD] = (KEY_FLAG_UNICODE | 46);
//		keymapAndroid[KeyEvent.KEYCODE_PLUS] = (KEY_FLAG_UNICODE | 43);
//		keymapAndroid[KeyEvent.KEYCODE_POUND] = (KEY_FLAG_UNICODE | 35);
//		keymapAndroid[KeyEvent.KEYCODE_SEMICOLON] = (KEY_FLAG_UNICODE | 59);
//		keymapAndroid[KeyEvent.KEYCODE_SLASH] = (KEY_FLAG_UNICODE | 47);
//		keymapAndroid[KeyEvent.KEYCODE_STAR] = (KEY_FLAG_UNICODE | 42);		
		
		// special keys mapping
		keymapExt = new int[256];
		keymapExt[context.getResources().getInteger(R.integer.keycode_F1)] = VK_F1;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F2)] = VK_F2;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F3)] = VK_F3;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F4)] = VK_F4;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F5)] = VK_F5;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F6)] = VK_F6;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F7)] = VK_F7;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F8)] = VK_F8;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F9)] = VK_F9;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F10)] = VK_F10;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F11)] = VK_F11;
		keymapExt[context.getResources().getInteger(R.integer.keycode_F12)] = VK_F12;
		keymapExt[context.getResources().getInteger(R.integer.keycode_tab)] = VK_TAB;
		keymapExt[context.getResources().getInteger(R.integer.keycode_print)] = VK_PRINT;
		keymapExt[context.getResources().getInteger(R.integer.keycode_insert)] = VK_INSERT | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_delete)] = VK_DELETE | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_home)] = VK_HOME | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_end)] = VK_END | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_pgup)] = VK_PRIOR | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_pgdn)] = VK_NEXT | VK_EXT_KEY;
	
		// numpad mapping
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_0)] = VK_NUMPAD0;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_1)] = VK_NUMPAD1;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_2)] = VK_NUMPAD2;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_3)] = VK_NUMPAD3;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_4)] = VK_NUMPAD4;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_5)] = VK_NUMPAD5;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_6)] = VK_NUMPAD6;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_7)] = VK_NUMPAD7;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_8)] = VK_NUMPAD8;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_9)] = VK_NUMPAD9;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_numlock)] = VK_NUMLOCK;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_add)] = VK_ADD;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_comma)] = VK_DECIMAL;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_divide)] = VK_DIVIDE | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_enter)] = VK_RETURN | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_multiply)] = VK_MULTIPLY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_subtract)] = VK_SUBTRACT;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_equals)] = (KEY_FLAG_UNICODE | 61);
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_left_paren)] = (KEY_FLAG_UNICODE | 40);
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_right_paren)] = (KEY_FLAG_UNICODE | 41);

		// cursor key codes
		keymapExt[context.getResources().getInteger(R.integer.keycode_up)] = VK_UP | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_down)] = VK_DOWN | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_left)] = VK_LEFT | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_right)] = VK_RIGHT | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_enter)] = VK_RETURN | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_backspace)] = VK_BACK;

		// shared keys
		keymapExt[context.getResources().getInteger(R.integer.keycode_win)] = VK_LWIN | VK_EXT_KEY;
		keymapExt[context.getResources().getInteger(R.integer.keycode_menu)] = VK_APPS | VK_EXT_KEY;	
		keymapExt[context.getResources().getInteger(R.integer.keycode_esc)] = VK_ESCAPE;
		
/*		keymapExt[context.getResources().getInteger(R.integer.keycode_modifier_ctrl)] = VK_LCONTROL;		
		keymapExt[context.getResources().getInteger(R.integer.keycode_modifier_alt)] = VK_LMENU;
		keymapExt[context.getResources().getInteger(R.integer.keycode_modifier_shift)] = VK_LSHIFT;		
*/
		// get custom keyboard key codes
		keymapExt[context.getResources().getInteger(R.integer.keycode_specialkeys_keyboard)] = EXTKEY_KBFUNCTIONKEYS;
		keymapExt[context.getResources().getInteger(R.integer.keycode_numpad_keyboard)] = EXTKEY_KBNUMPAD;
		keymapExt[context.getResources().getInteger(R.integer.keycode_cursor_keyboard)] = EXTKEY_KBCURSOR;

		keymapExt[context.getResources().getInteger(R.integer.keycode_toggle_shift)] = (KEY_FLAG_TOGGLE | VK_LSHIFT);		
		keymapExt[context.getResources().getInteger(R.integer.keycode_toggle_ctrl)] = (KEY_FLAG_TOGGLE | VK_LCONTROL);		
		keymapExt[context.getResources().getInteger(R.integer.keycode_toggle_alt)] = (KEY_FLAG_TOGGLE | VK_LMENU);
		keymapExt[context.getResources().getInteger(R.integer.keycode_toggle_win)] = (KEY_FLAG_TOGGLE | VK_LWIN);	

		initialized = true;
	}

	public void reset(KeyProcessingListener listener) {
		shiftPressed = false;
		ctrlPressed = false;
		altPressed = false;
		winPressed = false;
		setKeyProcessingListener(listener);
	}

	public void setKeyProcessingListener(KeyProcessingListener listener)  {
		this.listener = listener;
	}

	public boolean processAndroidKeyEvent(KeyEvent event) {
		switch(event.getAction())
		{
			// we only process down events
			case KeyEvent.ACTION_UP:
			{
				return false;
			}			
			
			case KeyEvent.ACTION_DOWN:
			{	
				boolean modifierActive = isModifierPressed();				
				// if a modifier is pressed we will send a VK event (if possible) so that key combinations will be 
				// recognized correctly. Otherwise we will send the unicode key. At the end we will reset all modifiers
				// and notifiy our listener.
				int vkcode = getVirtualKeyCode(event.getKeyCode());
				if((vkcode & KEY_FLAG_UNICODE) != 0)
					listener.processUnicodeKey(vkcode & (~KEY_FLAG_UNICODE));
				// if we got a valid vkcode send it - except for letters/numbers if a modifier is active
				else if (vkcode > 0 && (event.getMetaState() & (KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON | KeyEvent.META_SYM_ON)) == 0)
				{
					listener.processVirtualKey(vkcode, true);
					listener.processVirtualKey(vkcode, false);
				}
				else if(event.isShiftPressed() && vkcode != 0)
				{
					listener.processVirtualKey(VK_LSHIFT, true);
					listener.processVirtualKey(vkcode, true);
					listener.processVirtualKey(vkcode, false);										
					listener.processVirtualKey(VK_LSHIFT, false);
				}
				else if(event.getUnicodeChar() != 0) 
					listener.processUnicodeKey(event.getUnicodeChar());
				else
					return false;
				 			
				// reset any pending toggle states if a modifier was pressed
				if(modifierActive)
					resetModifierKeysAfterInput(false);
				return true;
			}

			case KeyEvent.ACTION_MULTIPLE:
			{
				String str = event.getCharacters();
				for(int i = 0; i < str.length(); i++)
					listener.processUnicodeKey(str.charAt(i));
				return true;
			}
			
			default:
				break;				
		}
		return false;
	}

	public void processCustomKeyEvent(int keycode) {
		int extCode = getExtendedKeyCode(keycode);
		if(extCode == 0)
			return;
		
		// toggle button pressed?
		if((extCode & KEY_FLAG_TOGGLE) != 0)
		{
			processToggleButton(extCode & (~KEY_FLAG_TOGGLE));
			return;
		}
		
		// keyboard switch button pressed?
		if(extCode == EXTKEY_KBFUNCTIONKEYS || extCode == EXTKEY_KBNUMPAD || extCode == EXTKEY_KBCURSOR)
		{
			switchKeyboard(extCode);
			return;
		}
		
		// nope - see if we got a unicode or vk
		if((extCode & KEY_FLAG_UNICODE) != 0)
			listener.processUnicodeKey(extCode & (~KEY_FLAG_UNICODE));
		else
		{
			listener.processVirtualKey(extCode, true);			
			listener.processVirtualKey(extCode, false);			
		}
		
		resetModifierKeysAfterInput(false);
	}
	
	public void sendAltF4()
	{
		listener.processVirtualKey(VK_LMENU, true);			
		listener.processVirtualKey(VK_F4, true);			
		listener.processVirtualKey(VK_F4, false);			
		listener.processVirtualKey(VK_LMENU, false);					
	}

	private boolean isModifierPressed() {
		return (shiftPressed || ctrlPressed || altPressed || winPressed);
	}
	
	public int getModifierState(int keycode) {
		int modifierCode = getExtendedKeyCode(keycode);
		
		// check and get real modifier keycode
		if((modifierCode & KEY_FLAG_TOGGLE) == 0)
			return -1;
		modifierCode = modifierCode & (~KEY_FLAG_TOGGLE);
		
		switch(modifierCode)
		{
			case VK_LSHIFT:
			{
				return (shiftPressed ? (isShiftLocked ? KEYSTATE_LOCKED : KEYSTATE_ON) : KEYSTATE_OFF);
			}
			case VK_LCONTROL:
			{
				return (ctrlPressed ? (isCtrlLocked ? KEYSTATE_LOCKED : KEYSTATE_ON) : KEYSTATE_OFF);
			}
			case VK_LMENU:
			{
				return (altPressed ? (isAltLocked ? KEYSTATE_LOCKED : KEYSTATE_ON) : KEYSTATE_OFF);
			}
			case VK_LWIN:
			{
				return (winPressed ? (isWinLocked ? KEYSTATE_LOCKED : KEYSTATE_ON) : KEYSTATE_OFF);
			}
		}
		
		return -1;
	}
	
	private int getVirtualKeyCode(int keycode) {
		if(keycode >= 0 && keycode <= 0xFF)
			return keymapAndroid[keycode];
		return 0;
	}
	
	private int getExtendedKeyCode(int keycode) {
		if(keycode >= 0 && keycode <= 0xFF)
			return keymapExt[keycode];
		return 0;		
	}

	private void processToggleButton(int keycode) {
		switch(keycode)
		{
			case VK_LSHIFT:
			{
				if(!checkToggleModifierLock(VK_LSHIFT))
				{
					isShiftLocked = false;
					shiftPressed = !shiftPressed;
					listener.processVirtualKey(VK_LSHIFT, shiftPressed);
				}
				else
					isShiftLocked = true;
				break;
			}
			case VK_LCONTROL:
			{
				if(!checkToggleModifierLock(VK_LCONTROL))
				{
					isCtrlLocked = false;
					ctrlPressed = !ctrlPressed;
					listener.processVirtualKey(VK_LCONTROL, ctrlPressed);
				}
				else
					isCtrlLocked = true;
				break;
			}
			case VK_LMENU:
			{
				if(!checkToggleModifierLock(VK_LMENU))
				{
					isAltLocked = false;
					altPressed = !altPressed;
					listener.processVirtualKey(VK_LMENU, altPressed);
				}
				else
					isAltLocked = true;
				break;
			}
			case VK_LWIN:
			{
				if(!checkToggleModifierLock(VK_LWIN))
				{
					isWinLocked = false;
					winPressed = !winPressed;
					listener.processVirtualKey(VK_LWIN | VK_EXT_KEY, winPressed);
				}
				else
					isWinLocked = true;					
				break;
			}
		}
		listener.modifiersChanged();
	}
	
	public void clearlAllModifiers() 
	{
		resetModifierKeysAfterInput(true);
	}
	
	private void resetModifierKeysAfterInput(boolean force) {
		if(shiftPressed && (!isShiftLocked || force))
		{
			listener.processVirtualKey(VK_LSHIFT, false);
			shiftPressed = false;
		}
		if(ctrlPressed && (!isCtrlLocked || force))
		{
			listener.processVirtualKey(VK_LCONTROL, false);
			ctrlPressed = false;
		}
		if(altPressed && (!isAltLocked || force))
		{
			listener.processVirtualKey(VK_LMENU, false);
			altPressed = false;
		}
		if(winPressed && (!isWinLocked || force))
		{
			listener.processVirtualKey(VK_LWIN | VK_EXT_KEY, false);
			winPressed = false;
		}

		if(listener != null)
			listener.modifiersChanged();
	}
	
	private void switchKeyboard(int keycode) {
		switch(keycode)
		{
			case EXTKEY_KBFUNCTIONKEYS:
			{
				listener.switchKeyboard(KEYBOARD_TYPE_FUNCTIONKEYS);
				break;
			}
		
			case EXTKEY_KBNUMPAD:
			{
				listener.switchKeyboard(KEYBOARD_TYPE_NUMPAD);
				break;
			}

			case EXTKEY_KBCURSOR:
			{
				listener.switchKeyboard(KEYBOARD_TYPE_CURSOR);
				break;
			}
			
			default:
				break;
		}
	}
	
    private boolean checkToggleModifierLock(int keycode) {
        long now = System.currentTimeMillis();
        
        // was the same modifier hit?
        if(lastModifierKeyCode != keycode)
        {
        	lastModifierKeyCode = keycode;
        	lastModifierTime = now;
        	return false;        	        
        }
        
        // within a certain time interval?
        if(lastModifierTime + 800 > now) 
        {
            lastModifierTime = 0;
            return true;
        }
        else
        {
        	lastModifierTime = now;
        	return false;
        }
    }
	
}

