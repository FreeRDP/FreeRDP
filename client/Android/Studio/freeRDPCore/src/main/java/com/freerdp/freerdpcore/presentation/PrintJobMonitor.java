/*
   Print Job Monitor

   Copyright 2026 Ibrahim Sevinc

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.os.FileObserver;

import androidx.annotation.Nullable;

import java.io.File;

/**
 * Watches /sdcard/Download for new rdp_print_*.pdf files written by the printer backend.
 * Fires onPrintJobComplete when the file is fully written (CLOSE_WRITE event).
 */
public class PrintJobMonitor extends FileObserver
{
	public interface Listener
	{
		void onPrintJobComplete(File pdfFile);
	}

	private static final String WATCH_DIR = "/sdcard/Download";
	private static final String PREFIX = "rdp_print_";
	private static final String SUFFIX = ".pdf";

	private final Listener listener;

	public PrintJobMonitor(Listener listener)
	{
		super(new File(WATCH_DIR), CLOSE_WRITE);
		this.listener = listener;
	}

	@Override public void onEvent(int event, @Nullable String path)
	{
		if (path == null)
			return;
		if (!path.startsWith(PREFIX) || !path.endsWith(SUFFIX))
			return;
		listener.onPrintJobComplete(new File(WATCH_DIR, path));
	}
}
