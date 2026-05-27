/*
   Print Proxy Activity

   Copyright 2026 Ibrahim Sevinc

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;
import android.print.PrintJob;
import android.print.PrintManager;

import android.app.Activity;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Transparent activity that shows the Android print dialog for a PDF URI.
 * Stays alive until the print job reaches a terminal state, then finishes.
 */
public class PrintProxyActivity extends Activity
{
	private PrintJob printJob;
	private final Handler handler = new Handler(Looper.getMainLooper());
	private final Runnable pollJob = new Runnable() {
		@Override public void run()
		{
			if (printJob == null || printJob.isCompleted() || printJob.isFailed() ||
			    printJob.isCancelled())
			{
				finish();
				return;
			}
			handler.postDelayed(this, 500);
		}
	};

	@Override protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		Uri uri = getIntent().getData();
		if (uri == null)
		{
			finish();
			return;
		}

		PrintManager pm = (PrintManager)getSystemService(Context.PRINT_SERVICE);
		printJob = pm.print(uri.getLastPathSegment(), new PdfAdapter(this, uri), null);
		handler.postDelayed(pollJob, 500);
	}

	@Override protected void onDestroy()
	{
		handler.removeCallbacks(pollJob);
		super.onDestroy();
	}

	private static class PdfAdapter extends PrintDocumentAdapter
	{
		private final Context ctx;
		private final Uri uri;

		PdfAdapter(Context ctx, Uri uri)
		{
			this.ctx = ctx;
			this.uri = uri;
		}

		@Override
		public void onLayout(PrintAttributes oldAttr, PrintAttributes newAttr,
		                     android.os.CancellationSignal signal, LayoutResultCallback cb,
		                     Bundle extras)
		{
			if (signal.isCanceled())
			{
				cb.onLayoutCancelled();
				return;
			}
			PrintDocumentInfo info = new PrintDocumentInfo.Builder(uri.getLastPathSegment())
			                             .setContentType(PrintDocumentInfo.CONTENT_TYPE_DOCUMENT)
			                             .setPageCount(PrintDocumentInfo.PAGE_COUNT_UNKNOWN)
			                             .build();
			cb.onLayoutFinished(info, !newAttr.equals(oldAttr));
		}

		@Override
		public void onWrite(PageRange[] pages, ParcelFileDescriptor dest,
		                    android.os.CancellationSignal signal, WriteResultCallback cb)
		{
			try (InputStream in = ctx.getContentResolver().openInputStream(uri);
			     FileOutputStream out = new FileOutputStream(dest.getFileDescriptor()))
			{
				if (in == null)
				{
					cb.onWriteFailed("Cannot open PDF");
					return;
				}
				byte[] buf = new byte[8192];
				int n;
				while ((n = in.read(buf)) != -1)
				{
					if (signal.isCanceled())
					{
						cb.onWriteCancelled();
						return;
					}
					out.write(buf, 0, n);
				}
				cb.onWriteFinished(new PageRange[] { PageRange.ALL_PAGES });
			}
			catch (IOException e)
			{
				cb.onWriteFailed(e.getMessage());
			}
		}
	}
}
