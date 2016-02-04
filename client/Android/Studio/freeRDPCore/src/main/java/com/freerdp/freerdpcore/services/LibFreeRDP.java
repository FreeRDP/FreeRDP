/*
   Android FreeRDP JNI Wrapper

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;


import com.freerdp.freerdpcore.application.GlobalApp;
import com.freerdp.freerdpcore.application.SessionState;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ManualBookmark;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

public class LibFreeRDP {
    private static final String TAG = "LibFreeRDP";

    static {
        final String[] libraries = {
                "openh264", "crypto", "ssl", "jpeg", "winpr",
                "freerdp", "freerdp-client", "freerdp-android"};
        final String LD_PATH = System.getProperty("java.library.path");

        for (String lib : libraries) {
            try {
                Log.v(TAG, "Trying to load library " + lib + " from LD_PATH: " + LD_PATH);
                System.loadLibrary(lib);
            } catch (UnsatisfiedLinkError e) {
                Log.e(TAG, "Failed to load library " + lib + ": " + e.toString());
            }
        }
    }

    private static native String freerdp_get_jni_version();

    private static native String freerdp_get_version();

    private static native String freerdp_get_build_date();

    private static native String freerdp_get_build_revision();

    private static native String freerdp_get_build_config();

    private static native int freerdp_new(Context context);

    private static native void freerdp_free(int inst);

    private static native boolean freerdp_parse_arguments(int inst, String[] args);

    private static native boolean freerdp_connect(int inst);

    private static native boolean freerdp_disconnect(int inst);

    private static native boolean freerdp_update_graphics(int inst,
                                                          Bitmap bitmap, int x, int y, int width, int height);

    private static native boolean freerdp_send_cursor_event(int inst, int x, int y, int flags);

    private static native boolean freerdp_send_key_event(int inst, int keycode, boolean down);

    private static native boolean freerdp_send_unicodekey_event(int inst, int keycode);

    private static native boolean freerdp_send_clipboard_data(int inst, String data);

    public static interface EventListener {
        void OnPreConnect(int instance);

        void OnConnectionSuccess(int instance);

        void OnConnectionFailure(int instance);

        void OnDisconnecting(int instance);

        void OnDisconnected(int instance);
    }

    public static interface UIEventListener {
        void OnSettingsChanged(int width, int height, int bpp);

        boolean OnAuthenticate(StringBuilder username, StringBuilder domain, StringBuilder password);

        boolean OnVerifiyCertificate(String subject, String issuer, String fingerprint);

        void OnGraphicsUpdate(int x, int y, int width, int height);

        void OnGraphicsResize(int width, int height, int bpp);

        void OnRemoteClipboardChanged(String data);
    }

    private static EventListener listener;

    public static void setEventListener(EventListener l) {
        listener = l;
    }

    public static int newInstance(Context context) {
        return freerdp_new(context);
    }

    public static void freeInstance(int inst) {
        freerdp_free(inst);
    }

    public static boolean connect(int inst) {
        return freerdp_connect(inst);
    }

    public static boolean disconnect(int inst) {
        return freerdp_disconnect(inst);
    }

    public static boolean cancelConnection(int inst) {
        return freerdp_disconnect(inst);
    }

    private static String addFlag(String name, boolean enabled) {
        if (enabled) {
            return "+" + name;
        }
        return "-" + name;
    }

    public static boolean setConnectionInfo(int inst, BookmarkBase bookmark) {
        BookmarkBase.ScreenSettings screenSettings = bookmark.getActiveScreenSettings();
        BookmarkBase.AdvancedSettings advanced = bookmark.getAdvancedSettings();
        BookmarkBase.DebugSettings debug = bookmark.getDebugSettings();

        String arg;
        ArrayList<String> args = new ArrayList<String>();

        args.add(TAG);
        args.add("/gdi:sw");

        String certName = "";
        if (bookmark.getType() != BookmarkBase.TYPE_MANUAL) {
            return false;
        }

        int port = bookmark.<ManualBookmark>get().getPort();
        String hostname = bookmark.<ManualBookmark>get().getHostname();

        args.add("/v:" + hostname);
        args.add("/port:" + String.valueOf(port));

        arg = bookmark.getUsername();
        if (!arg.isEmpty()) {
            args.add("/u:" + arg);
        }
        arg = bookmark.getDomain();
        if (!arg.isEmpty()) {
            args.add("/d:" + arg);
        }
        arg = bookmark.getPassword();
        if (!arg.isEmpty()) {
            args.add("/p:" + arg);
        }

        args.add(String.format("/size:%dx%d", screenSettings.getWidth(),
                screenSettings.getHeight()));
        args.add("/bpp:" + String.valueOf(screenSettings.getColors()));

        if (advanced.getConsoleMode()) {
            args.add("/admin");
        }

        switch (advanced.getSecurity()) {
            case 3: // NLA
                args.add("/sec-nla");
                break;
            case 2: // TLS
                args.add("/sec-tls");
                break;
            case 1: // RDP
                args.add("/sec-rdp");
                break;
            default:
                break;
        }

        if (!certName.isEmpty()) {
            args.add("/cert-name:" + certName);
        }

        BookmarkBase.PerformanceFlags flags = bookmark.getActivePerformanceFlags();
        if (flags.getRemoteFX()) {
            args.add("/rfx");
        }

        if (flags.getGfx()) {
            args.add("/gfx");
        }

        if (flags.getH264()) {
            args.add("/h264");
        }

        args.add(addFlag("wallpaper", flags.getWallpaper()));
        args.add(addFlag("window-drag", flags.getFullWindowDrag()));
        args.add(addFlag("menu-anims", flags.getMenuAnimations()));
        args.add(addFlag("themes", flags.getTheming()));
        args.add(addFlag("fonts", flags.getFontSmoothing()));
        args.add(addFlag("aero", flags.getDesktopComposition()));

        if (!advanced.getRemoteProgram().isEmpty()) {
            args.add("/app:" + advanced.getRemoteProgram());
        }

        if (!advanced.getWorkDir().isEmpty()) {
            args.add("/shell-dir:" + advanced.getWorkDir());
        }

        args.add(addFlag("async-channels", debug.getAsyncChannel()));
        args.add(addFlag("async-transport", debug.getAsyncTransport()));
        args.add(addFlag("async-input", debug.getAsyncInput()));
        args.add(addFlag("async-update", debug.getAsyncUpdate()));

        if (advanced.getRedirectSDCard()) {
            String path = android.os.Environment.getExternalStorageDirectory().getPath();
            args.add("/drive:sdcard," + path);
        }

        args.add("/clipboard");

        // Gateway enabled?
        if (bookmark.getType() == BookmarkBase.TYPE_MANUAL && bookmark.<ManualBookmark>get().getEnableGatewaySettings()) {
            ManualBookmark.GatewaySettings gateway = bookmark.<ManualBookmark>get().getGatewaySettings();

            args.add(String.format("/g:%s:%d", gateway.getHostname(), gateway.getPort()));

            arg = gateway.getUsername();
            if (!arg.isEmpty()) {
                args.add("/gu:" + arg);
            }
            arg = gateway.getDomain();
            if (!arg.isEmpty()) {
                args.add("/gd:" + arg);
            }
            arg = gateway.getPassword();
            if (!arg.isEmpty()) {
                args.add("/gp:" + arg);
            }
        }

        /* 0 ... disable
           1 ... local
           2 ... remote */
        args.add("/audio-mode:" + String.valueOf(advanced.getRedirectSound()));

        if (advanced.getRedirectMicrophone()) {
            args.add("/microphone");
        }

        args.add("/log-level:TRACE");
        String[] arrayArgs = args.toArray(new String[args.size()]);
        return freerdp_parse_arguments(inst, arrayArgs);
    }

    public static boolean updateGraphics(int inst, Bitmap bitmap, int x, int y, int width, int height) {
        return freerdp_update_graphics(inst, bitmap, x, y, width, height);
    }

    public static boolean sendCursorEvent(int inst, int x, int y, int flags) {
        return freerdp_send_cursor_event(inst, x, y, flags);
    }

    public static boolean sendKeyEvent(int inst, int keycode, boolean down) {
        return freerdp_send_key_event(inst, keycode, down);
    }

    public static boolean sendUnicodeKeyEvent(int inst, int keycode) {
        return freerdp_send_unicodekey_event(inst, keycode);
    }

    public static boolean sendClipboardData(int inst, String data) {
        return freerdp_send_clipboard_data(inst, data);
    }

    private static void OnConnectionSuccess(int inst) {
        if (listener != null)
            listener.OnConnectionSuccess(inst);
    }

    private static void OnConnectionFailure(int inst) {
        if (listener != null)
            listener.OnConnectionFailure(inst);
    }

    private static void OnPreConnect(int inst) {
        if (listener != null)
            listener.OnPreConnect(inst);
    }

    private static void OnDisconnecting(int inst) {
        if (listener != null)
            listener.OnDisconnecting(inst);
    }

    private static void OnDisconnected(int inst) {
        if (listener != null)
            listener.OnDisconnected(inst);
    }

    private static void OnSettingsChanged(int inst, int width, int height, int bpp) {
        SessionState s = GlobalApp.getSession(inst);
        if (s == null)
            return;
        UIEventListener uiEventListener = s.getUIEventListener();
        if (uiEventListener != null)
            uiEventListener.OnSettingsChanged(width, height, bpp);
    }

    private static boolean OnAuthenticate(int inst, StringBuilder username, StringBuilder domain, StringBuilder password) {
        SessionState s = GlobalApp.getSession(inst);
        if (s == null)
            return false;
        UIEventListener uiEventListener = s.getUIEventListener();
        if (uiEventListener != null)
            return uiEventListener.OnAuthenticate(username, domain, password);
        return false;
    }

    private static boolean OnVerifyCertificate(int inst, String subject, String issuer, String fingerprint) {
        SessionState s = GlobalApp.getSession(inst);
        if (s == null)
            return false;
        UIEventListener uiEventListener = s.getUIEventListener();
        if (uiEventListener != null)
            return uiEventListener.OnVerifiyCertificate(subject, issuer, fingerprint);
        return false;
    }

    private static void OnGraphicsUpdate(int inst, int x, int y, int width, int height) {
        SessionState s = GlobalApp.getSession(inst);
        if (s == null)
            return;
        UIEventListener uiEventListener = s.getUIEventListener();
        if (uiEventListener != null)
            uiEventListener.OnGraphicsUpdate(x, y, width, height);
    }

    private static void OnGraphicsResize(int inst, int width, int height, int bpp) {
        SessionState s = GlobalApp.getSession(inst);
        if (s == null)
            return;
        UIEventListener uiEventListener = s.getUIEventListener();
        if (uiEventListener != null)
            uiEventListener.OnGraphicsResize(width, height, bpp);
    }

    private static void OnRemoteClipboardChanged(int inst, String data) {
        SessionState s = GlobalApp.getSession(inst);
        if (s == null)
            return;
        UIEventListener uiEventListener = s.getUIEventListener();
        if (uiEventListener != null)
            uiEventListener.OnRemoteClipboardChanged(data);
    }

    public static String getVersion() {
        return freerdp_get_version();
    }
}
